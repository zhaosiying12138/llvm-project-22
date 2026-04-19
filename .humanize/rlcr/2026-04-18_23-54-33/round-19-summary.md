# Round 19 Summary

## Work Completed
- Removed the residual AC-3 source metadata called out by Round 18 review: unused floating-point pseudo/constant definitions, the stale GlobalISel CSE hook/include, and the stale vector stack comment.
- Trimmed YSX-owned tests so inactive copied RV32, ILP32/LP64F/LP64D/LP64E, Zb*, XThead/XVT, Zicond, RV64IA-TSO, WMO, NOZACAS, and related unsupported check-prefix blocks are gone.
- Preserved executed negative unsupported-feature tests for `+zbb`, vendor spellings, RV32 arch strings, compressed/vector/FP features, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- Fixed generated-test fallout by simplifying active RUN prefixes and deleting unused FileCheck placeholder blocks.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`, `YSXInstrInfo.h`, `YSXTargetMachine.cpp`, and `YSXFrameLowering.cpp` for the residual source metadata cleanup.
- `llvm/test/CodeGen/YSX/*.ll` and selected `llvm/test/MC/YSX/*.s` files for stale unsupported check-prefix removal and regenerated active rv64ima checks.
- RLCR files for Round 18 review, Round 19 prompt/contract/summary, and the mutable goal tracker/state updates.

## Validation
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` (`132` tests).
- PASS: YSX clang smoke compile produced `ELF 64-bit LSB relocatable, UCB RISC-V, soft-float ABI`; combined RISCV smoke compile also produced an ELF64 RISC-V soft-float object.
- PASS: negative probes rejected `+v`, `+32bit`, `+zca`, vendor `+xventanacondops`, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: source/test scans no longer find the reviewed stale unsupported markers except executed negative unsupported-feature tests.

## Remaining Items
- CPU/tune target-attribute warn-and-ignore policy remains queued because no feature leakage was observed.
- Current backend line counts: RISCV `136,068`; YSX `27,237`; reduction `108,831` lines, about `79.98%`.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `.humanize/bitlesson.md` has no concrete entries; the selector returned the placeholder `NONE` result for this round's tasks.
