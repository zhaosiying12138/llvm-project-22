# Round 5 Contract

## Mainline Objective

Restore the supported YSX Clang compilation path for default `ysx64` and explicit `rv64ima`, while preserving rejection of enabled non-IMA features.

## Target Acceptance Criteria

- AC-2: `ysx64` exposes only the supported `rv64ima` ISA/`lp64` ABI surface and rejects enabled unsupported extensions.
- AC-4: YSX-owned Clang tests execute the backend for the supported default and explicit `rv64ima` paths.

## Blocking Issues

- Clang still emits inherited RISCV disabled feature flags for `ysx64`; current YSX feature validation treats disabled unsupported features the same as enabled unsupported features and fatally rejects the supported compile path.
- Existing positive Clang driver coverage uses only `-###`, so it does not verify that the backend can compile a real translation unit.

## Queued Out Of Scope

- Further AC-3 source deletion in `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, `YSXSubtarget.h`, `YSX.h`, frame helpers, and TargetParser parser aliases.
- Stale copied CodeGen check-prefix cleanup.
- Residual copied "RISC-V" comments in YSX files.
- Immutable goal-tracker AC drift.

## Success Criteria

- `clang --target=ysx64-unknown-elf -c` compiles a simple C source.
- `clang --target=ysx64-unknown-elf -march=rv64ima -c` compiles a simple C source.
- Enabled unsupported features such as `-march=rv64imaf`, `-march=rv64imac`, and backend `-mattr=+f`/`+v` still reject.
- YSX-owned Clang tests include real compile coverage for the default target and explicit `rv64ima`.
- Focused YSX lit tests pass, both YSX-only and RISCV+YSX build targets still build, and `llvm/lib/Target/RISCV` remains unchanged.
