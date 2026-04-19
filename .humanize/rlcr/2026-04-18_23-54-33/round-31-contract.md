# Round 31 Contract

## Objective
Resolve the two Round-31 review blockers without touching the RISCV backend:

- Make advertised `ysx64-linux-gnu` Clang targets produce a Linux link command instead of reaching an unsupported-architecture abort in the Linux toolchain.
- Reject RVV-only `-mrvv-vector-bits=` for YSX before the generic RISCV driver path emits any RVV fixed-vector state.

## Planned Changes
- Add `ysx64` handling to Linux multiarch and dynamic-linker selection, using the RISC-V-compatible ELF loader spelling with the fixed YSX `lp64` ABI.
- Add `ysx64` handling to the GNU toolchain paths that share RISC-V assembler arguments, GCC-prefix discovery, and default unwind-table policy.
- Diagnose `-mrvv-vector-bits=` as unsupported for `ysx64`.
- Extend the YSX Clang driver test to cover both the Linux linker command and the RVV vector-bits rejection.

## Validation
- Rebuild the YSX-only and YSX+RISCV configured targets needed by the changed driver paths.
- Run the targeted YSX Clang driver lit test.
- Run focused YSX lit, smoke compile/linker-driver probes, `git diff --check`, and RISCV zero-diff checks.
