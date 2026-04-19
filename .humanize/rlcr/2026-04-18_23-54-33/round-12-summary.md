# Round 12 Summary

## Work Completed
- Added a YSX module-level unsupported-IR guard before codegen so scalable
  vector IR and direct non-masked-atomic `llvm.riscv.*` intrinsics fail with
  deterministic diagnostics instead of reaching SelectionDAG abort paths.
- Pruned copied RVV/VP CodeGenPrepare transforms while preserving the scalar
  rv64ima `and` preparation transform.
- Deleted disabled compatibility blocks from YSX lowering, DAG selection, and
  frame lowering.
- Pruned YSX DAG-to-DAG vector selector helpers that no generated YSX pattern
  references: segment load/store selection, VSETVLI stubs, VL/splat selector
  helpers, and vector pseudo peepholes.
- Removed the unreachable RVV/SF target-memory intrinsic metadata table from
  `YSXISelLowering.cpp`; masked atomics remain supported.
- Added negative `llc` coverage for scalable vector IR and direct
  `llvm.riscv.vsetvli`.
- Refreshed stale YSX CodeGen checks that now reflect the rv64ima-only YSX
  behavior and early scalable-vector rejection.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSX.h`
- `llvm/lib/Target/YuShuXin/YSXCodeGenPrepare.cpp`
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXTargetMachine.cpp`
- `llvm/test/CodeGen/YSX/unsupported-riscv-vector-intrinsic.ll`
- `llvm/test/CodeGen/YSX/unsupported-scalable-vector-ir.ll`
- `llvm/test/CodeGen/YSX/div_minsize.ll`
- `llvm/test/CodeGen/YSX/inline-asm-invalid.ll`
- `llvm/test/CodeGen/YSX/stack-slot-size.ll`

## Validation
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: YSX clang smoke compile produced an ELF64 RISC-V soft-float relocatable.
- PASS: RISCV clang smoke compile with `-march=rv64ima -mabi=lp64` produced an
  ELF64 RISC-V soft-float relocatable in the combined build.
- PASS: `llvm-mc -triple=ysx64 -mattr=+v /dev/null` rejects unsupported vector.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64imaf -c` rejects
  unsupported FP.
- PASS: `llc -mtriple=ysx64-unknown-elf` rejects scalable vector IR without
  crash.
- PASS: `llc -mtriple=ysx64-unknown-elf` rejects direct
  `llvm.riscv.vsetvli` IR without crash.
- PASS: `clang --target=ysx64-unknown-elf -###` emits only rv64ima positive
  target features plus relax.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Remaining Items
- `YSXSelectionDAGInfo.h` and large parts of `YSXISelLowering.cpp` still carry
  copied vector custom ISD names/lowering helpers. The new guard blocks direct
  unsupported vector IR from aborting, but the source surface still needs a
  deeper lowering/custom-ISD deletion round.
- BaseInfo/TableGen/RegisterInfo/Frame/Disassembler vector metadata and
  generated pseudo surfaces remain queued AC-3 work.
- Backend line count after this round is 53,216 lines versus 136,068 original
  RISCV backend lines.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned the placeholder response for this round;
  no actionable lesson was selected or added.
