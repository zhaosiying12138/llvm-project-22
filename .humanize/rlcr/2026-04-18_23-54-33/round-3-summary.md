# Round 3 Summary

## Work Completed
- Removed default MC exposure of unsupported FP/vector CSR aliases: `fflags`, `frm`, `fcsr`, `vstart`, `vxsat`, `vxrm`, `vcsr`, `vl`, `vtype`, and `vlenb` are no longer declared as YSX system-register operands.
- Added negative MC coverage for those CSR names and for raw 16-bit `.insn` emission, which now rejects compressed instruction length for YSX.
- Deleted the copied compressed/vector instruction-format TD includes from `YSXInstrInfo.td` and removed `YSXInstrFormatsC.td` / `YSXInstrFormatsV.td`.
- Pruned `YSXRegisterInfo.td` to remove FP register files, vector registers, vector CSR pseudo-registers, and FP/vector register classes.
- Simplified calling-convention TD/C++ to GPR-only LP64/ILP32 handling and removed FP/vector/RVE callee-saved register variants.
- Replaced the inherited FRM/FFLAGS CSR insertion pass with a no-op pass and changed FP environment lowering to avoid deleted custom CSR ISD nodes.
- Removed or neutralized selected FP/vector references in MC code emitter, disassembler, asm parser, register info, instruction info, and lowering so the pruned register tables compile.
- Preserved `llvm/lib/Target/RISCV` unchanged.

## Files Changed
- YSX backend sources under `llvm/lib/Target/YuShuXin`, including AsmParser, Disassembler, MCTargetDesc, CallingConv, FrameLowering, ISelLowering, InsertReadWriteCSR, InstrInfo, RegisterInfo, and system operands.
- Deleted YSX TD files: `llvm/lib/Target/YuShuXin/YSXInstrFormatsC.td` and `llvm/lib/Target/YuShuXin/YSXInstrFormatsV.td`.
- Updated negative coverage in `llvm/test/MC/YSX/unsupported-features.s`.
- Updated RLCR tracking files in `.humanize/rlcr/2026-04-18_23-54-33/`.

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc` in `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX/unsupported-features.s` from the YSX-only build directory.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` from the YSX-only build directory, with 130 discovered tests.
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## Remaining Items
- AC-3 is not complete. `YSXFeatures.td`, `YSXFrameLowering.*`, `YSXInstrInfo.cpp`, `YSXSubtarget.*`, `YSXISelDAGToDAG.*`, and MC helper files still contain copied feature/vector/FP naming or dead support surfaces that need another pruning slice.
- Some unsupported FP/vector logic is still present behind disabled or unreachable paths. This round focused on removing externally visible CSR leakage, TD register classes, C/V format includes, and enough C++ references to keep the backend buildable.
- The YSX disassembler still reports unused decoder-helper warnings inherited from removed unsupported surfaces.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The BitLesson selector returned the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output again, so it was treated as no usable lesson selection and no lesson file changes were made.
