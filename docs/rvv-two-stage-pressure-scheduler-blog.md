# RISC-V RVV 两阶段寄存器压力调度：源码机制、测试方法与报告解读

这篇文章解释当前分支里的 RVV 寄存器压力调度 demo。它不是一个默认开启的
RISC-V 后端策略，也不是一个完整的通用 rematerialization 框架；它是一个
默认关闭、目标明确的实验：

在固定 VLEN 下，构造足够大的 RVV element-wise 和 softmax reduce workload，
证明调度算法可以把 whole-register vector spill/reload 从大约 4096 级别压到
接近常数。对 softmax，这个结论必须满足一个硬约束：**不能重算 reduction**。

最终实现分成两阶段：

```text
Stage 1: -riscv-rvv-pressure-dag-sched
  在 MachineScheduler 里识别干净 RVV 高压 region，重写 load/store
  clustering 和调度偏好，尽量让宽 RVV 值就近消费。

Stage 2: -riscv-rvv-pressure-remat
  在 MachineScheduler 之前，对 softmax 这类 reduce 后 late use 的模式，
  克隆安全 load 并重算纯 element-wise 链，绝不克隆 reduction。clone 出来
  的节点随后和原始节点一起交给 Stage 1 的 pressure-aware scheduler 调度。
```

核心源码主要在两个文件：

- `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp`
  - Stage 1：识别 pressure region，改 DAG 依赖，改候选节点选择。
- `llvm/lib/Target/RISCV/RISCVVRegPressureRemat.cpp`
  - Stage 2：same-block RVV def-chain clone / pure recompute。

配套测试和证据在：

- `llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-scale.ll`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-cleaned-add-realcase.ll`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat*.ll|mir`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-cleaned-softmax-realcase.ll`
- `llvm/utils/rvv_pressure_evidence.py`
- `docs/rvv-pressure-evidence-validation.md`
- `docs/rvv-core-pressure-scale-measurements.md`

如果只想快速定位源码，可以先看这些入口：

| 机制 | 源码入口 |
| --- | --- |
| Stage 1 flag | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:32` |
| clean region 识别 | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:200` |
| reduce region 识别 | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:260` |
| reduction merge boundary | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:438` |
| pressure window artificial deps | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:555` |
| load/store cluster wrapper | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:649` |
| top-down candidate bias | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:779` |
| Stage 2 pass insertion | `llvm/lib/Target/RISCV/RISCVTargetMachine.cpp:398` |
| Stage 2 load/recompute classifiers | `llvm/lib/Target/RISCV/RISCVVRegPressureRemat.cpp:336` |
| simple RVV load clone 条件 | `llvm/lib/Target/RISCV/RISCVVRegPressureRemat.cpp:456` |
| recursive clone/recompute | `llvm/lib/Target/RISCV/RISCVVRegPressureRemat.cpp:524` |
| reduction-result barrier | `llvm/lib/Target/RISCV/RISCVVRegPressureRemat.cpp:279` |

## 1. 问题模型：为什么 RVV 会出现 4096 级别 spill

实验使用 `<128 x float>` 固定长度向量和：

```sh
-mtriple=riscv64
-mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b
-riscv-v-vector-bits-min=1024
```

在这个配置下，`<128 x float>` 通常会落到宽 RVV register group，例如 LMUL=4。
一个虚拟向量值不再只是“一个寄存器”，而是一组物理向量寄存器。fully-unrolled
代码如果先连续 load 很多向量，再集中计算和 store，就会同时打开大量宽 live
range。pre-RA MachineScheduler 一旦把这些 def 排得早、use 排得晚，后面的
register allocator 只能 spill。

测试里的 add 压力用例由 `rvv-pressure-scale.py add 896` 生成，形状是：

```llvm
%av0 = load <128 x float>, ptr %ap0
%bv0 = load <128 x float>, ptr %bp0
...
%sum0 = fadd <128 x float> %av0, %bv0
...
store <128 x float> %sum0, ptr %op0
```

如果调度接近“所有 load -> 所有 add -> 所有 store”，baseline 会出现约
`4056` 个 whole-register vector spill/reload 匹配。

softmax 压力用例更难。生成器的真实计算公式是：

```text
m      = max(all x blocks)
e_i    = exp2(log2(e) * (x_i - m))
s      = sum_i reduce_sum(e_i)
y_i    = e_i / s
store y_i
```

也就是数学上的：

```text
y_i = exp(x_i - max(x)) / sum_j exp(x_j - max(x))
```

这里 final stores 必须依赖 `%sumv`，否则只是一个假的 softmax-like case。
`v-reg-pressure-scale.ll` 专门检查 `%norm* = fdiv ... %sumv` 后再 store，避免
把 denominator-independent case 当作证据。

## 2. Stage 1：MachineScheduler 里的 pressure-aware DAG 调度

Stage 1 的入口选项定义在 `RISCVMachineScheduler.cpp`：

```c++
static cl::opt<bool> EnableRVVPressureDAGSched(
    "riscv-rvv-pressure-dag-sched", cl::Hidden, cl::init(false), ...);
```

它主要做三件事：

1. 判断当前 scheduling region 是否是值得介入的 RVV 高压 region。
2. 对干净内存访问移除过强的 order dependency，并加入 pressure window 依赖。
3. 在 `tryCandidate` 里偏向“消费已有 RVV 值”的节点，而不是继续调度新 load。

### 2.1 Region 识别：clean 和 reduce 两类

源码里有两个关键 classifier：

```c++
static bool isCleanRVVPressureRegion(...);
static bool isReduceRVVPressureRegion(...);
```

`isCleanRVVPressureRegion` 面向 add 这类 element-wise DAG。它要求：

- region 里 RVV 操作足够多；
- RVV def 的加权压力足够大；
- 至少有多个 RVV load；
- 至少有一个纯 RVV element-wise op；
- 至少有 store；
- 内存访问之间能证明独立。

实际条件在源码里是：

```c++
return RVVOps >= 12 && WeightedRVVDefs >= 32 && RVVLoads >= 2 &&
       RVVPureOps >= 1 && RVVStores >= 1 && hasIndependentMemory(MemRefs, AA);
```

`WeightedRVVDefs` 不是简单计数。`getRVVRegWeight` 会按 register class 给权重：

```text
VR   -> 1
VRM2 -> 2
VRM4 -> 4
VRM8 -> 8
```

这样 LMUL=4 的值会自然比 LMUL=1 更重，调度启发式不会把所有 vector def
当成同样便宜。

`isReduceRVVPressureRegion` 面向 softmax 这类 reduce 后还有 late use 的
region。它允许 reduction 出现，但要求：

- 有 RVV load；
- 有至少一个 RVV reduction；
- reduction 后还有 late use，例如 normalize/store；
- memory independence 仍然能证明。

这也是为什么 softmax Stage 1 会识别 reduce region，但不会把它当成普通
element-wise add 一样处理。

### 2.2 内存安全：不能靠地址空间编号假装 noalias

Stage 1 会改变调度 DAG 的内存 order dependency，所以必须保守证明内存独立。
源码里先把 `MachineMemOperand` 转成 `RVVMemRef`：

```c++
struct RVVMemRef {
  const Value *Ptr;
  const Value *Base;
  int64_t Offset;
  LocationSize Size;
  bool IsStore;
};
```

`areMemRefsIndependent` 的证明来源只有几类：

- 两个都是 load；
- AA 明确 `NoAlias`；
- 同一 base 下固定 offset/size 范围不重叠；
- 不同 base 且至少一个 base 是 `noalias` 参数。

注意这里没有“LLVM address space 不同就一定 noalias”的捷径。RISC-V 上有些
address-space cast 可以是 no-op，直接用地址空间编号证明 noalias 会误删内存
order edge。这个约束由 `v-reg-pressure-addrspace-alias.ll` 覆盖。

### 2.3 替换 load/store clustering：给 load slice 加窗口

RISC-V 原本会创建 load/store cluster mutation。这个实验没有简单关掉
clustering，而是包了一层：

```c++
llvm::createRISCVVRegPressureLoadClusterDAGMutation(...)
llvm::createRISCVVRegPressureStoreClusterDAGMutation(...)
```

wrapper 的逻辑是：

```text
if clean/reduce pressure region:
  removeIndependentRVVMemoryOrderDeps()
  addRVVPressureWindowDeps()
else:
  run original cluster mutation
```

`addRVVPressureWindowDeps` 是 Stage 1 的核心之一。它先为每个 RVV load 找到
对应 slice 的 close 点：

```c++
findRVVSliceClose(LoadSU, MRI, TII, PreferReduction)
```

对 clean region，close 往往是 store 或最终 consumer。对 reduce region，有两类
重要 close：

- reduction input slice 遇到 `vfred*` 时可以 close；
- remat 出来的 `load -> elementwise -> vfadd tree -> vfredusum` 链，在第一次
  多输入 RVV merge 处 close。对 softmax 的 exp sum 来说，这个 merge 通常是
  `vfadd.vv`，因为原 load-derived `e_i` 在这里已经被消费，继续追到最终
  `vfredusum` 会把 close 推得过晚。

后一条是融合方案里的关键点。如果把所有 pure RVV def 都当成透明节点，512 个
remat clone 的 close 会集中到最终 reduction，很多 close-to-next-load artificial
dependency 会因为成环被跳过，scheduler 仍然可能打开大量 M4 temporary。把
多输入 merge 当成当前 slice 的 close 后，clone 节点才真正受 pressure window
约束。

这里的 `close` 不是“softmax 语义上最后使用这个值的地方”，而是
**load-rooted pressure slice 的结束点**。softmax 的 sum-reduction 片段可以
抽象成：

```text
e0 = exp(x0 - m)
e1 = exp(x1 - m)
e2 = exp(x2 - m)
e3 = exp(x3 - m)

p0 = e0 + e1
p1 = e2 + e3
p2 = p0 + p1
s  = reduce_add(p2)
```

`vfredusum` 当然是 denominator `s` 的语义来源，但 `e0` 这条 leaf slice 在
`p0 = e0 + e1` 之后已经结束；后面继续活着的是 partial sum `p0`，不是
`e0`。因此 Stage 1 对 `x0 -> sub -> vfexp -> e0` 这条 pressure slice 的 close
可以落在第一个 multi-input merge，也就是常见的 `vfadd.vv`。这个 merge 的
结果随后形成新的 accumulator slice，再一路流向 `vfredusum`。

所以实际调度目标不是机械地排成 `e0, p0, e1, p1, ...`。`p0` 需要 `e0` 和
`e1` 都 ready，合理形态更接近：

```text
load/sub/vfexp for e0
load/sub/vfexp for e1
vfadd p0 = e0 + e1
load/sub/vfexp for e2
load/sub/vfexp for e3
vfadd p1 = e2 + e3
...
vfredusum s
```

这既给 load latency 留了窗口，也避免把所有 leaf `e_i` 都拖到最终
`vfredusum` 才 close。源码里这个规则不是按 opcode 名特判 `vfadd`，而是表达成：
single-input pure RVV def 对当前 slice 透明；multi-input pure RVV def 如果沿
data edge 能到达 reduction，就是 reduction merge boundary。

然后按 close 的调度顺序排序，用一个窗口限制同时打开的 RVV load 权重：

```c++
constexpr unsigned MaxOpenRVVLoadWeight = 8;
...
NextLoad->addPred(SDep(Close, SDep::Artificial));
```

直观含义是：如果继续调度下一个 load 会让“尚未 close 的 load 权重”超过 8，
就人为加一条依赖，让前一个 slice 的 close 先发生。这样调度器不会连续打开
太多 LMUL=4 load。

这条 artificial dependency 是局部的、可证明无环才加。源码里用
`reachesSUnit(NextLoad, Close)` 检查是否会形成 cycle。

### 2.4 候选选择：优先消费已有宽值

Stage 1 第二个关键点在 `RISCVPreRAMachineSchedStrategy::tryCandidate`：

```c++
if (RVVPressureAwareRegion && Zone && Zone->isTop()) {
  bool TryUsesCurrentVector = isRVVConsumerOrStore(TryMI);
  bool CandUsesCurrentVector = isRVVConsumerOrStore(CandMI);
  bool TryStartsVectorLiveRange = isRVVLoad(TryMI);
  bool CandStartsVectorLiveRange = isRVVLoad(CandMI);
  if (tryGreater(TryUsesCurrentVector && CandStartsVectorLiveRange,
                 CandUsesCurrentVector && TryStartsVectorLiveRange, ...))
    return ...
}
```

也就是说，在 pressure-aware region 的 top-down 调度里，如果一个候选会消费
当前已有 RVV 值，而另一个候选会开启新的 RVV load live range，就偏向前者。
这不是全局最优求解器，而是一个非常明确的局部策略：**少开新宽值，先消费旧宽值**。

对 add 这种纯 element-wise DAG，这已经足够。`add-896` 的测试结果是：

| Workload | Opt | Baseline | Stage 1 | Stage 1+2 |
| --- | --- | ---: | ---: | ---: |
| add-896 | O2 | 4056 | 0 | 0 |
| add-896 | O3 | 4056 | 0 | 0 |

Stage 1 可以把 add 从比例级 spill 压到 0，因为每个 load 只要尽快被 add/store
消费，就不需要跨越全局 reduce 或 late normalization。

## 3. Stage 2：softmax 的 load remat / pure recompute

softmax 不同。即使 Stage 1 能让 reduction 输入更流式，原始 `e_i` 仍然要在
sum reduction 后用于：

```text
y_i = e_i / s
```

如果一直保留所有 `e_i`，就会把宽向量 live range 跨过 reduction 链，baseline
和 stage1-only 都仍然是比例级：

| Workload | Opt | Baseline | Stage 1 | Stage 1+2 |
| --- | --- | ---: | ---: | ---: |
| softmax-512 | O2 | 4034 | 2532 | 0 |
| softmax-512 | O3 | 4034 | 2532 | 0 |

这就是 Stage 2 的动机：不要让 `x_i` 或 `e_i` 长活到 late store；在 late use
附近重新 load `x_i`，重新计算：

```text
centered_i = x_i - m
e_i        = exp(centered_i)
y_i        = e_i / s
store y_i
```

但这里有一个硬约束：`m` 和 `s` 是 reduction 结果，不能重算 reduction。
Stage 2 只允许使用已经算出的 reduction result，不能 clone `vfredmax`、
`vfredusum` 或 reduction-result move-like def。

### 3.1 Pass 开关和插入位置

Stage 2 在 `RISCVVRegPressureRemat.cpp`，入口是：

```c++
bool RISCVVRegPressureRemat::runOnMachineFunction(MachineFunction &MF)
```

它首先检查两个 flag：

```c++
if (!isRISCVRVVPressureDAGSchedEnabled()) {
  if (isRISCVRVVPressureRematRequested())
    warning: remat requires dag-sched
  return false;
}

if (!isRISCVRVVPressureRematRequested())
  return false;
```

因此：

- 只开 Stage 1：remat pass 不改代码；
- 只开 Stage 2：打印 warning，然后不做 remat；
- Stage 1+2 同时开启：先 clone/remat，再让 MachineScheduler 调度原始节点和
  clone 节点。

pass pipeline 里，Stage 2 被插在 `RenameIndependentSubregsID` 后、
`MachineSchedulerID` 前。这样 remat 不再需要自己做 post-scheduler layout
splice；它只负责把安全的 late-use def chain 复制出来，最终布局交给
MachineScheduler 的 pressure window artificial dependency 和候选选择。

`v-reg-pressure-remat.ll` 覆盖了这些接口，包括旧的
`-riscv-v-reg-pressure-aware-sched` flag 已经不存在。

### 3.2 什么可以 clone 或 recompute

Stage 2 的基本单位是“同一个 basic block 内，def 在 use 之前”。核心 helper：

```c++
getSimpleRVVLoadDef(MI)
isPureRecomputableDef(MI)
getCloneableRVVDef(MI)
```

`getSimpleRVVLoadDef` 只接受简单、无副作用、非 volatile/atomic、非 fault-first
的 RVV load，并且只有一个 RVV def。它还拒绝 load 自身使用 RVV input 的情况，
避免 gather/segment/masked 等复杂形态。

`isPureRecomputableDef` 只接受可移动的 element-wise def。对于 RVV FP 指令，
条件更严格：

- 不能是 reduction；
- 不能写 `VXSAT`；
- 不能是 masked/segment/gather/scatter；
- 如果动态读取 `$frm`，并且 block 里有人定义/clobber `FRM`，就拒绝；
- 对 `VF*` 或 `YUSHUXIN` 这类 FP op，必须有完整 fast-math/no-fp-except 语义。

源码里的 fast-math 判断是：

```c++
NoFPExcept &&
FmNoNans &&
FmNoInfs &&
FmNsz &&
FmArcp &&
FmContract &&
FmAfn &&
FmReassoc
```

这保证 recompute 不是在 strict FP 或动态 rounding 语义下偷偷改变结果。

### 3.3 reduction barrier：绝不重算 reduction

Stage 2 的安全性核心是：

```c++
isReductionUse(MI)
isReductionResultMoveLikeDef(MI)
isReductionResultDef(MI)
isReductionRematBarrier(MI)
```

`isReductionUse` 通过 opcode 名称识别 `VRED` / `VFRED` / `VWRED`。
`isReductionResultMoveLikeDef` 识别 copy、insert/extract subreg、reg_sequence、
`VMV`、`VFMV` 这类 move-like def。

`isReductionResultDef` 是递归判断：如果一个 move-like def 的输入来自 reduction
结果，那么它本身也算 reduction result def。这样 Stage 2 不只是阻止直接 clone
`vfredusum`，还阻止 clone “从 reduction result 拷贝出来的值”。

所有 clone/recompute 入口都会检查 barrier：

```c++
if (isReductionRematBarrier(DefMI) || isReductionRematBarrier(Use))
  return false;
```

`cloneDefChainForUse` 递归 clone 输入链时，如果遇到 reduction barrier，也不会
向上复制 reduction。softmax late normalize 可以使用已经存在的 denominator，
但不会重建 denominator。

### 3.4 核心动作：clone/recompute def chain

Stage 2 现在只做 clone/recompute，不再做 layout splice。`processLoad` 会寻找
这样的模式：

```text
load x
... reduction uses x ...
... late element-wise use/store uses x ...
```

如果 late use 在 reduction 之后，且 clone 可以缩短足够多的 weighted live
range，就在 late use 前 clone load，并把 late use 改写到新寄存器。

`processRecompute` 做的是更强的版本：如果 late use 使用的是纯 element-wise def，
它可以递归 clone 整条安全链：

```text
load x
sub x, max
vfexp
late normalize/store
```

变成：

```text
load x
sub x, max
vfexp
reduction builds s

load x'       ; cloned near store
sub x', max   ; recomputed pure op
vfexp'        ; recomputed pure fast-math op
normalize with existing s
store
```

这就是报告里看到 `vfexp 512 -> 1024`、`loads 512 -> 1536` 的原因：多出来的是
safe load 和 pure element-wise recompute，不是 reduction recompute。

clone 发生在 MachineScheduler 之前，因此这些新节点不是被 Stage 2 强行挪到
store 前。它们会进入同一个 scheduling DAG，再由 Stage 1 的 pressure window
依赖决定何时开启下一条 load/compute 链。这样既保留了 load latency hiding 的
空间，也避免了 post-scheduler 直接 splice 指令打乱调度结果。

### 3.5 为什么 softmax Stage 1 仍然是比例级

这点很重要。`softmax-512` 的 Stage 1 结果是 `2532`，没有下降到 0。这不是坏事，
反而是正确分层的证据。

Stage 1 只改变 MachineScheduler DAG 调度。它可以把 reduction 输入更流式地
排到 reduction 前，但它不能消除这样一个事实：`e_i` 在 sum reduction 后仍被
late normalization 使用。只要不复制或重算 `e_i`，这些宽值就必须跨过 reduction
链。因此 Stage 1 不能完成 softmax 的最终压力边界。

Stage 2 才负责把 late use 改成“重新 load/recompute element-wise 链”，从而
缩短 `x_i/e_i` 的 live range。最终报告：

```text
baseline  = 4034
stage1    = 2532
stage1+2  = 0
```

这个结果说明：真正让 softmax 从比例级 spill 变成常数的是 Stage 2，同时
Stage 1+2 的 pass dump 又证明 reduction 数量没变。

## 4. 测试体系：每层测什么

这个 demo 的测试不是只看一个 `FileCheck`。它分四层。

### 4.1 lit 编译测试

核心 lit 命令：

```sh
build-riscv/bin/llvm-lit -q \
  llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp-strict-scalable.ll \
  llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-split-use.mir \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-atomic.mir \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-scale.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-cleaned-add-realcase.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-cleaned-softmax-realcase.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-reduction-result.mir \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-addrspace-alias.ll
```

这些测试覆盖：

- `v-reg-pressure-scale.ll`
  - add baseline 有 spill，Stage 1 没有 whole-register spill/reload；
  - softmax baseline 有 spill，Stage 1+2 没有 whole-register spill/reload；
  - generated softmax IR 的 store 依赖 `%sumv`；
  - softmax 都使用 `yushuxin.vfexp`，没有退回 `exp2f`。
- `v-reg-pressure-remat.ll`
  - pass order 在 MachineScheduler 前、Greedy RA 前；
  - remat without stage1 会 warning 并 no-op；
  - 旧 flag 被移除。
- `v-reg-pressure-remat-split-use.mir`
  - 一个 load 同时喂 reduction 和 late element-wise use 时，只 clone late use，
    不 clone reduction。
- `v-reg-pressure-remat-reduction-result.mir`
  - reduction result 和 move-like reduction result 不可 remat。
- `v-reg-pressure-remat-atomic.mir`
  - volatile/atomic/alias barrier/dynamic FRM/kill flags 等安全边界。
- `v-reg-pressure-addrspace-alias.ll`
  - 不把不同 address space 当作 RISC-V noalias 证明。
- `v-reg-pressure-cleaned-add-realcase.ll`
  - 从 realcase 清理出的 add 形状仍触发 Stage 1，而不是只在 synthetic generator
    上成立。
- `v-reg-pressure-cleaned-softmax-realcase.ll`
  - 从 realcase 清理出的 softmax 形状仍通过调度与 vfexp 检查。

### 4.2 规模计数测试

规模测试的 fixed configuration：

```sh
VLEN_FLAGS="-mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024"
COUNT_RE='\b[vu][sl][1248]r\.v\b'
```

`COUNT_RE` 只数 whole-register vector spill/reload：

```text
vs1r.v vs2r.v vs4r.v vs8r.v
vl1r.v vl2r.v vl4r.v vl8r.v
```

它不会把普通业务 load/store，例如 `vle32.v` / `vse32.v`，算成 spill。

手工复现 add：

```sh
python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py add 896 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py add 896 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -riscv-rvv-pressure-dag-sched -o - |
  grep -Eo "$COUNT_RE" | wc -l
```

手工复现 softmax：

```sh
python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -riscv-rvv-pressure-dag-sched -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS \
  -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -o - |
  grep -Eo "$COUNT_RE" | wc -l
```

当前记录的结果在 `docs/rvv-core-pressure-scale-measurements.md`：

| Workload | Opt | Baseline | Stage 1 | Stage 1+2 |
| --- | --- | ---: | ---: | ---: |
| add-896 | O2 | 4056 | 0 | 0 |
| softmax-512 | O2 | 4034 | 2532 | 0 |
| add-896 | O3 | 4056 | 0 | 0 |
| softmax-512 | O3 | 4034 | 2532 | 0 |

### 4.3 evidence 脚本

更完整的证据由：

```sh
python3 llvm/utils/rvv_pressure_evidence.py \
  --generated-count 512 \
  --opt O2 --opt O3 \
  --work-dir /tmp/rvv-pressure-evidence-r11-strict-round
```

脚本做的事情比 lit 更完整：

1. 生成 large softmax IR。
2. 检查 final stores 是否依赖 `%sumv`。
3. baseline / stage1 / stage1+2 分别编译汇编。
4. 检查 `yushuxin.vfexp` 存在且没有 `exp2f` fallback。
5. 统计 whole-register spill/reload。
6. 生成 remat pass、MachineScheduler、Greedy RA 前后的 pass dump。
7. 比较 remat pass 前后的 reduction 数量。
8. 对 cleaned realcase softmax 做同类检查。

当前成功输出摘要是：

```text
SUMMARY: PASS=25 FAIL=0 SKIP=2
```

`SKIP=2` 是因为没有传 `--runtime`，runtime add/softmax 被显式跳过；这不是失败。

### 4.4 runtime ELF / qemu 验证

runtime 验证用：

```sh
python3 llvm/utils/rvv_pressure_evidence.py \
  --runtime \
  --generated-count 1 \
  --opt O2 \
  --work-dir /tmp/rvv-pressure-runtime-r11-strict-round
```

这个脚本会：

- 为 add 和 softmax 生成小规模 IR；
- 编译 baseline 和 stage1+2 两种 ELF；
- 用本机 `qemu-riscv64` 执行；
- add 检查输出字节完全等于预期；
- softmax 用 Python host reference 做容差比较；
- runtime softmax 为了 qemu 可运行，不使用实验 `yushuxin.vfexp`，而是使用
  scalar `exp2f` lowering 加 harness 内 freestanding polynomial `exp2f` helper。

当前成功摘要：

```text
SUMMARY: PASS=19 FAIL=0 SKIP=0
```

关键行类似：

```text
PASS: runtime-add-baseline: qemu output matched exactly
PASS: runtime-add-stage1+2: qemu output matched exactly
PASS: runtime-softmax-baseline: qemu output matched host reference, max_abs=...
PASS: runtime-softmax-stage1+2: qemu output matched host reference, max_abs=...
```

runtime 证明的是“调度开关不会改变程序结果”。它不用于证明 `yushuxin.vfexp`
选择；vector exp 选择由 compiler evidence 路径证明。

## 5. 如何解读测试报告

`rvv_pressure_evidence.py` 的输出是最重要的报告。可以按下面顺序读。

### 5.1 `generate-*`

```text
PASS: generate-softmax-512: wrote /tmp/.../generated-softmax-512.ll
```

这只是说明压力用例 IR 生成成功。真正判断 softmax 是否有效，要看下一类。

### 5.2 `*-denominator`

```text
PASS: generated-softmax-512-denominator: 512 store(s) depend on %sumv
```

这行非常关键。它表示所有 512 个 final stores 都是：

```llvm
%norm_i = fdiv fast <128 x float> %exp_i, %sumv
store <128 x float> %norm_i, ...
```

如果这里失败，后面的 spill 数字没有意义，因为那可能不是一个真正的 softmax
late-denominator-use case。

### 5.3 `*-asm`

典型输出：

```text
PASS: generated-softmax-512-O2-baseline-asm:
  spills/reloads=4034, yushuxin.vfexp=512

PASS: generated-softmax-512-O2-stage1-asm:
  spills/reloads=2532, yushuxin.vfexp=512

PASS: generated-softmax-512-O2-stage1+2-asm:
  spills/reloads=0, yushuxin.vfexp=1024
```

解读方式：

- `spills/reloads` 只数 whole-register vector spill/reload，不数普通业务访存。
- baseline 接近 4096，说明压力 case 足够大。
- Stage 1 对 softmax 仍是比例级，说明 DAG-only 没有假装解决 reduce late-use。
- Stage 1+2 为 0，说明 remat/recompute 把宽 live range 压短了。
- `yushuxin.vfexp=512 -> 1024` 是预期：Stage 2 重算 pure element-wise exp 链。
- 如果出现 `exp2f`，脚本会报 FAIL，因为 baseline/stage1/stage1+2 必须在同一
  vector-exp 特性下比较。

对 add，应该读成：

```text
baseline ~4056
stage1   0
stage1+2 0
```

这说明 add 的压力问题只靠 Stage 1 就能解决；Stage 2 不应该“ materially”
改变 add 结果。

### 5.4 `*-pass-dump`

```text
PASS: generated-softmax-512-O2-pass-dump:
  captured remat, MachineScheduler, and greedy sections in /tmp/...txt
```

这说明脚本成功抓到了：

- `machine-scheduler` 前后；
- `riscv-v-reg-pressure-remat` 前后；
- `greedy` 前后。

如果只看最终汇编，你只能知道 spill 消失了；pass dump 才能说明它是怎么消失的。
特别是 remat pass 前后，可以看到：

```text
before:
  load/sub/vfexp ... reduction builds s

after:
  reduction still exists once
  late store 前新增 load/sub/vfexp
```

### 5.5 `*-no-reduction-recompute`

典型输出：

```text
PASS: generated-softmax-512-O2-no-reduction-recompute:
  remat reduction count unchanged (513); vfexp 512->1024, loads 512->1536
```

这是 softmax 结论最关键的一行。

含义：

- `reduction count unchanged (513)`：remat pass 前后 reduction 数量不变。
- `vfexp 512->1024`：多出来的是 element-wise exp recompute。
- `loads 512->1536`：多出来的是 cloned safe loads。

所以 Stage 2 的逻辑不是：

```text
重算 max reduction
重算 sum reduction
```

而是：

```text
保留原 reduction 结果
在 late store 前重载 x_i
重算 x_i - m 和 exp(x_i - m)
使用已有 s 做 normalize
```

如果 future patch 让 reduction count 变成 `513 -> 1025` 或类似数字，这就是
硬失败，即使 spill 数字很好看也不能接受。

### 5.6 realcase softmax 行

报告里还有：

```text
PASS: cleaned-realcase-softmax-O2-baseline-asm: spills/reloads=0, yushuxin.vfexp=1
...
PASS: cleaned-realcase-softmax-O2-no-reduction-recompute:
  remat reduction count unchanged (2); vfexp 1->1, loads 1->2
```

这里 baseline 本身就是 0，因为 cleaned realcase 很小，不是 stress case。
它的意义不是证明 scale，而是证明真实来源的 softmax 形状也满足：

- denominator dependency；
- reduction 不被复制；
- vector exp lowering 正常；
- pass dump 能复现。

规模结论看 generated `softmax 512`；语义来源看 cleaned realcase。

### 5.7 runtime 行

runtime 输出里：

```text
PASS: runtime-softmax-stage1+2:
  qemu output matched host reference, max_abs=..., sum_abs=..., exp2f_abs=...
```

读法：

- `max_abs`：128 个 softmax 输出元素与 host reference 的最大绝对误差。
- `sum_abs`：sum reduction 结果误差。
- `exp2f_abs`：harness 内 freestanding `exp2f` probe 误差。

`SKIP` 不能当 PASS。比如缺 qemu、qemu ISA 不支持、工具链缺失都会显式 SKIP。
只有 `FAIL=0` 且需要 runtime 时 `SKIP=0`，才说明本机跑通了完整 runtime 验证。

## 6. Pass dump 里应该看到什么

最小可读复现命令：

```sh
python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 4 |
build-riscv/bin/llc -O2 \
  -mtriple=riscv64 \
  -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b \
  -riscv-v-vector-bits-min=1024 \
  -riscv-rvv-pressure-dag-sched \
  -riscv-rvv-pressure-remat \
  -stop-after=riscv-v-reg-pressure-remat \
  -o -
```

理想结构是：

```text
load x_0
sub x_0, m
vfexp e_0
load x_1
sub x_1, m
vfexp e_1
...
vfredusum builds s

load x_0'
sub x_0', m
vfexp e_0'
mul/div with existing s
store y_0
...
```

`docs/rvv-pressure-evidence-validation.md` 里记录了精简 excerpt。关键是看：

- reduction 前有原始 streaming exp 链；
- reduction 后 late store 前有 cloned load + recomputed exp 链；
- denominator/reduction result 是已有值，不是重新 reduction 出来的新值。

## 7. 常见误读

### 误读 1：Stage 1 softmax 没降，说明 Stage 1 没用

不对。Stage 1 对 add 已经把 4056 降到 0。softmax 里 Stage 1 不能解决
reduce 后 late use 的全局依赖，这是算法边界，不是实现失败。

### 误读 2：`vfexp 512->1024` 说明重算太多，不安全

需要看同时出现的 reduction count：

```text
reduction count unchanged (513)
```

重算的是 pure element-wise 链，不是 reduction。对这个 demo，允许用计算量换
wide live range；不允许用重算 reduction 换 spill。

### 误读 3：`loads 512->1536` 是普通内存流量，不是 remat 证据

它正是 remat 证据之一，但要和 alias/side-effect 检查一起读。Stage 2 只 clone
same-block safe load，且检查 volatile/atomic/alias store/call/base clobber 等 barrier。

### 误读 4：realcase baseline 为 0，说明测试太弱

realcase cleaned softmax 是语义形状证明，不是压力规模证明。压力规模由 generated
`softmax 512` 负责。

### 误读 5：runtime 没用 `yushuxin.vfexp`，所以不能证明 compiler path

runtime 证明语义正确性；compiler evidence 证明 vector exp lowering 和 spill
趋势。两者分开是因为本机 qemu 未必支持实验 custom vector exp 指令。所有
baseline/stage1/stage1+2 的 compiler evidence 都带
`+experimental-yushuxin-vfexp`。

## 8. 如果要改这个算法，先看哪些测试

如果改 Stage 1：

```sh
build-riscv/bin/llvm-lit -q \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-scale.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-addrspace-alias.ll
```

如果改 Stage 2：

```sh
build-riscv/bin/llvm-lit -q \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat.ll \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-split-use.mir \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-reduction-result.mir \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-remat-atomic.mir
```

如果改 vfexp / strict FP 周边：

```sh
build-riscv/bin/llvm-lit -q \
  llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll \
  llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp-strict-scalable.ll
```

任何触碰核心算法的改动，最后都应该跑完整 evidence：

```sh
python3 llvm/utils/rvv_pressure_evidence.py \
  --generated-count 512 \
  --opt O2 --opt O3 \
  --work-dir /tmp/rvv-pressure-evidence

python3 llvm/utils/rvv_pressure_evidence.py \
  --runtime \
  --generated-count 1 \
  --opt O2 \
  --work-dir /tmp/rvv-pressure-runtime
```

接受标准不是某个固定 magic number，而是趋势和不变量：

- add：baseline 在 4096 压力区间，Stage 1 接近 0。
- softmax：baseline 和 Stage 1 可以仍然比例级，Stage 1+2 必须接近 0。
- reduction count：Stage 1+2 remat 前后不变。
- generated softmax：stores 依赖 `%sumv`。
- compiler evidence：softmax 使用 `yushuxin.vfexp`，不回退 `exp2f`。
- runtime：需要本机 qemu 时 `FAIL=0` 且 `SKIP=0`。

## 9. 总结

这个实现的核心不是“写一个更聪明的 register allocator”，而是在 register
allocation 之前修正调度给 allocator 制造的压力形状。

Stage 1 在 MachineScheduler 里限制 RVV load live range 的打开速度，并偏向
就近消费。它解决 add 这种 clean element-wise 高压 case。

Stage 2 针对 softmax 这类 reduce 后 late use 的结构，用 safe load clone 和
pure element-wise recompute 替代长 live range。它严格禁止 reduction recompute，
并用 pass-dump 的 reduction count 不变来证明这一点。

最终证据链是：

```text
large add:      4056 -> 0
large softmax:  4034 -> 2532 -> 0
reduction:      513 -> 513
runtime:        PASS=19 FAIL=0 SKIP=0
```

这才是这个 demo 的核心结论：在固定 VLEN 的大规模 RVV stress case 上，正确的
两阶段调度可以把 spill 从随 slice 数增长的比例级，压到接近 O(1) 的常数，同时
不靠重算 reduction 取巧。
