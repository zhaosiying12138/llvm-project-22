# Round 28 Contract

## Mainline Objective

Complete AC-3 source pruning by deleting residual copied MIPS/CCMov/load-store-pair scaffolding from the YSX backend.

## Target ACs

- AC-3: YSX implementation must not retain support code or dead scaffolding for removed features.
- AC-1: RISCV source and RISCV test directories must remain untouched while pruning YSX source.

## Blocking Issues

- `YSXSubtarget.*` still declares and defines inert `useMIPSLoadStorePairs()` and `useMIPSCCMovInsn()` hooks.
- `YSXInstrInfo.*` still declares and defines dead load/store-pair helpers and carries a copied MIPS load/store-pairing comment.

## Queued Issues

- Goal tracker immutable AC-list drift remains queued; continue using `docs/plan.md` as the complete AC source without editing the immutable tracker section.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because they do not block this source-pruning round.

## Success Criteria

- The reviewed MIPS/CCMov/load-store-pair hooks and helper declarations/definitions are removed unless a fresh call-site audit finds retained rv64ima callers.
- The broad scan for `MIPS|CCMov|ccmov|mips\.ccmov|RV64I-CCMOV|RV64-MIPS` has no YSX source or test matches except intentional negative coverage, if any.
- Round-25 stale-prefix and Round-27 broad removed-surface scans remain clean.
- YSX-only build targets, combined RISCV+YSX build targets, focused YSX lit, smoke/negative probes, `git diff --check`, and RISCV source/test zero-diff pass.
