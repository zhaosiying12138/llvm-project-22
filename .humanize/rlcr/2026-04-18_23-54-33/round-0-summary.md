# Round 0 Summary

## What Was Implemented

- Created a new standalone `YSX` backend in `llvm/lib/Target/YuShuXin` by
  copying the RISCV backend, renaming backend-visible symbols/files to `YSX`,
  and wiring new LLVM target libraries/init entrypoints.
- Added LLVM and Clang target plumbing for `ysx64`, including triple parsing,
  driver/codegen routing, and YSX-specific option names to avoid collisions
  with RISCV in combined static builds.
- Pruned YSX to an `rv64ima`-only surface by removing GISel and deleting
  non-required vector/profile/scheduler/auxiliary files while keeping the
  directory layout aligned with RISCV.
- Added YSX-owned LLVM and Clang regression tests by copying the
  `rv64ima`-applicable RISCV subset and rewriting target spellings/options.
- Fixed standalone and co-build integration issues, including generated target
  config emission, duplicate hidden options, duplicate helper symbols, and
  TableGen fusion record collisions between RISCV and YSX.

## Files Changed

- Modified LLVM/Clang integration:
  - `llvm/CMakeLists.txt`
  - `llvm/lib/Target/CMakeLists.txt`
  - `llvm/include/llvm/TargetParser/Triple.h`
  - `llvm/lib/TargetParser/Triple.cpp`
  - `llvm/lib/TargetParser/TargetDataLayout.cpp`
  - `clang/lib/Basic/Targets.cpp`
  - `clang/lib/Driver/Driver.cpp`
  - `clang/lib/Driver/ToolChains/Arch/RISCV.cpp`
  - `clang/lib/Driver/ToolChains/Clang.cpp`
  - `clang/lib/Driver/ToolChains/CommonArgs.cpp`
  - `llvm/utils/UpdateTestChecks/asm.py`
- Added new target parser headers:
  - `llvm/include/llvm/TargetParser/YSXISAInfo.h`
  - `llvm/include/llvm/TargetParser/YSXTargetParser.h`
- Added new backend tree:
  - `llvm/lib/Target/YuShuXin/`
- Added new test trees:
  - `llvm/test/CodeGen/YSX/`
  - `llvm/test/MC/YSX/`
  - `clang/test/CodeGen/YSX/`
  - `clang/test/Driver/YSX/`
- Deleted unused YSX GISel/vector/profile/scheduler files that were not needed
  for the retained `rv64ima` implementation.

## Validation

- YSX-only configure/build in
  `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm` succeeds for
  `llvm-mc`, `llc`, `clang`, `lld`, `opt`, and supporting lit tools.
- RISCV+YSX configure/build in
  `/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm` succeeds for
  `llvm-mc`, `llc`, and `clang` with both `libLLVMYSXCodeGen.a` and
  `libLLVMRISCVCodeGen.a` present.
- YSX codegen/MC/driver smoke tests pass:
  - `llc -mtriple=ysx64-unknown-elf -filetype=asm`
  - `llvm-mc -triple=ysx64-unknown-elf -filetype=obj`
  - `clang --target=ysx64-unknown-elf -S -x c /dev/null -o -`
- Combined-build coexistence smoke tests pass for both `ysx64-unknown-elf` and
  `riscv64-unknown-elf` using `llc` and `llvm-mc`.
- Full YSX regression suite passes:
  - `llvm/test/CodeGen/YSX`
  - `llvm/test/MC/YSX`
  - `clang/test/CodeGen/YSX`
  - `clang/test/Driver/YSX`
  - Total discovered tests: 129, failures: 0.

## Remaining Items

- The backend is materially smaller and functionally standalone, but it still
  exceeds the historical rough size target of ~30k LOC. Further size-focused
  refactoring is deferred because all current acceptance criteria are passing.
- RLCR stop-gate review still needs to run on the completed round artifacts.

## BitLesson Delta

Action: none
Lesson ID(s): NONE
Notes: No BitLesson update was needed for this round.
