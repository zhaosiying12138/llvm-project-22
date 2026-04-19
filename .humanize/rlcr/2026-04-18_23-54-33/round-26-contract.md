# Round 26 Contract

## Mainline Objective

Complete AC-4 YSX test-corpus hygiene by auditing and removing inactive
copied check blocks for removed ISA, vendor, and tuning surfaces across the
YSX-owned tests.

## Target Acceptance Criteria

- AC-4: YSX-owned LLVM and Clang tests keep only active rv64ima-retained
  checks plus intentional negative coverage for unsupported surfaces.
- AC-1: Existing RISCV sources and tests remain unchanged.

## Blocking Issues

- Additional inactive removed-surface check blocks remain in YSX CodeGen tests
  beyond the two files cleaned in Round 25.
- The current stale-test scan still reports copied prefixes for removed C,
  E, FP, Zicfilp, Zabha/Zalasr, XAndes, SFB, CCMOV, and VTCONDOPS surfaces.

## Queued Out Of Scope

- Goal tracker immutable AC-list drift remains queued because review uses
  `docs/plan.md` and tracker rules forbid editing the immutable section.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because
  no current evidence shows unsupported feature leakage.
- Backend source pruning remains out of scope because the Round 24 review
  marked AC-1, AC-2, and AC-3 met.

## Success Criteria

- The broadened stale-prefix scan from the Round 25 review has no unintended
  matches in YSX-owned tests.
- Per-file cleanup removes inactive copied blocks for at least the reviewed
  prefixes: `RV64C`, `RV64E`, `RV64IF`, `RV64ID`, `RV64IFD`,
  `CHECK-ZICFILP`, `RV64I-LARGE-ZICFILP`, `RV64IA-ZABHA`,
  `RV64IA-ZALASR`, `XANDESPERF`, `SFB64`, `VTCONDOPS64`, and
  `CMV-FUSION`.
- Active retained prefixes such as `RV64I`, `RV64`, `RV64IA`,
  `RV64I-ZALRSC`, and active negative-test strings remain intact.
- Focused YSX lit suites, smoke/negative probes, `git diff --check`, and
  RISCV source/test zero-diff validation pass before commit.
