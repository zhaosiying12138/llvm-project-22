# Round 17 Summary

## Work Completed
- Deleted the reviewed residual FP/vector source surface from YSX Lowering,
  DAG selection, BaseInfo, TableGen formats, generated pseudo plumbing,
  RegisterInfo, Subtarget, MachineFunctionInfo, AsmPrinter, AsmParser,
  InstPrinter, MCCodeEmitter, ELF/target streamers, and Disassembler.
- Removed `.variant_cc` handling, FP/vector parser/printer/decode hooks,
  vector TSFlags and operand metadata, vector pseudo tables, VLEN/YSXVec
  helpers, vector-call state, and the dead FP constant promotion pass.
- Removed unused Disassembler decoder helpers exposed by the new generated
  table and cleaned the final `YSXMatInt.cpp` unused-variable warning.
- Current source-size snapshot: RISCV backend `136,068` lines; YSX backend
  `29,861` lines; net reduction `106,207` lines, about `78.1%`.

## Files Changed
- Core YSX backend only under `llvm/lib/Target/YuShuXin`, plus RLCR tracker
  files for Round 17.
- No changes to `llvm/lib/Target/RISCV`.

## Validation
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` (`132` tests)
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: smoke compiles for `ysx64-unknown-elf` and `riscv64-unknown-elf -march=rv64ima -mabi=lp64` both produce ELF64 RISC-V soft-float relocatables.
- PASS: negative probes reject `llvm-mc -triple=ysx64 -mattr=+v`, `clang --target=ysx64-unknown-elf -march=rv64imaf`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: reviewed FP/vector/generated-surface scan has no matches for `YSXVec`, `VInstructions`, `VLEN`, `FRMARG`, `VEC_POLICY`, `VPseudos`, `variant_cc`, `VType`, `GPRAsFPR`, `VMask`, `FPRX`, `ConstantFPSDNode`, or `APFloat`.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## Remaining Items
- AC-3 remains active for residual RV32 mode branches, ILP32 compatibility
  metadata, and vendor/removed-extension false-query callsites that are still
  visible in source scans.
- Stale inactive YSX CodeGen check-prefix trimming remains queued until the
  RV32/vendor source-surface slice is complete.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: no reusable failure pattern was found; the selector returned only its
  placeholder output for this task.
