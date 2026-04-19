# Round 19 Contract

## Mainline Objective
Finish the rv64ima-only YSX surface by deleting the residual unsupported-source metadata and trimming YSX-owned tests so they describe only active `ysx64`/`rv64ima` behavior plus executed negative probes.

## Target ACs
- AC-3: YSX contains no copied FP, GlobalISel, RV32, vendor, compressed, vector, or other removed-feature support metadata.
- AC-4: YSX tests are owned YSX coverage for the retained rv64ima surface and do not keep inactive unsupported copied check blocks.

## Blocking Issues
- Residual AC-3 source metadata from the Round 18 review: `PseudoFloatLoad`, floating-point mask constants, stale GlobalISel include, and stale RISC-V/vector/floating-point comments.
- Residual AC-4 test surface from the Round 18 review: inactive RV32, ILP32/LP64E, Zbb, XThead, RV64IA-TSO, WMO/NOZACAS, and other unsupported copied prefixes in YSX tests.

## Queued Out Of Scope
- CPU/tune target-attribute warn-and-ignore policy remains queued unless cleanup exposes feature leakage.
- Immutable tracker drift remains documented and is not edited in this round.

## Success Criteria
- Searched YSX source has no reviewed residual FP/GISel/source-metadata markers.
- YSX tests no longer contain stale unsupported check prefixes except in executed negative tests.
- `git diff --check` passes.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- YSX-only build, combined RISCV+YSX static build, focused YSX lit suites, smoke compiles, and unsupported-feature negative probes pass after cleanup.
