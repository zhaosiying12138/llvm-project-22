# 复现 RVV writeback-bank scheduler 的 integrated 示例

本文说明如何在本分支中执行 `riscv-wb-bank-scheduler` pass，复现
`reorder + PostRA rename + insert NOP` 协同修复 RVV VRF writeback bank conflict
的效果，并用 `llvm-mca` 检查 raw/repaired 两个汇编序列。

当前实现复用 LLVM 的 MachineScheduler 框架：pass 用 `ScheduleDAGMI` 构造并维护
调度 DAG，用 `PostGenericScheduler` 派生的 strategy 重写 top-down `pickNode()`，
只在候选选择处加入 writeback-bank-aware 检查、PostRA rename fallback 和显式
NOP stall。writeback latency 不再由 pass 手写 opcode 表决定，而是从
`riscv-wb-bank-poc` 的 `SchedMachineModel` 通过 `TargetSchedModel` 读取。因此
下面所有执行 pass 的 `llc` 命令都带上 `-mcpu=riscv-wb-bank-poc`，确保 `llc`
和 `llvm-mca` 使用同一套 latency/resource 模型。

以下命令均假设当前目录是仓库根目录：

```bash
cd /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-riscv-wb-bank-scheduler-poc
```

## 1. 构建需要的 LLVM 工具

如果还没有本分支对应的构建目录，先配置并构建 `llc`、`llvm-mca`、
`FileCheck`、`not` 和 `llvm-lit`：

```bash
cmake -S llvm -B ../build_riscv_wb_bank_scheduler_poc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=RISCV \
  -DLLVM_ENABLE_PROJECTS= \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF

ninja -C ../build_riscv_wb_bank_scheduler_poc llc llvm-mca FileCheck not llvm-lit
```

后续命令使用这个变量简化路径：

```bash
BUILD=../build_riscv_wb_bank_scheduler_poc
```

## 2. 执行 pass 并查看 integrated block

目标 MIR 用例是：

```text
llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir
```

其中 `integrated_reorder_rename_nop_rvv` 的原始顺序是：

```text
I1  vfmacc.vf v5,  f0, v8      L=4, WB C4, Bank 0
I2  vfadd.vf  v7,  v10, f1     L=3, WB C4, Bank 0
I3  vadd.vv   v6,  v12, v13    L=2, WB C4, Bank 1
I4  vadd.vv   v9,  v16, v18    L=2, WB C5, Bank 0
I5  vmv.v.v   v11, v20         L=1, WB C5, Bank 0
I6  vmv.v.v   v14, v9          L=1, WB C6, Bank 1
```

输入问题是两个同 bank 写回冲突：

```text
C4 Bank 0: I1/v5 和 I2/v7 冲突
C5 Bank 0: I4/v9 和 I5/v11 冲突
```

执行 pass 并只截取 integrated block：

```bash
$BUILD/bin/llc -mtriple=riscv64 -mcpu=riscv-wb-bank-poc -mattr=+v,+f \
  -run-pass=riscv-wb-bank-scheduler \
  -riscv-wb-bank-scheduler \
  -simplify-mir \
  llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir \
  -o - \
  | sed -n '/name: *integrated_reorder_rename_nop_rvv/,/^\.\.\./p'
```

关键输出应包含：

```text
renamable $v5 = PseudoVFMACC_VFPR32_M1_E32 ...
renamable $v6 = PseudoVADD_VV_M1 ...
renamable $v7 = PseudoVFADD_VFPR32_M1_E32 ...
$v0 = PseudoVADD_VV_M1 ...
$x0 = ADDI $x0, 0
$v11 = PseudoVMV_V_V_M1 ...
$v14 = PseudoVMV_V_V_M1 ... $v0 ...
PseudoRET implicit $v14
```

这段输出对应三步修复：

```text
1. Reorder:
   I3 被移动到 I2 前面，I2 的 issue 从 C1 变成 C2，
   因此 I2 的写回从 C4 移到 C5，消除 C4 Bank 0 冲突。

2. PostRA rename:
   I4 的目的寄存器从 v9 改成 v0。
   v9 是奇数 VR，属于 Bank 0；v0 是偶数 VR，属于 Bank 1。
   同时 I6 的源操作数从 v9 改成 v0，保持数据依赖语义。
   因此 C5 变成 I2 写 Bank 0、I4' 写 Bank 1。

3. Insert NOP:
   I5 仍会写 Bank 0。此时不能安全地继续 reorder/rename，
   pass 插入 `ADDI x0, x0, 0` 作为 NOP，把 I5 issue 从 C4 推到 C5，
   让 I5 的写回移动到 C6。
```

最终 pass 模型中的事件表是：

```text
Issue : C0 I1, C1 I3, C2 I2, C3 I4', C4 NOP, C5 I5, C6 I6
WB B0 : C4 I1/v5, C5 I2/v7, C6 I5/v11
WB B1 : C3 I3/v6, C5 I4'/v0, C7 I6/v14
```

每个 `(writeback cycle, VRF bank)` 只出现一次。

## 3. 用 FileCheck 验证 pass 输出

直接运行 MIR 回归测试：

```bash
$BUILD/bin/llc -mtriple=riscv64 -mcpu=riscv-wb-bank-poc -mattr=+v,+f \
  -run-pass=riscv-wb-bank-scheduler \
  -riscv-wb-bank-scheduler \
  -simplify-mir \
  llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir \
  -o - \
  | $BUILD/bin/FileCheck \
      llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir
```

该命令通过时，说明 `integrated_reorder_rename_nop_rvv` 的输出顺序、`v9 -> v0`
rename、`I6` use 改写和 NOP 插入都匹配测试中的 `CHECK`。

也可以执行 lit：

```bash
$BUILD/bin/llvm-lit llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir
```

如果要把这个 MIR 用例继续编译成汇编，不要使用 `-run-pass`。`-run-pass`
只运行 pass 并打印 MIR；要从指定 pass 前恢复后续 codegen，使用
`-start-before=riscv-wb-bank-scheduler`：

```bash
$BUILD/bin/llc -mtriple=riscv64 -mcpu=riscv-wb-bank-poc -mattr=+v,+f \
  -start-before=riscv-wb-bank-scheduler \
  -riscv-wb-bank-scheduler \
  llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir \
  -o /tmp/rvv-wb-bank-repaired.s

sed -n '/integrated_reorder_rename_nop_rvv:/,/^$/p' \
  /tmp/rvv-wb-bank-repaired.s
```

## 4. 用 llvm-mca 复现 raw conflict

raw 输入文件是：

```text
llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s
```

内容等价于 pass 修复前的顺序：

```text
vfmacc.vf v5, ft0, v8
vfadd.vf  v7, v10, ft1
vadd.vv   v6, v12, v13
vadd.vv   v9, v16, v18
vmv.v.v   v11, v20
vmv.v.v   v14, v9
```

执行：

```bash
$BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s
```

预期失败信息的关键部分是：

```text
LLVM ERROR: RISC-V banked VRF writeback conflict:
bank 0 has multiple vector writes at static writeback cycle 4;
instruction #1 writes v7, conflicting with instruction #0 writing v5
```

含义是：在 `llvm-mca` 的静态检查模型中，I1 和 I2 都在 C4 写 Bank 0。
这正是 raw block 中第一个必须修复的 writeback-bank conflict。

可用测试形式验证：

```bash
$BUILD/bin/not $BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s \
  2>&1 \
  | $BUILD/bin/FileCheck \
      llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s
```

## 5. 用 llvm-mca 验证 repaired 序列

repaired 输入文件是：

```text
llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s
```

内容对应 pass 输出后的顺序：

```text
vfmacc.vf v5, ft0, v8
vadd.vv   v6, v12, v13
vfadd.vf  v7, v10, ft1
vadd.vv   v0, v16, v18
addi      zero, zero, 0
vmv.v.v   v11, v20
vmv.v.v   v14, v0
```

执行：

```bash
$BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s
```

关键输出是：

```text
Iterations:        1
Instructions:      7
Total Cycles:      9
...
 1      4     1.00                        vfmacc.vf
 1      2     1.00                        vadd.vv
 1      3     1.00                        vfadd.vf
 1      2     1.00                        vadd.vv
 1      1     1.00                        nop
 1      1     1.00                        vmv.v.v
...
Resources:
[0]   - WBPOC_EXEC
```

并且不会出现：

```text
RISC-V banked VRF writeback conflict
```

可用测试形式验证：

```bash
$BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s \
  | $BUILD/bin/FileCheck \
      llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s
```

这里的 `riscv-wb-bank-poc` 调度模型是 PoC 专用模型：只有一个流水化执行资源
`WBPOC_EXEC`，并把 `vmv/vadd/vfadd/vfmacc` 的 latency 分别设为 `1/2/3/4`。
文档图中的 `C0..C7` 是 pass 的静态 issue/writeback 事件表；
`llvm-mca` 的 `Total Cycles: 9` 是 mca 对同一七条指令序列的块级统计。
两者使用同一组固定指令 latency，但一个用于解释 bank hazard，另一个用于
工具级吞吐/完成周期汇总。

也可以直接把 `llc` 生成的 integrated block 喂给 `llvm-mca`。建议去掉 `ret`，
这样 mca 输出只覆盖要分析的七条 RVV/NOP 指令：

```bash
$BUILD/bin/llc -mtriple=riscv64 -mcpu=riscv-wb-bank-poc -mattr=+v,+f \
  -start-before=riscv-wb-bank-scheduler \
  -riscv-wb-bank-scheduler \
  llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir \
  -o - \
  | sed -n '/integrated_reorder_rename_nop_rvv:/,/^$/p' \
  | sed '/^[[:space:]]*ret$/d' \
  | $BUILD/bin/llvm-mca \
      -mtriple=riscv64 \
      -mcpu=riscv-wb-bank-poc \
      --dispatch=1 \
      -iterations=1
```

如果不删除 `ret`，`llvm-mca` 会报告 8 条指令、`Total Cycles: 10`，并打印
`found a return instruction` warning；这是 mca 提醒它会忽略 PC 更新，不影响
前面七条指令的 bank-conflict 检查结论。

## 6. 一键检查本示例

```bash
$BUILD/bin/llc -mtriple=riscv64 -mcpu=riscv-wb-bank-poc -mattr=+v,+f \
  -run-pass=riscv-wb-bank-scheduler \
  -riscv-wb-bank-scheduler \
  -simplify-mir \
  llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir \
  -o - \
  | $BUILD/bin/FileCheck \
      llvm/test/CodeGen/RISCV/rvv-writeback-bank-scheduler.mir

$BUILD/bin/not $BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s \
  2>&1 \
  | $BUILD/bin/FileCheck \
      llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-conflict.s

$BUILD/bin/llvm-mca \
  -mtriple=riscv64 \
  -mcpu=riscv-wb-bank-poc \
  --dispatch=1 \
  -iterations=1 \
  < llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s \
  | $BUILD/bin/FileCheck \
      llvm/test/tools/llvm-mca/RISCV/BankedWriteback/integrated-repaired.s
```
