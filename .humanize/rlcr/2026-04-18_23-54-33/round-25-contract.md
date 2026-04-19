# Round 25 Contract

## Mainline Objective

Remove inactive copied removed-extension check blocks from YSX-owned tests so
the test corpus reflects only the retained rv64ima surface plus intentional
negative coverage.

## Target Acceptance Criteria

- AC-4: YSX tests are derived from the rv64ima-applicable subset and do not
  preserve inactive C/Zca/Zcmp/RV32/vendor copied check blocks.
- AC-1: RISCV sources and tests remain unchanged while YSX-owned tests are
  cleaned and revalidated.

## Blocking Issues

- `llvm/test/MC/YSX/align.s` contains inactive copied C/Zca check prefixes and
  compressed-instruction expectations.
- `llvm/test/CodeGen/YSX/callee-saved-gprs.ll` contains inactive copied Zcmp
  check blocks and comments.

## Queued Out Of Scope

- Goal tracker immutable AC-list drift remains queued because review uses
  `docs/plan.md` and tracker rules forbid editing the immutable section.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because
  no current evidence shows unsupported feature leakage.
- Backend source pruning remains out of scope because AC-1/AC-2/AC-3 were
  marked met in the Round 24 review.

## Success Criteria

- `align.s` keeps active rv64ima alignment coverage and no longer contains
  inactive C/Zca check prefixes or compressed-instruction expectations.
- `callee-saved-gprs.ll` keeps active RV64I YSX checks and no longer contains
  inactive Zcmp check prefixes or push/pop comments.
- The required stale-prefix scan for removed copied variants has no matches in
  YSX-owned LLVM/Clang tests.
- Focused YSX lit suites, smoke compiles, unsupported negative probes,
  `git diff --check`, and RISCV zero-diff validation pass before commit.
