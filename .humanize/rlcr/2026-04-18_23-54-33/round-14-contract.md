# Round 14 Contract

## Mainline Objective

Delete YSX scalable-vector frame-lowering support and its per-function YSXVec
frame state so the rv64ima backend no longer advertises or maintains vector
stack objects.

## Target ACs

- AC-3: remove copied support code for features outside rv64ima.
- AC-4: preserve the focused YSX regression and negative coverage after the
  frame deletion slice.

## Blocking Issues

- `YSXFrameLowering.*` still supports `TargetStackID::ScalableVector`, scalable
  CFA expressions, YSXVec stack padding, scalable vector callee-save handling,
  vector spill stack IDs, and YSXVec stack probing. These block this round
  because they are the selected source-deletion objective.
- `YSXMachineFunctionInfo.h` still stores YSXVec stack size, alignment, and
  padding fields that exist only for the frame paths being deleted.

## Queued Out Of Scope

- Full deletion of `YSX_UNSUPPORTED_ISD` and the broad vector/FP/vendor callers
  in `YSXISelLowering.*` remains queued for a dedicated lowering round.
- BaseInfo/TableGen vector TSFlags, operand kinds, generated vector pseudo
  tables, RegisterInfo vector metadata, Subtarget VLEN helpers, and
  Disassembler FP/vector helpers remain queued AC-3 deletion work unless they
  are needed to make this frame slice compile.
- Stale copied CodeGen check-prefix trimming and CPU/tune target-attribute
  policy remain queued and must not replace this round objective.

## Success Criteria

- `YSXFrameLowering.h` no longer declares YSXVec frame helpers or
  `getStackIDForScalableVectors()`.
- `YSXFrameLowering.cpp` no longer references `TargetStackID::ScalableVector`,
  `getYSXVec*`, `setYSXVec*`, `YSXVecCalleeSaved`, scalable CFA expression
  construction, YSXVec stack probing, or vector spill stack IDs.
- `YSXMachineFunctionInfo.h` no longer stores YSXVec stack size/alignment/padding
  fields or accessors.
- Supported rv64ima stack/frame code still builds and existing stack-oriented
  YSX tests continue to pass.
- `git diff --check` is clean.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- YSX-only build passes for `LLVMYSXCodeGen llvm-mc clang llc`.
- Combined RISCV+YSX build passes for
  `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- Focused YSX lit suites pass:
  `llvm/test/MC/YSX`, `llvm/test/CodeGen/YSX`,
  `clang/test/Driver/YSX`, and `clang/test/CodeGen/YSX`.
- YSX and RISCV smoke compiles still produce ELF64 RISC-V soft-float objects.
- Negative probes still reject YSX `+v`, `rv64imaf`, scalable vector IR, and
  direct `llvm.riscv.vsetvli`.
