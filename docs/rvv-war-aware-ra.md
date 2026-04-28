# RISC-V RVV WAR-aware RA demo

这篇文档说明当前分支里的 RVV WAR-aware register allocation demo。它的目标不是把 LLVM 变成一个完整的 RVV 微结构模拟器，而是把一个 LLVM RA 层面真实可控的选择暴露出来：当多个 RVV 物理寄存器都能合法分配时，尽量不要立刻复用同一基本块里刚刚结束的物理向量寄存器；然后用 `llvm-mca` 的默认关闭 WAR issue 模型证明这种重命名选择会减少可见的 issue stall。

当前分支保留四个功能边界：

1. `RegAllocGreedy` 的 recent-reuse-aware 物理寄存器选择。
2. `llvm-mca` 的 OOO scheduler 自定义 issue hazard hook 和 RISC-V RVV WAR 模型。
3. 一个轻量 standalone report utility，用于从汇编文本里数短距离 RVV WAR pair。
4. 聚焦测试用例，而不是扩展到 fault-first、indexed memory、mask memory、segmented transfer 等非常用 corner case。

## 为什么现有 LLVM RA 看不到这个问题

LLVM 的寄存器分配首先处理的是合法性：寄存器类约束、live interference、spill/split/eviction 成本、copy/hint 和 ABI 约束。对 `RegAllocGreedy` 来说，如果一个虚拟寄存器的某个物理寄存器没有 live interference，那么复用这个物理寄存器就是合法的。刚刚死亡的值释放了 `$v8`，下一个短生命周期值继续拿 `$v8`，在 LLVM 的 live interval 模型里没有问题。

但某些乱序实现里可能存在 RVV write-after-read issue hazard：较老的 RVV reader 还在 scheduler 队列里等待 issue，较年轻的 RVV writer 又准备写同一个物理向量寄存器。为了维持物理寄存器语义，硬件可能必须延后发射这个 younger writer，直到 older reader 真正 issue。这个 hazard 不是 live interval interference，因为 older reader 在 ISA 顺序上读的是旧值，younger writer 写的是后来的新值；从编译器语义看它们可以合法复用同一个物理寄存器。

所以 RA 侧能做的不是改变合法性，而是在“同样合法的 free physical register”之间改变偏好：优先选择最近没有在当前基本块里用过、或上一次同块 assignment 结束得更早的 RVV 物理寄存器。

## 为什么现有 llvm-mca 也看不到

`llvm-mca` 已经能模拟 register dependency、resource pressure、dispatch/issue/retire 等事件，但默认模型不会额外表达“younger RVV write 必须等 older unissued RVV read”的 target-specific issue constraint。旧的 `CustomBehaviour::checkCustomHazard` 更适合在固定 pipeline 点上处理 target hazard；这个分支需要的是 OOO scheduler 在每次从 `ReadySet` 选择指令前，能问 target：这个 ready instruction 是否因为更老的未 issue 指令而暂时不能 issue。

因此 generic MCA 改动很窄：

- [CustomBehaviour.h](../llvm/include/llvm/MCA/CustomBehaviour.h) 增加 `checkCustomIssueHazard` 和 `noteCustomIssueBlockedCycle`，默认实现什么都不做。
- [Context.cpp](../llvm/lib/MCA/Context.cpp) 创建 `Scheduler` 时把 target `CustomBehaviour` 传进去。
- [Scheduler.cpp](../llvm/lib/MCA/HardwareUnits/Scheduler.cpp) 在 OOO `Scheduler::select` 里，对资源可用的 ready candidate 调用 target hook；如果 candidate 被 custom hazard 挡住，就继续尝试其他 ready 指令，并记录 blocked cycle。

这个设计保持默认行为不变：没有 target override 或没有打开 hidden option 时，scheduler 路径仍等价于原始 OOO issue 选择。

## RA 实现

### TargetRegisterInfo hook

generic RA 不应该硬编码 RISC-V 或 RVV，所以 [TargetRegisterInfo.h](../llvm/include/llvm/CodeGen/TargetRegisterInfo.h) 增加两个默认关闭 hook：

```c++
virtual bool shouldUseRecentPhysRegReuseAvoidance(
    Register VirtReg, const TargetRegisterClass *RC,
    const MachineFunction &MF) const {
  return false;
}

virtual void getRecentPhysRegReuseAliases(
    MCRegister PhysReg, const TargetRegisterClass *RC,
    SmallVectorImpl<MCRegister> &Aliases, const MachineFunction &MF) const {
  Aliases.push_back(PhysReg);
}
```

RISC-V 在 [RISCVRegisterInfo.cpp](../llvm/lib/Target/RISCV/RISCVRegisterInfo.cpp) 里通过 hidden option 打开：

```c++
-riscv-rvv-avoid-recent-vreg-reuse
```

`shouldUseRecentPhysRegReuseAvoidance` 只有在这个 option 打开且 register class 是 RVV 时返回 true。`getRecentPhysRegReuseAliases` 会根据 RVV register class 的 LMUL 和 NF 展开占用范围，例如 LMUL=2 从 `v8` 开始会记录 `v8` 和 `v9`，这样后续分配 `v9` 也能看到和 `v8m2` 的重叠历史。

### RegAllocGreedy 评分点

核心入口在 [RegAllocGreedy.cpp](../llvm/lib/CodeGen/RegAllocGreedy.cpp) 的 `tryAssign`：

```c++
MCRegister PhysReg = tryAssignRecentReuseAvoidingPhysReg(VirtReg, Order);
bool UsedRecentReuseScoring = PhysReg.isValid();

if (!PhysReg) {
  for (auto I = Order.begin(), E = Order.end(); I != E && !PhysReg; ++I) {
    ...
  }
}
```

这意味着新算法只插在 direct assignment 之前。如果没有启用 hook、当前 virtual register 没有可安全定位的真实 def、或没有 free candidate，它直接回到原始 LLVM 路径。Eviction、splitting、spill、last chance recoloring 仍然走原来的 greedy allocator。

`tryAssignRecentReuseAvoidingPhysReg` 的评分对象只包括 `AllocationOrder` 里当前 `LiveRegMatrix` 判定无 interference 的物理寄存器。排序规则是：

1. `RegCosts` 更低者优先。
2. 没有同块 recent-reuse 记录者优先。
3. 如果都有同块记录，上一段 assignment 的 `Segment.End` 越早越好。
4. 前面相同，soft hint 作为 tie-breaker。

也就是说，recent-reuse 不覆盖硬成本和硬合法性，只在合法 free candidate 之间改变选择。

### 近期历史如何维护

[RegAllocGreedy.h](../llvm/lib/CodeGen/RegAllocGreedy.h) 增加 `RecentPhysRegReuseRecords`，按 target 展开的 physical alias bucket 存记录。一次记录保存：

- virtual register id；
- 最终分配的 physical register；
- register class；
- live interval segment 的 def/end snapshot；
- 这个 interval 是否已被 `LiveRangeEdit` 移除。

几个一致性点比较关键：

- `recordRecentPhysRegReuse` 只在 virtual register 真正分配到 physical register 后记录。
- `aboutToRemoveInterval` 不直接删除历史，而是标记 `IntervalRemoved`，保留已经发生过的 segment snapshot，避免 split/erase 后丢失“刚才确实用过这个物理寄存器”的信息。
- `LRE_WillShrinkVirtReg` 会删除 active record，因为 shrink 后旧 segment 形状不再可靠。
- last chance recoloring 里的临时 assignment 先放到 pending list，只有 `selectOrSplit` 最终成功时才 commit，避免失败回滚的 recolor 污染后续评分。

## llvm-mca 实现

### Scheduler hook

[Scheduler.cpp](../llvm/lib/MCA/HardwareUnits/Scheduler.cpp) 的 OOO `select` 现在会在资源可用后询问 target hook：

```c++
if (CB && CB->checkCustomIssueHazard(IR, WaitSet, PendingSet, ReadySet)) {
  SawCustomBlock = true;
  CustomBlockedIndices.push_back(QueueIndex);
  continue;
}
```

如果一个 ready instruction 被 custom hazard 挡住，scheduler 不会立刻停住整个周期，而是继续看其他 ready instruction；只有当本周期至少出现过 custom block 时，才通过 `noteCustomIssueBlockedCycle` 让 target 统计 blocked cycle。

### RISC-V RVV WAR 模型

RISC-V 侧实现在 [RISCVCustomBehaviour.cpp](../llvm/lib/Target/RISCV/MCA/RISCVCustomBehaviour.cpp)，由 hidden option 控制：

```c++
-riscv-rvv-war-hazard-model
```

打开后，`checkCustomIssueHazard` 把当前候选 writer 和更老的 `WaitSet`、`PendingSet`、`ReadySet` 指令比较：

1. 只看 `Writer.getSourceIndex() > Reader.getSourceIndex()` 的 older reader。
2. 只看 RISC-V vector physical register `V0..V31`。
3. 如果 writer 的 defs 和 older reader 的 uses 读写同一个 RVV physical register，就阻止 writer issue。
4. 记录 hazard key、blocked issue event、blocked issue cycle、涉及的寄存器和最多 8 条 sample。

报告通过 target end view 输出：

```text
RVV WAR Hazard
Total hazards: N
Blocked issue events: N
Blocked issue cycles: N
Registers: v8 ...
Samples:
  #3 waits for #2 on v8
```

当前分支的 MCA 模型刻意保持聚焦：它覆盖直接物理 RVV register 的 WAR issue hazard，用来验证 RA 产生的 `v8 -> v8` 与 `v8 -> v20` 这类差异；不把旧实验分支后续那些 fault-first、indexed memory、mask memory、segmented transfer 的 alias corner case 一起纳入。

### Demo CPU

[RISCVProcessors.td](../llvm/lib/Target/RISCV/RISCVProcessors.td) 增加 `rvv-war-demo`。它复用一个 OOO scheduling model，并打开足够的 RVV ISA feature，让测试里简单的 vector load/ALU 可以进入 MCA。这个 CPU 不是要代表某个真实芯片，而是提供一个稳定的 demo 环境：默认模型能 OOO issue，打开 WAR option 后才能看到额外 stall。

## 测试方法

推荐用独立 build 目录：

```bash
cmake -S llvm -B build-rvv-war-ra-core -G Ninja \
  -DLLVM_ENABLE_PROJECTS=llvm \
  -DLLVM_TARGETS_TO_BUILD=RISCV \
  -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
  -DCMAKE_BUILD_TYPE=Release

ninja -C build-rvv-war-ra-core llc llvm-mca FileCheck not count llvm-config llvm-readobj
```

### RA 测试

```bash
build-rvv-war-ra-core/bin/llvm-lit -a \
  llvm/test/CodeGen/RISCV/rvv/rvv-war-ra-reuse.mir \
  llvm/test/CodeGen/RISCV/rvv/rvv-war-ra-nonzero-cost.mir
```

[rvv-war-ra-reuse.mir](../llvm/test/CodeGen/RISCV/rvv/rvv-war-ra-reuse.mir) 同时跑默认和打开 option 两种模式：

```bash
llc %s -mtriple=riscv64 -mattr=+v \
  -run-pass=greedy,virtregrewriter -o - | FileCheck %s --check-prefix=DEFAULT

llc %s -mtriple=riscv64 -mattr=+v \
  -run-pass=greedy,virtregrewriter \
  -riscv-rvv-avoid-recent-vreg-reuse -o - | FileCheck %s --check-prefix=AVOID
```

读这个测试时，重点看同一段 MIR 在 `DEFAULT` 和 `AVOID` 下的物理寄存器编号。例如默认路径可能连续复用 `$v29`，打开 option 后第二个短生命周期 RVV 值改用 `$v30`。这证明 RA 的选择点确实改变了，而不是后端某个更晚 pass 做了重写。

[rvv-war-ra-nonzero-cost.mir](../llvm/test/CodeGen/RISCV/rvv/rvv-war-ra-nonzero-cost.mir) 是 cost-aware 形状测试：它保证 recent-reuse scoring 遍历的是合法 free candidate，同时仍保留 `RegCosts` 优先级。

### llvm-mca 测试

```bash
build-rvv-war-ra-core/bin/llvm-lit -a \
  llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/rvv-war-hazard.s \
  llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/rvv-war-renamed.s \
  llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/scalar-no-war.s
```

[rvv-war-hazard.s](../llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/rvv-war-hazard.s) 是核心对照：

```asm
vadd.vv v10, v8, v12
vadd.vv v8, v14, v15
vadd.vv v18, v8, v19
```

默认 `llvm-mca` 不输出 `RVV WAR Hazard`；打开 `-riscv-rvv-war-hazard-model` 后，总周期从 6 增到 7，并输出 `v8` 的 blocked issue report。这说明同一个物理 RVV register 的 read/write 复用在 MCA issue 模型里变成了可见 stall。

[rvv-war-renamed.s](../llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/rvv-war-renamed.s) 把中间 writer 改成 `v20`，对应 RA 避免复用后的形状。打开 WAR 模型后总周期仍是 6，`Total hazards: 0`。这就是验证 RA 优化效果的最小闭环：同样的指令资源环境下，物理寄存器命名不同，WAR stall 消失。

[scalar-no-war.s](../llvm/test/tools/llvm-mca/RISCV/RVVWarDemo/scalar-no-war.s) 证明模型只作用于 RVV physical register。标量 GPR 的 WAR-like 读写不会进入 `RVV WAR Hazard` 统计。

### Report utility

轻量脚本在 [riscv-rvv-war-report.py](../llvm/utils/riscv-rvv-war-report.py)。它从汇编文本中寻找一个可配置窗口内的 RVV read -> later write pair：

```bash
llvm/utils/riscv-rvv-war-report.py --window=8 input.s
```

输出类似：

```text
RVV WAR pairs: 1
Window: 8
Samples:
  line 1 reads v8 -> line 2 writes v8
    read:  vadd.vv v10, v8, v12
    write: vadd.vv v8, v14, v15
```

这个工具不是调度模型，也不替代 `llvm-mca`。它的用途是快速从 RA 输出或手写汇编里定位“哪些地方出现了短距离复用”，再把代表性片段交给 `llvm-mca` 看周期和 blocked issue report。当前脚本同样保持聚焦，只覆盖普通文本汇编里的直接 RVV register pair。

## 如何解读报告

RA 测试回答的是：“打开 `-riscv-rvv-avoid-recent-vreg-reuse` 后，allocator 是否真的改选了另一个合法 RVV 物理寄存器？”看 `FileCheck` 里的 `$vN` 即可。

`llvm-mca` 测试回答的是：“这个物理寄存器命名差异是否能转化为 issue stall 差异？”看三项：

- `Total Cycles`：hazard case 比 renamed case 更高。
- `Total hazards`：唯一的 writer/reader/register 组合数量。
- `Blocked issue events/cycles`：scheduler 实际因为 custom hazard 推迟 issue 的次数和周期。

Report utility 回答的是：“汇编文本里哪里有短距离 RVV WAR pair？”它适合做定位和样本提取，不适合做性能结论。

把三者连起来看，才是这个 demo 的完整证据链：

1. MIR RA 测试证明新 allocator option 改变 RVV physical register reuse。
2. `llvm-mca` hazard/renamed 测试证明这种 reuse 会被 WAR issue 模型计为 stall，而重命名后 stall 消失。
3. report utility 帮助从更大输出中提取可解释的 WAR pair 样本。

## 非目标

这个分支不包含 `realtest/`，也不提交 workload artifact。它也不继续覆盖旧实验分支里那些非常用指令的 MCA/report corner case。当前目标是把核心机制讲清楚，并用小而可读的测试用例证明：RA 的物理寄存器选择可以减少 `llvm-mca` 中可见的 RVV WAR issue hazard。
