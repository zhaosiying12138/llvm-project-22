# Round 6 Summary

## Work Completed
- Added `YSX64TargetInfo` and routed `ysx64` through it instead of instantiating `RISCV64TargetInfo`.
- Disabled the inherited RISCV vector frontend surface for YSX: no `__riscv_v_intrinsic`, no `__riscv_vector`, no RVV target types, and no RVV builtin shard.
- Specialized YSX driver feature emission so default `ysx64` emits only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax` instead of the RISCV disabled extension universe.
- Replaced the `YSXISAInfo = RISCVISAInfo` alias with a small YSX ISA parser for the retained rv64ima surface and replaced the direct `YSXVType = RISCVVType` namespace alias with explicit compatibility forwarding for the later backend vector-deletion round.
- Expanded YSX driver tests for the frontend surface, unsupported `rv64imav`, missing vector macros, and unavailable RVV type/builtin names.
- Updated the mutable goal tracker to mark the Clang/TargetParser frontend-surface leak resolved and keep backend AC-3 pruning active.

## Files Changed
- `clang/lib/Basic/Targets.cpp`
- `clang/lib/Basic/Targets/RISCV.h`
- `clang/lib/Basic/Targets/RISCV.cpp`
- `clang/lib/Driver/ToolChains/Arch/RISCV.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `llvm/include/llvm/TargetParser/YSXISAInfo.h`
- `.humanize/rlcr/2026-04-18_23-54-33/round-6-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja clang LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `clang --target=ysx64-unknown-elf -### -c -x c /dev/null` emits only the YSX positive feature list plus `+relax`.
- PASS: `clang --target=ysx64-unknown-elf -dM -E -x c /dev/null` no longer defines `__riscv_v_intrinsic` or `__riscv_vector` and still defines rv64 I/M/A/Zmmul/Zaamo/Zalrsc macros.
- PASS: RVV probes for `__rvv_int8m1_t` and `__builtin_rvv_vsetvli` fail for YSX.
- PASS: default and explicit `-march=rv64ima` YSX Clang smoke compiles produce ELF64 RISC-V relocatable objects in YSX-only and combined builds.
- PASS: `-march=rv64imaf`, `-march=rv64imac`, and `-march=rv64imav` reject with `YuShuXin only supports -march=rv64ima`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `build_ysx_riscv_host_llvm`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: RISCV macro smoke still defines `__riscv_v_intrinsic` for `--target=riscv64-unknown-elf`, confirming the YSX condition did not remove RISCV's vector macro.

## Remaining Items
- AC-3 remains active for backend source pruning: vector/FP/vendor/RV32/compressed lowering, DAG selection, pass declarations, TSFlags, frame helpers, and false-return compatibility APIs remain in YSX backend sources.
- `YSXVType` still forwards to RISCV vector type helpers to keep the current backend compiling; deleting those users belongs to the next backend pruning slice.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for implementation and validation tasks, so no actionable lesson was selected or added.
