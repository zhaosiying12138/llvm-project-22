# RVV 寄存器压力感知调度：从长活跃区间到就近消费

这篇文章记录的是一个默认关闭的 RISC-V RVV 后端实验：在 LLVM 22.1.3
分支上，为一类 fully unrolled、宽向量、pre-RA 阶段容易溢出的 kernel
加入寄存器压力感知调度。它不是一个通用承诺，也不是要改变 LLVM 的默认
RISC-V 代码生成策略；目标更窄：在用户显式传入隐藏选项时，让 RVV 宽虚拟
寄存器更早被消费，减少调度阶段制造出来的长活跃区间，从而降低后续 register
allocator 看到的 whole-register spill/reload。

这类问题在 `<128 x float>` 这样的固定长度 IR 向量上很容易被放大。给定
`-riscv-v-vector-bits-min=1024`，`<128 x float>` 会自然落到 RVV LMUL=4
一类宽寄存器形态；如果 kernel 被完全展开，IR 中会出现一串 load、一串
elementwise 运算、一串 store，或者先 reduce 再对原向量继续做 normalize、
select、store。普通 pre-RA MachineScheduler 在没有目标特定压力偏置时，
会倾向保留 load clustering 和一些局部顺序特性。这个选择对很多标量和较窄
向量代码是合理的，但在 RVV wide vreg 上，连续打开十几个新的虚拟向量值，
就等于把十几个 LMUL=4 的值同时交给 register allocator。物理向量寄存器
只有 32 个 architectural register，LMUL=4 的值一次占 4 个寄存器组，压力
很快就超过可分配空间，最终汇编里会出现 `vs4r.v`、`vl4r.v` 这样的整组
spill/reload。

实验的核心思路很朴素：当一个调度区域已经显著呈现 RVV 高压特征时，不要再
优先打开新的 RVV load live range，而是尽量把已经 load 出来的向量拿去做
consumer 或 store。换句话说，从“先把输入都搬进来”改成“load 几个，马上
消费几个”。对 elementwise add 这类工作负载，单靠这种就近消费就能把宽值
活跃区间压短到 allocator 能处理的范围；对 softmax、top2、RMSNorm 这类
reduce 后还要复用原输入向量的 workload，还需要一个更保守的 reload clone
pass，把少数安全的 load 在 late elementwise use 前重新物化，避免一个宽
输入值跨过 reduce 链一直活到很后面。

## 使用接口

这个实验暴露三个显式接口，全部默认关闭：

```sh
-mllvm -riscv-v-reg-pressure-aware-sched
-mllvm -riscv-v-reg-pressure-report
-mattr=+experimental-yushuxin-vfexp
```

`-mllvm -riscv-v-reg-pressure-aware-sched` 打开调度和 reload clone 两部分
逻辑。不开这个选项时，scheduler 不启用 RVV pressure-aware region，reload
pass 也不插入 pipeline，load/store clustering 仍按原来的 RISC-V 路径工作。

`-mllvm -riscv-v-reg-pressure-report` 打开 frame lowering 阶段的诊断输出。
它不会改变代码生成，只会在 stderr 打印每个函数的 RVV scalable stack
占用、RVV spill slot 数量和一个 fixed stack size 估计值。这个选项适合跟
spill/reload grep 一起用来分析趋势。

`+experimental-yushuxin-vfexp` 是为了 softmax workload 加入的实验特性。
它允许 SelectionDAG 把 vector `llvm.exp.*` lowering 成一条实验性的
`yushuxin.vfexp` RVV 指令。这个 feature 同样默认关闭；没有它，普通
`llvm.exp.v128f32` 不会选择 `yushuxin.vfexp`。这里的指令 encoding 是
reserved OP-V unary placeholder，目的是给调度实验提供一个可见的 vector
exp 节点，不代表最终 ISA 编码。

典型复现命令如下：

```sh
build-riscv/bin/llc -O2 -mtriple=riscv64 \
  -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 \
  input.ll -o output.s

build-riscv/bin/llc -O2 -mtriple=riscv64 \
  -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 \
  -riscv-v-reg-pressure-aware-sched \
  input.ll -o output.opt.s
```

softmax 因为包含 `llvm.exp.v128f32`，需要把 `-mattr` 改成：

```sh
-mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b
```

MachineScheduler 观察可以使用现有 debug channel：

```sh
build-riscv/bin/llc -O2 -mtriple=riscv64 \
  -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 \
  -debug-only=machine-scheduler \
  input.ll -o output.s
```

whole-register RVV spill/reload 的快速计数使用：

```sh
rg -o '\b[vu][sl][1248]r\.v\b' output.s | wc -l
```

这个正则只数 `vs1r.v`、`vs2r.v`、`vs4r.v`、`vs8r.v` 以及对应 reload，
不会把普通 `vse32.v`、`vle32.v` 数据访存算进去。

## 为什么普通调度会放大 RVV 压力

LLVM pre-RA MachineScheduler 的输入还是虚拟寄存器 SSA 形式。调度器此时
看不到最终物理寄存器分配结果，但它会决定一个 basic block 内各个 MachineInstr
的相对顺序。这个顺序直接决定 live interval：一个 def 被排得越早、最后一个
use 被排得越晚，它的 live range 越长；多个宽值 live range 重叠得越多，后端
越容易溢出。

对 RVV 来说，单个 virtual vector register 的“宽度”不是一个抽象细节。
`<128 x float>` 在 VLEN=1024 下可以用 LMUL=4 表示；一个值占用一组向量
寄存器。fully unrolled add workload 有 32 组输入 `a[i]`、32 组输入
`b[i]`、32 组输出 `sum[i]`。如果调度顺序接近 IR 的形状，就会先连续产生
64 个输入向量 live range，然后才开始 `vfadd`。即使很多值只被使用一次，
它们也已经在调度上变成了长活跃值。

generic load clustering 本来是为 locality、address generation、后续 peephole
等目标服务的。把相邻 load 聚在一起通常是好事；但在这个场景里，load clustering
把“打开新 live range”的动作进一步集中。对宽 RVV 值而言，一串 `vle32.v`
之后不马上消费，就会让 bottom boundary 的寄存器压力接近甚至达到 VR=32
这类极限。allocator 只有两个选择：要么重新排列已经固定的 schedule，这不是
它的职责；要么把部分向量值 spill 到 scalable vector stack slot。

baseline 汇编里最明显的信号就是 folded whole-register spill：

```asm
vfadd.vv v4, v4, v0
csrr a7, vlenb
slli a7, a7, 4
mv t0, a7
slli a7, a7, 2
add a7, a7, t0
add a7, sp, a7
addi a7, a7, 80
vs4r.v v4, (a7)                        # vscale x 32-byte Folded Spill
```

在 MachineScheduler 的现有日志里，也能看到压力驱动的选择，例如：

```text
Pick Bot REG-EXCESS
Bottom Pressure: VR=32
```

这里需要强调一点：这个实验没有新增专门的 scheduler debug 文案。可观察变化
来自 MachineScheduler 已有日志中的 pick 顺序、pressure 数值和 final
schedule。也就是说，打开实验选项后，日志里不会出现一个新的“RVV pressure
heuristic fired”字符串；你看到的是同一个 debug channel 下，候选节点选择
顺序和最终指令序列发生变化。

## `yushuxin.vfexp` 的实现边界

softmax workload 需要一个 vector exponential。为了避免把问题混到 libcall
展开或标量化路径里，分支加入了一个实验性的 Yushuxin RVV exp 指令：
`yushuxin.vfexp`。它的作用是让 `llvm.exp.v128f32` 在 feature 开启时成为
一条真实的 RVV unary FP 指令，从而跟 `vfadd`、`vfred*`、`vse` 一样进入
调度和寄存器分配。

TableGen 层面，feature 定义为 `FeatureExperimentalYushuxinVfexp`，命令行
名字是 `experimental-yushuxin-vfexp`，并且依赖 `Zve32f`。汇编谓词要求
显式带上这个 feature；`llvm/test/MC/RISCV/rvv/yushuxin-vfexp.s` 覆盖了
有 feature 时 assemble/disassemble 成功、无 feature 时报错的行为。

真实指令在 `RISCVInstrInfoV.td` 中复用 RVV unary floating-point 结构：

```tablegen
defm YUSHUXIN_VFEXP_V :
  VSQR_FV_VS2<"yushuxin.vfexp", 0b010011, 0b01000>;
```

`0b010011` 来自同一类 unary OP-V floating-point 形态，`0b01000` 是此实验
暂用的 reserved slot。代码里明确把它标成 non-final placeholder encoding。
伪指令在 `RISCVInstrInfoVPseudos.td` 中定义为 `PseudoYUSHUXIN_VFEXP`，
同样带 `HasExperimentalYushuxinVfexp` predicate，并标记可能 raise FP
exception、使用 `FRM`、`VL`、`VTYPE`。

SelectionDAG lowering 只处理受支持的 RVV floating vector type。`RISCVISelLowering`
里，原本需要 libcall 的 `ISD::FEXP` 对 vector 类型默认仍然 Expand；只有
当 subtarget 有 `hasExperimentalYushuxinVfexp()` 时，才把对应 VT 的
`ISD::FEXP` action 设为 Legal。随后 VSD/VVL pattern 把 `(fexp vector)`
或 `riscv_fexp_vl` 匹配到 `PseudoYUSHUXIN_VFEXP_*`。测试
`llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll` 检查默认路径不出现
`yushuxin.vfexp`，feature 开启后出现 `vle32.v`、`yushuxin.vfexp`、
`vse32.v`。

bf16 是一个刻意保守的例外。TableGen pattern 只为非 bf16 FP vector 生成
`PseudoYUSHUXIN_VFEXP_*` 匹配，因此 lowering 不能把 bf16 vector `FEXP`
直接标成 Legal。对 `+zvfbfmin` 或 `+experimental-zvfbfa`，bf16 exp 仍走
bf16 到 f32 的 promote/custom split 路径；如果同时开启
`+experimental-yushuxin-vfexp`，被提升后的 f32 `FEXP` 可以再选择
`yushuxin.vfexp`。`yushuxin-vfexp-bf16.ll` 覆盖了这个边界，避免 scalable
bf16 exp 掉进没有 bf16 pattern 的 instruction selection 路径。

这里没有覆盖 strict FP exp、GlobalISel `G_FEXP` 选择，也没有试图为所有
element type 构建近似数学语义。它是一个受 feature gate 保护的 CodeGen
实验入口，服务于 RVV pressure 调度实验。

## Scheduler：识别高压区域并改变偏好

调度器入口在 `RISCVMachineScheduler`。RISC-V target machine 的
`createMachineScheduler` 创建 `ScheduleDAGMILive`，策略类是
`RISCVPreRAMachineSchedStrategy`。这本来已经承载了 RISC-V 的 vsetvli
调度启发式；本实验在同一个策略上加一层 RVV register pressure 逻辑。

第一步是判断当前 scheduling region 是否值得介入。代码扫描 `DAG.SUnits`，
统计两件事：有 RVV operand 的指令数量 `RVVOps`，以及定义 RVV 虚拟寄存器的
指令集合 `RVVDefs`。判断条件是：

```c++
RVVOps >= 12 && RVVDefs.size() >= 8
```

这个阈值不是硬件事实，而是实验性的保守门槛。它的目的不是捕获所有 RVV
代码，而是过滤掉小函数、小向量、只有一两个 RVV 临时值的普通 region。只有
同时满足“区域里有足够多 RVV 操作”和“确实会打开多个 RVV def”的 block，
才会被视为 pressure-aware region。

打开 `-riscv-v-reg-pressure-aware-sched` 后，`initPolicy` 会把
`RegionPolicy.ShouldTrackPressure` 设为 true，让 MachineScheduler 计算
候选节点的 register pressure delta。`initialize` 中如果当前 region 命中
高压条件，则把 `OnlyTopDown` 设为 true，并关闭 `OnlyBottomUp`。这个选择
很关键：bottom-up 调度容易从 block 末端向前选择 store 或 late consumer，
在当前实验里会让“已经很多宽值活着”的局面更难解释；top-down 顺序更接近
“load 后马上安排可用 consumer”的目标，也更容易让 vsetvli dataflow 保持
直观。

第二步是在候选比较里加入一个很窄的偏置。调度器定义了两个概念：

```c++
isRVVLoad(MI) = MI.mayLoad() && !MI.mayStore() && hasRVVRegDef(MI)

isRVVConsumerOrStore(MI) =
  !isRVVLoad(MI) &&
  hasRVVRegUse(MI) &&
  (hasRVVRegDef(MI) || MI.mayStore())
```

前者代表“继续打开一个新的 RVV live range”，典型例子是 `PseudoVLE32`。
后者代表“消费当前已经存在的 RVV value”，包括 elementwise op 和 vector
store。候选在同一 boundary 可比较时，如果一个候选是 consumer/store，而
另一个候选是 load，pressure-aware region 会优先 consumer/store。这样，
`vle32.v` 之后更容易接上 `vfadd.vv`，`vfadd.vv` 之后更容易接上 `vse32.v`，
而不是继续调度下一批 `vle32.v`。

第三步是处理 generic load/store clustering。`RISCVTargetMachine` 创建
pre-RA scheduler 时，原来会根据 subtarget tuning 添加 load cluster 和
store cluster mutation。实验选项开启后，这两个 mutation 被包装成
`RISCVVRegPressureClusterMutation`。包装器仍保留原 mutation，但在 apply
时先检查当前 DAG 是否高 RVV 压力；如果是，就跳过 generic clustering；如果
不是，就照常 apply。这样做避免了在高压区域继续人为拉长 load run，同时不
影响普通 region 的 clustering 策略。

向量 mask mutation 被保留。也就是说，`createRISCVVectorMaskDAGMutation`
仍然按原条件添加；masked workload 的测试检查 `v0.t` 仍能正确出现。实验只
改变 load/store clustering 和候选选择偏好，不把 mask 相关合法性揉进压力
逻辑里。

优化后 vector add 的 final schedule 和汇编会变成更短的 producer-consumer
片段，例如：

```asm
vle32.v v8, (a0)
vle32.v v12, (t4)
vle32.v v16, (a3)
vle32.v v20, (a6)
vfadd.vv v8, v8, v12
vse32.v v8, (a2)
```

这不是说每个 load 后都必须紧跟唯一 consumer。调度器仍然受依赖、资源、
vsetvli 兼容性、node order 等规则约束。实验只是在高压 RVV region 里改变
“同样合法时优先什么”的答案：优先缩短已打开宽值的 live range。

## Reload clone：为 reduce 后 late use 降低活跃跨度

elementwise add 的形状比较友好：两个输入 load 后做 `vfadd`，结果 store，
输入值生命周期结束。reduce 类 workload 更难。以 softmax 为例，输入向量
先参与 `vfredmax` 或 `vfredosum` 得到标量 max/sum，随后还要用原输入向量
做 subtract、exp、normalize、store。即使 scheduler 尽量就近消费，reduce
链本身也会迫使某些宽值跨过一段较长的依赖链。

本分支因此加了 `RISCVVRegPressureReload` pass。它的位置在 pre-RA
MachineScheduler 之后、register allocation 之前，并且只在
`-riscv-v-reg-pressure-aware-sched` 开启时通过 `insertPass(&MachineSchedulerID,
...)` 插入。它仍然运行在 pre-RA virtual-register 形式下，但不会再让后续 pre-RA scheduler
把刚插入的 cloned load 拉回到 block 前面。`RISCVVLOptimizer` 仍在更早位置
先稳定 RVV VL 相关伪指令和 operands；reload clone 则在最终 pre-RA schedule
确定后，把重新物化的 load 放在 late use 附近。

这个 pass 的名字里有 reload，但它不是在 register allocation 后修补真实
reload。它做的是 pre-RA virtual-register 形式下的 conservative rematerialization：
只有同一 basic block 命中和 scheduler 一致的高压阈值，即 `RVVOps >= 12 &&
RVVDefs.size() >= 8`，pass 才会考虑 clone。对这个高压 block 内的 simple
RVV load，如果它先被 reduction 使用，之后又被 elementwise 或 store 使用，
pass 可以在 late use 前 clone 一份 load，并把 late use 的 operand 改写到
新虚拟寄存器。这样原始 load 的 def 不必从 block 前部一直活到 late use；
late use 获得一个更近的 def。

simple RVV load 的定义很严格：

- 指令 `mayLoad()`，不能 `mayStore()`；
- 不能有 unmodeled side effects；
- 必须有 memory operand，且 memory operand 不能 volatile、atomic，也必须
  是 unordered；
- 必须只定义一个 RVV virtual register；
- 不能同时使用 RVV register；
- 不能是 fault-first load，例如 `PseudoVLE*FF_V*`。

reduction use 的识别也保持直接：通过 `TII->getName(opcode)`，名字包含
`VRED`、`VFRED` 或 `VWRED` 即视为 reduce。late elementwise use 则要求该
指令使用 RVV register，不是 reduction，并且自身要么定义 RVV register，要么
是 store。也就是说，clone 目标是“reduce 之后还会继续消费这个宽向量”的场景。

安全边界比盈利边界更重要。pass 只看 same-block；跨 basic block 直接视为
有 barrier。原 load 到 late use 之间如果出现 store、call、unmodeled side
effects，或者 volatile/atomic/ordered memory，都会跳过 clone。这个策略
没有做复杂 alias analysis，也不尝试证明 noalias 参数关系；遇到可能改变
内存可见值的情况就保守放弃。原 load 的 scalar/physical 输入如果在 late use
之前被重新定义或 clobber，也会跳过；真正 clone 时还会清理被延长 live range
的 kill flags，避免生成 use-after-kill MIR。测试覆盖了高压 MIR 正例以及 skip
case：低压 reduce-plus-late-use 不 clone，volatile/atomic/ordered memory、
可能别名的 intervening store、call intervened、fault-first load、输入寄存器
被重定义等情况也都不会 clone。

clone 本身也保持机械：创建同 register class 的新虚拟寄存器，复制 load
MachineInstr，把 clone 的 def 改成新寄存器，插到 late use 前，然后只改写
这个 late use 中使用旧寄存器的 operands。如果没有 operand 被改写，clone
会被删除。pass 不会删除原 load，也不会合并多个 late use；它只是给每个安全
late use 一个更近的 producer。

这个设计解释了为什么 reduce workload 的结果不是 0 spill。softmax、top2、
RMSNorm 都有不可消除的 reduce/normalize 依赖链，某些宽向量和标量结果必须
同时活着。reload clone 可以把“原输入跨过 reduce 后再用于 elementwise”的
一部分压力拆开，但不能改变算法依赖，也不能在不安全 memory 情况下重新 load。

## Frame diagnostics：看 scalable stack 和 spill slot

仅 grep 汇编里的 `vs4r.v`/`vl4r.v` 可以快速看到 whole-register spill/reload
数量，但不够解释 stack 侧的变化。因此分支在 `RISCVFrameLowering` 加了
隐藏诊断选项 `-riscv-v-reg-pressure-report`。输出形式类似：

```text
riscv-v-reg-pressure-report: function=vector_add_32 rvv-scalable-stack-bytes=768 rvv-spill-slots=24 fixed-stack-estimate=160
```

三个字段含义不同：

`rvv-scalable-stack-bytes` 是 RISC-V 后端分配 RVV scalable vector stack
object offset 后记录的 RVV stack size。它描述 scalable vector stack 区域
的字节量；实际运行时还跟 `vscale` 有关，但这个数字足以比较同一 VLEN 设置
下的 before/after。

`rvv-spill-slots` 只统计 `MachineFrameInfo` 中满足三个条件的对象：不是 dead
object、是 spill slot object、stack id 是 `TargetStackID::ScalableVector`。
这意味着它关注 register allocator 为 RVV spill 创建的 scalable vector
spill slot，而不是所有 scalable vector frame object。

`fixed-stack-estimate` 来自 `MFI.estimateStackSize(MF)`，反映普通 fixed
stack 的估计。优化后有时 fixed estimate 会变大，例如若为了减少 RVV spill
而保留更多地址计算或其他普通栈安排，这个字段可以帮助确认变化不是只在 RVV
侧发生。

一个容易误读的 sanity case 是 scalable alloca：

```llvm
define void @rvv_alloca() {
  %x = alloca <vscale x 4 x i32>, align 16
  ret void
}
```

它会贡献 scalable vector stack bytes，但不是 spill slot，所以
`rvv-spill-slots=0`。这正是诊断想表达的区别：RVV scalable stack object
不等于 RVV spill slot。

## Debug log 和汇编上能看到什么

打开 `-debug-only=machine-scheduler` 后，不要寻找新的自定义文本。这个实验
改变的是候选选择和最终 schedule，而不是 debug 打印格式。baseline 中常见
的是调度器在 bottom boundary 为了压力选择节点，日志能看到类似：

```text
Pick Bot REG-EXCESS
Bottom Pressure: VR=32
```

对应汇编里，宽向量 live range 已经长到需要 whole-register spill：

```asm
vfadd.vv v4, v4, v0
...
vs4r.v v4, (a7)                        # vscale x 32-byte Folded Spill
...
vl4r.v v4, (a7)                        # vscale x 32-byte Folded Reload
```

打开 `-riscv-v-reg-pressure-aware-sched` 后，MachineScheduler 仍然在同一个
debug channel 中打印 pick 和 final schedule，但高压 region 倾向 top-down，
并且同 boundary 下更愿意选择 RVV consumer/store 而不是继续 load。最终汇编
最容易识别的变化是片段化的 load-consume-store：

```asm
vle32.v v8, (a0)
vle32.v v12, (t4)
vfadd.vv v8, v8, v12
vse32.v v8, (a2)
```

在 vector add workload 上，这个变化足以消除 `vs{1,2,4,8}r.v` 和
`vl{1,2,4,8}r.v`。在 reduce workload 上，汇编里仍会有少量 whole-register
spill/reload，但数量和 RVV stack slot 明显下降。若同时观察 MIR，可在
`-stop-after=riscv-v-reg-pressure-reload` 的输出中看到安全 late use 前多出
一条 cloned `PseudoVLE32_V_M4`。

## 四个 workload 的结果

测量使用固定 `VLEN=1024`，workload 来自
`llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll`。每个
函数被单独抽出，分别跑 baseline 和 optimized，`-O2`、`-O3` 都保留。softmax
额外启用 `+experimental-yushuxin-vfexp`。whole-register spill/reload 用
前面的 `rg` 正则计数，RVV stack/slot 用 `-riscv-v-reg-pressure-report`
采集。

| Workload | Opt | Baseline whole-reg spills/reloads | Optimized whole-reg spills/reloads | Baseline RVV stack / slots | Optimized RVV stack / slots |
|---|---:|---:|---:|---:|---:|
| 32 x `<128 x float>` add | O2 | 50 | 0 | 768 / 24 | 0 / 0 |
| 32 x `<128 x float>` add | O3 | 50 | 0 | 768 / 24 | 0 / 0 |
| safe-softmax reduce + exp | O2 | 86 | 17 | 816 / 33 | 96 / 3 |
| safe-softmax reduce + exp | O3 | 86 | 17 | 816 / 33 | 96 / 3 |
| top-2 compare/select/reduce | O2 | 93 | 8 | 872 / 34 | 64 / 2 |
| top-2 compare/select/reduce | O3 | 93 | 8 | 872 / 34 | 64 / 2 |
| RMSNorm square/sum/sqrt/normalize | O2 | 268 | 37 | 2208 / 69 | 128 / 4 |
| RMSNorm square/sum/sqrt/normalize | O3 | 268 | 37 | 2208 / 69 | 128 / 4 |

vector add 是最纯粹的 elementwise case。baseline 的 IR 形状是 32 组 a/b
load，随后 32 组 `fadd`，再 32 组 store。generic scheduling 和 clustering
会把很多 RVV load 排在 consumer 前，导致 50 次 whole-register spill/reload，
RVV stack 为 768 bytes、24 个 spill slots。pressure-aware scheduling 打开后，
load、`vfadd`、store 被拉近，输入值的 live range 大幅缩短，O2/O3 都降到
0 次 whole-register spill/reload，RVV stack/slots 也变成 0/0。

safe-softmax 包含 reduce 和 exp。它通常先对输入做 max 或 sum reduction，
再对向量做 subtract、`yushuxin.vfexp`、normalize、store。这里 scheduler
负责避免继续打开不必要的新 RVV live range，reload clone 负责处理 reduce
之后的 late elementwise use。结果从 86 次降到 17 次，RVV stack/slots 从
816/33 降到 96/3。剩余 spill 主要来自 reduction 和 normalize 之间仍需共存
的宽值与标量链，不能靠简单重排完全消除。

top2 workload 的形状是 compare/select/reduce 混合。它要保留候选 top 值，
做向量比较和选择，再通过 `vfredmax.vs` 一类 reduction 得到标量结果。baseline
有 93 次 whole-register spill/reload、872/34 的 RVV stack/slots。优化后降到
8 次和 64/2。这个 case 说明 consumer-first 调度不只服务于 `vfadd`，对
`vmf*`、`vmerge`、`vfred*` 周围的宽值生命周期也有帮助；但 reduce 输出和
后续 select 链仍然会留下少量压力峰值。

RMSNorm 是四个 workload 中压力最高的一个。它包括 square、sum reduction、
sqrt 或 reciprocal sqrt、normalize、store，多阶段依赖让输入、平方结果、
归一化中间值都可能跨较长区间。baseline 达到 268 次 whole-register
spill/reload，RVV stack/slots 是 2208/69。打开实验后降到 37 次和 128/4。
这个下降主要来自两部分：调度器缩短 elementwise 段的 live range，reload
clone 避免原输入向量无谓跨过 reduce 后的 late normalize use。剩余 37 次
说明这不是一个“消灭所有 spill”的算法重写；它只是把由调度顺序造成的额外
压力去掉了大部分。

值得注意的是，四个 workload 的 O2/O3 结果相同。这不意味着优化级别永远
无关，而是这些 synthetic fully unrolled IR 在本次后端 pipeline 中形成了
相同的关键机器级压力形态。报告保留 O2/O3，是为了确认这个实验不是只在某个
优化级别偶然生效。

## 从一个调度区域看生命周期

理解这个补丁时，最好把注意力放在一个 basic block 内的生命周期，而不是只看
最终 spill 数字。假设 IR 里有四组展开的 add：

```llvm
%a0 = load <128 x float>, ptr %ap0
%b0 = load <128 x float>, ptr %bp0
%a1 = load <128 x float>, ptr %ap1
%b1 = load <128 x float>, ptr %bp1
%s0 = fadd <128 x float> %a0, %b0
%s1 = fadd <128 x float> %a1, %b1
store <128 x float> %s0, ptr %out0
store <128 x float> %s1, ptr %out1
```

如果 machine schedule 延续“先 load，再算，再 store”的形状，`%a0` 的 live
range 从第一条 load 开始，一直持续到 `%s0` 的 `vfadd`。在四组展开里这还不
严重；在三十二组展开里，`%a0` 要跨过后面六十多条 load 和地址计算。`%b0`、
`%a1`、`%b1` 也类似，只是起点略晚。这样一来，调度器并没有违反依赖，却把
allocator 要面对的最大同时活跃宽值数量推高了。

pressure-aware 调度想要的形状不是任意乱排，而是让每组输入形成短链：

```text
load a0
load b0
add s0
store s0
load a1
load b1
add s1
store s1
```

真实 schedule 不会这么整齐，因为地址计算、`vsetvli` 兼容性、资源模型、
弱边和后续 store 都会参与排序。但只要大方向从“打开更多 load”改成“消费已经
打开的值”，最大 live overlap 就会下降。这个下降对 LMUL=4 特别明显：减少
一个同时活跃值，常常等价于释放四个 architectural vector register 的压力。
当调度区域里有十几个这样的值时，是否早消费几个宽值，会直接决定 allocator
是留在物理寄存器内，还是开始生成 scalable spill slot。

这也是为什么本文反复强调 pre-RA。到了 register allocation 之后，真实
spill/reload 已经插入，调度器只能在一个更受限制的世界里做 post-RA 调整。
pre-RA 阶段虽然没有物理寄存器分配结果，但它有改变 live range 形状的机会。
RVV 宽寄存器的压力问题，很多时候不是 allocator 单独造成的；allocator 只是
把前面调度顺序造成的重叠用 spill 表达出来。

## 阈值、偏置和误伤控制

`RVVOps >= 12 && RVVDefs.size() >= 8` 看起来像两个魔数，但它们承担的是误伤
控制，而不是精确建模。小 region 里即使存在 RVV load 和 consumer，强行关闭
clustering 或改变 top/bottom 策略也未必有收益；相反，可能破坏原本有利于
访存局部性或后续 peephole 的顺序。阈值要求 region 至少已经有一定规模，并且
确实会产生多个 RVV def，才进入 pressure-aware 模式。

这里没有按 LMUL 精确计分，是一个有意的折中。更细的模型可以读取 register
class，估算 LMUL，对不同 SEW/LMUL 组合给出不同权重；但那会把实验从“局部
调度偏置”推进到“目标相关压力模型重写”。当前 workload 都是 `<128 x float>`
在 VLEN=1024 下形成的宽值，简单计数已经足以区分高压区域和普通区域。对于
后续要推广的版本，LMUL-aware 计分会是自然方向，但它不应该成为这个实验正确
性的前提。

候选偏置也刻意放在 GenericScheduler 的一组既有比较之后。代码先处理物理
寄存器、excess pressure、critical pressure、cluster 等通用因素，再在同
boundary 场景下判断 RVV consumer/store 是否应优先于 RVV load。这样做的
含义是：如果某个候选因为依赖、资源或压力明显更差，它不会仅仅因为“看起来
像 consumer”就被强行选中。实验不是要绕开 MachineScheduler，而是把 RISC-V
RVV 的一个局部事实加入既有决策：在高压宽向量区域里，继续 load 往往比消费
已有宽值更危险。

top-down 偏好同样是局部启用。普通 RISC-V 调度仍可以使用原来的 top/bottom
策略；只有命中高压 RVV region 时才把 `OnlyTopDown` 打开。这个选择还有一个
工程上的好处：调度结果更容易和源 IR 的 producer-consumer 关系对应起来。
当我们阅读 `-debug-only=machine-scheduler` 日志时，top boundary 的候选变化
更直观，也更容易把某条 `vle32.v`、`vfadd.vv`、`vse32.v` 的相对位置和 live
range 缩短联系起来。

## Reload clone 的具体例子

reload clone pass 可以用一个极小的 reduce-plus-late-use 例子解释。输入向量
`%v` 先被 reduction 消费：

```llvm
%v = load <128 x float>, ptr %in
%r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
%x = fadd <128 x float> %v, %v
store <128 x float> %x, ptr %out
store float %r, ptr %sum
```

如果不 clone，`%v` 的机器层 def 必须从 load 活到 reduction，再继续活到
late `fadd`。reduction 本身通常会产生一段依赖链，并且可能引入标量搬运、
`vfmv.f.s` 或其他提取动作。调度器即使尽量安排 consumer，也无法让 late
`fadd` 提前到 reduction 之前，因为语义上它确实在后面使用同一个输入。于是
`%v` 成为跨越 reduce 区域的宽活跃值。

clone 后，MIR 形状接近：

```text
%v0 = PseudoVLE32_V_M4 ...
%r  = PseudoVFREDOSUM_VS_M4 ... %v0 ...
%v1 = PseudoVLE32_V_M4 ...
%x  = PseudoVFADD_VV_M4 ... %v1, %v1 ...
```

这多了一次真实数据 load，但换来的是更短的宽寄存器 live range。是否盈利
取决于 workload：如果原来的 live range 迫使 allocator 生成多次
whole-register spill/reload，那么一次普通 vector load 往往更便宜；如果
区域本来没有压力，clone 就没有必要。因此 pass 只在 scheduler flag 开启且
basic block 命中高压阈值时运行，而且只寻找“先 reduce、后 late elementwise/store”
的形状。它不是一个通用 rematerialization pass。

安全判断是这段逻辑的底线。volatile load 不能复制，因为每次 load 都是可观察
事件；atomic 或 ordered memory 不能复制，因为内存顺序语义会改变；中间有
store 时，即使源 IR 看起来可能 noalias，当前机器 pass 也不做充分证明，因此
按 barrier 处理；中间有 call 或 unmodeled side effect 同理。跨块场景还会
涉及控制流路径、支配关系和 memory 可见性，当前实现全部跳过。这些 skip 会
损失一些机会，但它们让 pass 的正确性更容易审查。

## 结果背后的成本权衡

表格中的数字只统计 whole-register spill/reload，因此它衡量的是一个很具体的
成本：allocator 被迫把整个 RVV register group 写到 scalable stack，再读回来。
对于 LMUL=4 的值，这类 spill 不只是多一条普通 store。它还需要按 `vlenb`
计算 scalable offset，使用 `vs4r.v`/`vl4r.v` 访问整组寄存器，并占用 RVV
scalable spill slot。baseline 汇编里经常能看到 spill 前后夹着一串 `csrr
vlenb`、`slli`、`add sp` 之类地址计算，这些也会增加普通寄存器和指令数压力。

优化后 fixed stack estimate 有时反而变大，这不矛盾。RVV spill slot 减少
描述的是 scalable vector stack 区域下降；fixed estimate 可能因为不同的
地址计算、临时标量或 frame layout 估计而变化。真正要看 RVV pressure 目标
是否达成，应同时观察 `rvv-scalable-stack-bytes`、`rvv-spill-slots` 和
whole-register spill/reload 计数。vector add 从 768/24 到 0/0，说明宽向量
spill 被完全避免；softmax/top2/RMSNorm 保留少量 slot，说明算法形状仍有
不可完全规避的高压点。

另一个权衡是 load 数量。reload clone 可能增加普通 `vle32.v`，这在没有压力
时未必划算。但在 reduce workload 中，baseline 的代价不是一次 late load，
而是多个 whole-register spill/reload 加 scalable stack slot。实验选项通过
默认关闭和形状识别，把这个权衡限制在用户明确要分析 RVV pressure 的场景里。
如果未来要默认启用，就必须有更完整的 profitability model，例如估算 clone
load 与预期 spill 节省之间的关系，并考虑 cache、带宽和目标微架构。

## 与现有 RISC-V 后端逻辑的关系

这个实验没有绕开 RISC-V 后端已有的 vsetvli 处理。`RISCVPreRAMachineSchedStrategy`
本来就维护 `TopInfo` 和 `BottomInfo`，用 `RISCVVSETVLIInfoAnalysis` 比较候选
指令的 VTYPE/VL 兼容性，尽量减少不必要的 vsetvli 变化。新增 RVV pressure
偏置和这个启发式共存在同一个 `tryCandidate` 流程里，并且排在已有 vsetvli
heuristic 之后；当 VTYPE/VL 兼容性能够区分候选时，RISC-V 特定的 vsetvli
决策先保留住，pressure tie-breaker 只在它没有选出更好候选时再介入。

load/store clustering 的处理也不是全局删除。包装器只是在高压 DAG 上跳过
cluster mutation；非高压区域仍然使用原来的 `createLoadClusterDAGMutation`
和 `createStoreClusterDAGMutation`。这点很重要，因为 clustering 对很多代码
仍然是合理优化。一个小的 RVV copy loop、一个只有几条 vector load/store 的
block，不应该因为这个实验而失去原本的调度行为。

frame diagnostic 也保持只读。它复用 frame lowering 已经计算出的 RVV stack
object 信息，不改变 frame layout，不插入额外对象，也不参与 spill 决策。
这让它适合用于 lit 测试和手工报告：同一条 llc 命令只要追加
`-riscv-v-reg-pressure-report`，就能得到可比较的诊断行，而不会污染汇编输出。

## 复现时应该按什么顺序看

手工复现时，我建议先看汇编，再看 frame 诊断，最后看 scheduler debug。汇编
最直接：先用 baseline 命令生成 `output.s`，跑
`rg -o '\b[vu][sl][1248]r\.v\b' output.s | wc -l`。如果这个数字已经是 0，
说明该输入并没有触发本文讨论的 whole-register spill 问题，继续看调度日志
意义不大。若能看到 `vs4r.v`、`vl4r.v`，再打开实验选项生成 optimized 汇编，
比较同一个函数里这些指令是否减少，以及普通 `vle32.v`、`vfadd.vv`、
`vse32.v` 的相对位置是否更接近。

第二步看 `-riscv-v-reg-pressure-report`。这个诊断有时比 grep 更能说明趋势。
例如某个函数 whole-register spill/reload 数量从几十降到个位数，但不是 0；
如果同时看到 `rvv-spill-slots` 从三十多个降到两三个，就能确认 allocator
确实少分配了大量 RVV scalable spill slot。反过来，如果 grep 结果变化很小，
但 stack bytes 大幅变化，就需要检查是否有某些 spill 被合并、某些 reload
形式不在正则覆盖范围内，或者测试输入没有被单函数拆分而混入了其他函数。

第三步才看 `-debug-only=machine-scheduler`。这个日志很长，尤其是 fully
unrolled 输入会产生大量候选比较。读日志时不要把注意力放在每一行资源分数，
而要找三个结构信号：当前 region 是否在追踪 pressure，pick 是从 top 还是
bottom 来，final schedule 中 RVV load 和 consumer 的距离是否缩短。baseline
中如果经常看到 bottom pressure 接近 VR 上限，并且 final schedule 中连续
出现大量 RVV load，那么它和最终 whole-register spill 的关联就很强。optimized
中如果 top-down pick 更稳定，consumer/store 更早出现，后面的汇编变化通常
也会跟上。

调试 reload clone 时，`-stop-after=riscv-v-reg-pressure-reload` 比最终汇编
更清楚。这个 pass 现在位于 pre-RA MachineScheduler 之后，所以停在这里可以
直接观察最终 pre-RA schedule 上 late use 附近是否插入了 cloned load，并检查
late use 是否改写到新虚拟寄存器。如果没有 clone，先看四类原因：block 是否
命中高压阈值，load 是否 simple，是否确实先出现 reduction use，load 到 late
use 之间或 late use 本身是否有 barrier。大多数“为什么没 clone”的答案都落在
低压 block、volatile、store、call、side effect、fault-first load、输入寄存器
重定义或跨块这些保守跳过条件上。

还要避免一个常见误判：optimized 汇编中普通 `vle32.v` 变多，不一定是退化。
对 reduce workload 来说，reload clone 的目标就是用更近的一次普通 load
替换长 live range 引发的多次 whole-register spill/reload。判断是否划算，
不能只数普通 load，而要把 `vs4r.v`/`vl4r.v`、scalable spill slot、地址计算
和调度后的 live range 一起看。这个实验的报告选择 whole-register spill/reload
作为主指标，是因为它最接近原问题：宽 RVV 值被迫离开寄存器文件。

## 四类 workload 的机器级形状

vector add 的机器级形状是最容易理解的。每个展开单元只有两个输入和一个输出，
没有跨单元数据依赖。理想 schedule 可以把每个单元压成很短的链。baseline
的问题不是算法复杂，而是展开后的所有 load 在调度上彼此独立，clustering 和
node order 会自然把它们排到一起。pressure-aware 调度只要打破这个“先全读入”
的形状，就能让 allocator 几乎看不到高峰。它是这个实验的正控制组：如果这个
case 都无法消除 spill，说明 scheduler 偏置没有真正缩短 live range。

safe-softmax 则代表“输入向量有两个阶段用途”的模式。第一阶段是 reduction，
需要把一整组输入压成标量；第二阶段是 elementwise exp 和归一化，又需要每个
元素的向量值。IR 层看起来 `%v` 只是被用了两次，但机器层 reduction 不是一条
无成本指令，它会拉出一段依赖链。若让 `%v` 跨过这段链，LMUL=4 的 live range
就很长。clone 一次 load 后，late exp/normalize 使用新 def，原 def 可以在
reduction 后结束。这就是 softmax 从 86 降到 17 的主要原因。

top2 的特点是 mask 和 select 参与压力。比较产生 mask，select 或 merge 产生
新的候选向量，reduction 再从候选中提取最大值。这里不仅有数据向量，还有 mask
寄存器和条件使用。实验保留 vector mask mutation，避免为了压力优化破坏 mask
相关调度约束。结果从 93 降到 8，说明压力偏置和 mask 逻辑可以共存；剩余少量
spill 则反映 compare/select/reduce 的中间值不能都被局部消费掉。

RMSNorm 的压力最高，因为它同时有“先算统计量”和“再用统计量归一化”的两阶段
结构。输入要平方，平方值要参与 sum reduction，sum 之后还要 sqrt，再把输入
或中间值乘以归一化系数输出。这个形状会制造多个不同来源的宽值：原输入、
平方结果、归一化结果，每类值的最晚 use 都可能不近。调度器可以让局部 elementwise
链更紧凑，reload clone 可以切断部分原输入跨 reduce 链的活跃区间，但无法把
统计量依赖本身消掉。因此 RMSNorm 的下降幅度很大，却仍保留 37 次 whole-register
spill/reload。

## 这不是哪些问题的解法

这个实验容易被误读成“LLVM 后端应该总是避免 load clustering”。这不是本文的
结论。load clustering 对很多目标和很多函数仍然有价值，尤其是压力不高、访存
局部性更重要、或者后续优化能从相邻 load/store 获益的场景。本文只说明，在
宽 RVV、fully unrolled、高 def 数量的 pre-RA region 中，继续 clustering
可能把 register pressure 推过临界点；在这种区域里，跳过 clustering 更有利。

它也不是一个自动 unroll 或 loop vectorizer 策略。真实应用中，减少 unroll
factor、改变 vectorization plan、在 IR 层分块处理，有时能从源头避免压力峰值。
后端调度只能在已经给定的 MachineInstr DAG 内重排，不能改变算法分块，也不能
把一个巨大基本块重新切成多个循环。本文的方案适合修正“后端调度顺序让压力更坏”
的问题，不替代中端对代码形状的控制。

它还不是一个完整的 RVV spill cost model。当前报告用 spill/reload 次数和
stack slot 做趋势分析，没有为每种 LMUL、SEW、微架构、缓存状态建立统一成本。
在某些机器上，额外普通 load 的代价可能比预期更高；在另一些机器上，避免
`vs8r.v`/`vl8r.v` 的收益可能更大。要把这个实验推进到默认启用，需要把这些
差异纳入更系统的性能评估。

## 测试覆盖怎样对应实现风险

这个分支的测试不是只检查最终表格里的数字，而是把风险拆成几层。第一层是
feature gate：`yushuxin-vfexp.ll` 和 MC 测试分别确认无 feature 时不会选择
实验指令，有 feature 时 CodeGen、assembler、disassembler 都能识别。这一层
保护默认行为，避免一个实验 placeholder encoding 悄悄进入普通 RISC-V 目标。

第二层是 scheduler 行为：workload 测试在 baseline 前缀下确认 vector add
确实会出现 whole-register spill，在实验前缀下确认这些 spill 消失，并且
softmax、top2、RMSNorm 仍能生成关键 RVV 指令。这里没有把每一条 schedule 都
写死，因为 MachineScheduler 的合法顺序可能随资源模型和上游变化调整；测试
锁定的是行为边界：flag 开启后高压 workload 应该明显改善，flag 关闭时不假设
新调度。

第三层是 reload 安全性：IR 测试覆盖低压 no-clone、pipeline 位置和基础 skip
case，MIR 测试则构造高压 block 来确认 reduce 后 late use 前可以出现 cloned
load，并确认 volatile、atomic/ordered、fault-first load、输入寄存器重定义、
stale kill flag 等边界都被保守处理。这个测试比单纯看性能更重要，因为 reload
clone 一旦越过 memory 或机器寄存器语义边界，错误可能只在特定运行时数据下暴露。
因此测试宁愿保守，也不接受“看起来 noalias”但机器层没有证明的改写。

第四层是诊断：`-riscv-v-reg-pressure-report` 的测试确认输出字段存在，同时
用 scalable alloca sanity case 说明 scalable stack object 不等于 spill slot。
这样报告里的 `rvv-scalable-stack-bytes` 和 `rvv-spill-slots` 才有稳定解释，
不会把普通 scalable alloca 误算成寄存器压力问题。四层测试合在一起，覆盖了
默认关闭、feature gating、调度趋势、reload 正确性和诊断口径这几个主要风险。
它们也让后续维护者能快速判断回归属于哪一类：是实验指令门控失效，是调度
顺序重新拉长了宽值活跃区间，还是 reload pass 的安全边界被意外放宽。对一个
默认关闭的实验来说，这种分层比单个大而全的 golden assembly 更容易维护。

## 为什么选择保守实现

这个实验有意没有把问题做大。真正完整的解法可能涉及更精细的 RVV register
pressure model、LMUL-aware live range cost、MachineScheduler 和 RA 之间更
强的反馈，甚至在 IR 层避免制造 fully unrolled 宽向量压力峰值。但这些方向
都会扩大风险面，也更难证明默认行为不受影响。

当前实现只在显式隐藏选项下启用，并且只在高压 region 改变偏好。没有 feature
时，`llvm.exp` 不会选择实验 Yushuxin 指令；没有 scheduler flag 时，reload
pass 不运行，cluster mutation 不被包装，默认调度策略保持原状。reload clone
遇到 volatile、atomic、ordered、store、call、unmodeled side effects、跨块
use 都跳过；它宁愿少优化，也不做需要 alias 推理的 speculative reload。

调度启发式同样不试图“手写最终顺序”。它仍然在 GenericScheduler 的候选比较
框架内工作，先尊重物理寄存器偏置、pressure excess、critical pressure、
cluster、weak edge、resource、latency、node order 和 RISC-V vsetvli heuristic。
RVV consumer/store over load 的偏置只在 pressure-aware region、同 boundary
候选可比时介入。这个位置使它足够可控：它能改变高压 fully unrolled kernel
的形状，但不会把所有 RVV block 都变成固定模板。

## 限制和后续方向

第一，`yushuxin.vfexp` encoding 是实验 placeholder，不是稳定 ABI 或最终
ISA 承诺。它使用 reserved OP-V unary slot，只应该在带
`+experimental-yushuxin-vfexp` 的实验构建中观察。

第二，reload clone 不覆盖复杂 alias case。即使 IR 参数带 noalias，当前
MachineFunction pass 也没有建立完整 MemorySSA 或 AA 证明；只要同一 block
里有可能改变内存观察结果的 store/call/side effect，就跳过。跨 basic block
的 load/use 也不处理。

第三，GlobalISel 和 strict fexp 不在范围内。当前 lowering 关注 SelectionDAG
vector `ISD::FEXP`，并通过 feature gate 选择 RVV pseudo。GlobalISel 的
`G_FEXP`、严格浮点异常语义、不同 rounding/exception 模式下的数学契约，都
需要单独设计。

第四，dynamic loop-vectorized workload 没有覆盖。本实验使用 fully unrolled
synthetic IR 是为了稳定复现 pre-RA 调度造成的宽值压力峰值。真实循环可能受
unroll factor、VL policy、memory dependence、loop carried dependence 和
target scheduling model 共同影响，不能直接把这里的数字外推成普遍性能结论。

第五，结果只说明 spill/stack 趋势，不等同于最终 runtime speedup。减少
whole-register spill/reload 通常是好信号，尤其是 RVV scalable spill 很重；
但调度变化也可能改变局部 latency、resource balance 或 cache 行为。真正
进入默认路径前，还需要更广泛 workload、硬件性能计数和 code size 评估。

## 小结

这次实验把一个常见但容易被忽略的问题具体化了：RVV wide virtual register
的活跃区间长度，往往是 pre-RA scheduling 决定的。对 fully unrolled
`<128 x float>` kernel，generic load clustering 和“先 load 后统一消费”的
顺序会把很多 LMUL=4 值同时交给 register allocator，最后表现为 `vs4r.v`、
`vl4r.v` 这类昂贵的 whole-register spill/reload。

`-riscv-v-reg-pressure-aware-sched` 的策略是默认关闭、局部介入、保守盈利。
在高 RVV 压力 region，它开启 pressure tracking，倾向 top-down 调度，跳过
会继续拉长 load run 的 generic clustering，并在同等合法时优先已经打开值的
consumer/store。`RISCVVRegPressureReload` 则为 reduce 后 late use 的安全
same-block simple load 提供 clone，避免原输入向量跨过过长依赖链。

四个 workload 的结果显示了这个方向的价值：vector add 从 50 次 whole-register
spill/reload 降到 0，safe-softmax 从 86 降到 17，top2 从 93 降到 8，
RMSNorm 从 268 降到 37；对应 RVV spill slots 也从 24/33/34/69 降到
0/3/2/4。对 elementwise 场景，关键是就近消费；对 reduce 场景，关键是
调度和保守 reload clone 共同缩短宽值活跃跨度。剩余 spill 是设计边界的一部分：
当算法依赖要求宽值跨越 reduction 或 normalize 链时，调度器不能也不应该假装
这些依赖不存在。
