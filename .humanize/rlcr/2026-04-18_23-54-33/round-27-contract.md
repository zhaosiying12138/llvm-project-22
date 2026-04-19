# Round 27 Contract

## Mainline Objective

Complete AC-4 YSX test-corpus hygiene by removing the remaining inactive copied CCMOV/MIPS select check blocks from YSX-owned CodeGen tests.

## Target ACs

- AC-4: YSX-owned tests must cover the retained rv64ima target surface without copied checks for removed ISA, vendor, or tuning surfaces.
- AC-1: RISCV source and RISCV test directories must remain untouched while cleaning YSX tests.

## Blocking Issues

- YSX CodeGen tests still contain inactive `RV64I-CCMOV` and `RV64-MIPS` check blocks with `mips.ccmov` expectations in `select-and.ll`, `select-or.ll`, `select-cc.ll`, and `select-cond.ll`.

## Queued Issues

- Goal tracker immutable AC-list drift remains queued; continue using `docs/plan.md` as the complete AC source without editing the immutable tracker section.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because they do not block this test-corpus cleanup round.

## Success Criteria

- All inactive `RV64I-CCMOV`, `RV64-MIPS`, and `mips.ccmov` copied check blocks are removed from the four reviewed select tests while active `RV64I` and `RV64` checks and IR bodies are preserved.
- The CCMOV/MIPS scan has no matches in YSX source or tests except intentional negative coverage if any exists.
- The Round-25 stale-prefix scan remains clean.
- Focused lit for the four changed tests, the full focused YSX lit suite, smoke/negative probes, `git diff --check`, and RISCV source/test zero-diff pass.
