# Round 14 Summary

## Work Completed
- Deleted the scalable-vector frame-lowering slice selected by
  `round-14-contract.md`.
- Removed YSXVec stack size/alignment/padding state from
  `YSXMachineFunctionInfo.h`.
- Removed scalable-vector stack-ID support, scalable CFA expression helpers,
  YSXVec callee-save filtering/CFI, vector spill stack-ID assignment, YSXVec
  stack probing, and YSXVec frame scavenging helpers from `YSXFrameLowering.*`.
- Simplified frame size calculations back to `MachineFrameInfo::getStackSize()`
  for the rv64ima-only frame path.
- Current line counts: original RISCV backend `136,068` lines; current YSX
  backend `51,831` lines; total reduction from copied RISCV is `84,237` lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-14-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-14-summary.md`
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.h`
- `llvm/lib/Target/YuShuXin/YSXMachineFunctionInfo.h`

## Validation
- `rg "YSXVec|getStackSizeWithYSXVecPadding|ScalableVector|PROBED_STACKALLOC_YSXVec|StackIDForScalable|StackOffset::getScalable|getYSXVec|setYSXVec|VLENB|vlenb" llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp llvm/lib/Target/YuShuXin/YSXFrameLowering.h llvm/lib/Target/YuShuXin/YSXMachineFunctionInfo.h` produced no matches.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 discovered tests.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- YSX smoke compile produced `/tmp/ysx-smoke.o: ELF 64-bit LSB relocatable, UCB RISC-V, soft-float ABI`.
- RISCV smoke compile produced `/tmp/riscv-smoke.o: ELF 64-bit LSB relocatable, UCB RISC-V, soft-float ABI`.
- Negative probes rejected YSX `+v`, `rv64imaf`, scalable vector IR, and direct
  `llvm.riscv.vsetvli` with deterministic diagnostics.

## Remaining Items
- AC-3 remains active for the remaining copied unsupported surfaces:
  `YSXSelectionDAGInfo.h`, broad `YSXISelLowering.*` vector/RISCVVType/YSXVec
  lowering, BaseInfo/TableGen vector/FP/RV32 metadata, RegisterInfo/Subtarget
  leftovers, generated vector pseudo surfaces, and Disassembler FP/vector
  helpers.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: No reusable failure lesson was discovered; this was a successful
  source-deletion slice.
