# Round 7 Summary

## Work Completed
- Added YSX-owned Clang feature handling in `YSX64TargetInfo`: feature-map
  initialization, target-feature handling, target-attribute parsing, and
  feature-name validation now use a YSX whitelist instead of inherited RISCV
  parsing.
- Kept the accepted YSX frontend feature set to `rv64ima` plus internal
  `64bit`, optional `relax`, and reserved-GPR target features. User target
  attributes reject unsupported features and do not expose internal `+64bit` as
  an arch extension.
- Rejected unsupported FP/vector inline asm constraints for YSX by overriding
  constraint validation/conversion for the copied RISCV target info subclass.
- Made unsupported YSX target attributes diagnose as frontend errors before IR
  emission instead of being ignored as generic target-attribute warnings or
  reaching the backend.
- Expanded YSX Clang driver tests for `target("arch=+v")`,
  `target("arch=rv64imaf")`, `target("arch=+64bit")`, explicit cc1
  `-target-feature +v`, and unsupported `"f"`/`"vr"` asm constraints.
- Updated the mutable goal tracker to record the Round 7 local fix while
  keeping AC-3 backend source pruning active.

## Files Changed
- `clang/lib/Basic/Targets/RISCV.h`
- `clang/lib/Basic/Targets/RISCV.cpp`
- `clang/lib/Sema/SemaDeclAttr.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `.humanize/rlcr/2026-04-18_23-54-33/round-7-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja clang LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: default and explicit `-march=rv64ima` YSX Clang smoke compiles produce
  ELF64 RISC-V soft-float relocatable objects.
- PASS: `target("arch=+v")`, `target("arch=rv64imaf")`, and
  `target("arch=+64bit")` fail before IR emission with
  `invalid feature combination: YSX only supports the rv64ima ISA`.
- PASS: explicit cc1 `-target-feature +v` fails before predefines and does not
  define `__riscv_vector` or `__riscv_v*` macros.
- PASS: YSX rejects `"f"` and `"vr"` inline asm constraints in Clang
  syntax/semantic checks.
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in
  `build_ysx_riscv_host_llvm`.
- PASS: combined build smoke compiles for both `ysx64-unknown-elf` and
  `riscv64-unknown-elf` produce ELF64 RISC-V relocatable objects.
- PASS: RISCV still defines `__riscv_v_intrinsic`; YSX still does not define
  `__riscv_v_intrinsic` or `__riscv_vector`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Line-count status: `llvm/lib/Target/RISCV` is 136,068 lines across 188
  files; `llvm/lib/Target/YuShuXin` is 60,045 lines across 86 files. Round 7
  did not reduce backend lines because it targeted Clang frontend leaks.

## Remaining Items
- AC-3 remains active: YSX backend sources still retain vector/FP/vendor/RV32/
  compressed lowering, selection, pass declarations, TSFlags, frame helpers,
  false-return compatibility APIs, and vector helper forwarding that must be
  deleted in later pruning rounds.
- Stale copied YSX CodeGen check-prefix cleanup remains queued until the backend
  source-pruning pass removes the corresponding implementation surfaces.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for the Round 7
  contract/hardening, test, and validation tasks, so no actionable lesson was
  selected or added.
