# Round 32 Contract

## Objective
Resolve the Round-32 review blockers while preserving the Round-31 driver fixes and keeping RISCV sources/tests untouched.

## Blocking Issues
- P1: YSX test directories run unconditionally in default LLVM/Clang test suites even when the experimental YSX target is not registered.
- P2: `ysx64` is included in `Triple::isRISCV64()`, but the GNU RISC-V multilib selector still checks only the raw `riscv64` enum and therefore treats `ysx64` as an RV32 multilib target.

## Planned Changes
- Add YSX local lit guards to the YSX-owned LLVM and Clang test directories so they are unsupported unless the `ysx-registered-target` lit feature is present.
- Change the GNU RISC-V multilib 64-bit predicate from an enum-only `riscv64` check to `TargetTriple.isRISCV64()`.
- Rebuild affected driver components, rerun focused YSX tests, and run direct probes for guarded lit behavior and Linux multilib/linker output.

## Non-Goals
- Do not change RISCV backend sources or RISCV tests.
- Do not broaden YSX ISA support beyond `rv64ima`.
