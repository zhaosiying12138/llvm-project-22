# Round 22 Contract

## Mainline Objective
Delete the remaining dead YuShuXin DAG selector scaffolding for removed non-rv64ima source paths without changing RISCV.

## Target ACs
- AC-3: YSX implementation contains no support code for removed features.
- AC-4: YSX-owned tests and validation remain passing after the pruning slice.

## Blocking Issues
- `AddrRegRegScale` and `AddrRegZextRegScale` TableGen complex-pattern classes and their selector declarations/implementations remain even though YSX has no retained scaled register-register addressing form.
- `selectSHXADDOp` and `selectSHXADD_UWOp` remain as copied Zba-style selector helpers for removed SHXADD patterns.
- `selectSF_VC_X_SE` and `performCombineVMergeAndVOps` remain as copied SiFive/vector selector hooks with no rv64ima role.

## Queued Out Of Scope
- Immutable goal-tracker AC-list drift remains queued because `docs/plan.md` is the review source of truth and the immutable tracker section must not be edited.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because current probes show no unsupported feature leakage and this round is source pruning.

## Success Criteria
- `rg "AddrRegRegScale|AddrRegZextRegScale|selectSHXADD|SHXADD|selectSF_VC_X_SE|performCombineVMergeAndVOps" llvm/lib/Target/YuShuXin` has no matches.
- YSX-only Ninja build of `LLVMYSXCodeGen llvm-mc clang llc` passes.
- Combined RISCV+YSX Ninja build of `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passes.
- Focused YSX LLVM/Clang lit suites pass.
- Retained rv64ima smoke tests and unsupported-feature probes pass.
- `git diff --check` passes and `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## BitLesson
- Selector result: `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, treated as no applicable lesson.
