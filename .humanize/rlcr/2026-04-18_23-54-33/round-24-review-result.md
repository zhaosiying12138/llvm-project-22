# Round 24 Goal Alignment Review

Mainline Progress Verdict: ADVANCED

Round 24 advanced the mainline objective by closing the exact `.reloc`
compressed/vendor/custom relocation leak found in Round 23. The broad
`RISCV.def` and `RISCV_nonstandard.def` imports are gone from
`YSXAsmBackend::getFixupKind`, the reviewed unsupported relocation names now
reject, retained standard `.reloc` names still assemble, and the focused MC
negative test passes.

The full `docs/plan.md` goal is still not complete. AC-4 remains blocked by
stale inactive copied test check blocks for removed extensions.

## Part 1: Goal Tracker Audit

The goal-tracker immutable section still lists only AC-1 and AC-2; this is the
known tracker drift recorded since Round 0. I reviewed against `docs/plan.md`
as the source of truth, which has AC-1 through AC-4.

| AC | Status | Evidence (if MET) | Blocker (if NOT MET) | Justification (if DEFERRED) |
|----|--------|-------------------|----------------------|-----------------------------|
| AC-1 | MET | YSX remains standalone in source, Round 24 touched only YSX/test/tracker files, `git diff -- llvm/lib/Target/RISCV \| wc -l` is `0`, and the reported YSX-only plus RISCV+YSX builds have no contradictory evidence. | - | - |
| AC-2 | MET | Direct review probe rejects `.reloc ., R_RISCV_RVC_BRANCH, sym`; retained `.reloc ., R_RISCV_32, sym` assembles; `unsupported-features.s` passes; prior unsupported `-mattr`, `.option arch`, CSR, `.insn`, Clang arch, and unsupported IR probes are preserved. | - | - |
| AC-3 | MET | The Round-24 source leak is fixed: `rg "RISCV_nonstandard|R_RISCV_VENDOR|R_RISCV_RVC_|R_RISCV_QC_|R_RISCV_NDS_|R_RISCV_CHERIOT|R_RISCV_CUSTOM" llvm/lib/Target/YuShuXin llvm/test/MC/YSX` reports only intentional negative-test lines, and `rg "RISCV\\.def|RISCV_nonstandard\\.def" llvm/lib/Target/YuShuXin llvm/test/MC/YSX` has no matches. Current YSX backend size remains materially smaller than RISCV. | - | - |
| AC-4 | PARTIAL | YSX-owned tests exist and focused tests pass. | YSX tests still contain inactive copied check blocks for removed features, including `C-OR-ZCA-EXT` / `C-EXT` blocks in `llvm/test/MC/YSX/align.s` and `RV64IZCMP` blocks with `cm.push`/`cm.popret` in `llvm/test/CodeGen/YSX/callee-saved-gprs.ll`. These are not active RUN prefixes, but they violate the plan requirement that copied tests be excluded or rewritten to the retained rv64ima surface. | - |

### Forgotten Items Detection

Forgotten item found: stale inactive unsupported test prefixes were treated as
resolved in Round 19, but the current tree still has at least two concrete
leftovers:

1. `llvm/test/MC/YSX/align.s` contains inactive `C-OR-ZCA-EXT-*` and
   `C-EXT-INST` expectations, including `c.nop` checks.
2. `llvm/test/CodeGen/YSX/callee-saved-gprs.ll` contains inactive
   `RV64IZCMP` and `RV64IZCMP-WITH-FP` check blocks with Zcmp push/pop output.

The tracker has been corrected: Plan Version 50 keeps final work active under
an AC-4 test-cleanup task and records this Round 24 review.

### Deferred Items Audit

There are no rows in `Explicitly Deferred`. The queued immutable-tracker drift
remains valid because the immutable section must not be edited and reviews are
using `docs/plan.md`. The CPU/tune warn-and-ignore policy remains queued and
non-blocking because no reviewed probe has shown unsupported feature leakage.

### Goal Completion Summary

Acceptance Criteria: 3/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 1
Critical blockers: stale inactive removed-extension check blocks in YSX-owned tests

## Part 2: Mainline Drift Audit

Round 24 had a clear singular objective: close the `.reloc` removed-relocation
surface. Claude advanced that objective directly and added negative coverage.
The newly found stale-test issue is plan-derived AC-4 work, not a new source
surface regression.

Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 1
Queued Side Issues: 2

Blocking side issue:

1. AC-4 stale inactive copied test prefixes for removed C/Zca/Zcmp surfaces.

Queued side issues:

1. Goal tracker immutable AC-list drift.
2. CPU/tune target-attribute warn-and-ignore diagnostics.

## Part 3: Implementation Review

The Round-24 relocation implementation is correct for the reviewed leak.
`YSXAsmBackend.cpp` now uses an explicit `StringSwitch` whitelist of retained
standard RISC-V ELF relocation names rather than including the full RISCV and
nonstandard relocation definition files. This removes the source dependency on
the copied relocation-name tables that caused the Round-23 failure.

Direct probes with the current YSX-only tool confirm behavior:

```text
.reloc ., R_RISCV_RVC_BRANCH, sym    -> error: unknown relocation name
.reloc ., R_RISCV_32, sym            -> assembles
```

The focused test `llvm/test/MC/YSX/unsupported-features.s` also passes, and
`git diff --check HEAD^..HEAD` plus the RISCV zero-diff check pass.

The remaining implementation gap is in test corpus hygiene, not the Round-24
relocation code. `llvm/test/MC/YSX/align.s` has inactive C/Zca FileCheck
prefixes even though no YSX RUN line exercises compressed alignment behavior.
`llvm/test/CodeGen/YSX/callee-saved-gprs.ll` has inactive Zcmp push/pop
check blocks even though Zcmp support has been removed. These stale blocks can
mask accidental resurrection of removed-surface expectations and contradict
the plan's AC-4 requirement for an rv64ima-applicable YSX-owned test subset.

I did not rerun the full Ninja build in this review sandbox. I did run direct
MC probes, targeted lit for `unsupported-features.s`, `align.s`, and
`callee-saved-gprs.ll`, `git diff --check HEAD^..HEAD`, and RISCV zero-diff.

## Part 4: Goal Tracker Update

I updated `goal-tracker.md` mutable state:

1. Bumped Plan Version to 50.
2. Added a Round 24 review plan-evolution entry.
3. Replaced the resolved `.reloc` blocking issue with the stale-test AC-4
   blocker.
4. Reopened active test-cleanup and final revalidation tasks.
5. Added a Round-24 completed/verified row for the relocation pruning slice,
   marked review-partial because AC-4 still blocks full completion.

The immutable section was not modified.

## Part 5: Progress Stagnation Check

Development is not stagnating. Rounds 20 through 24 addressed distinct,
review-identified blockers: dead removed-feature scaffolding, `.insn` opcode
acceptance, selector residue, SFB/scheduler/custom-ISD residue, and now
`.reloc` relocation-name acceptance. The stale-test issue is a recurring AC-4
cleanup class, but the recent rounds still made concrete mainline progress and
did not loop on the same exact defect.

## Required Action Items

Mainline Gaps:

1. Remove inactive copied C/Zca check blocks from
   `llvm/test/MC/YSX/align.s`, including `C-OR-ZCA-EXT-*` and `C-EXT-INST`
   expectations, while preserving active rv64ima alignment coverage.
2. Regenerate or manually trim `llvm/test/CodeGen/YSX/callee-saved-gprs.ll` so
   only active `RV64I` / `RV64I-WITH-FP` YSX checks remain; remove inactive
   `RV64IZCMP*` blocks and Zcmp comments.
3. Run a focused stale-prefix scan for unsupported copied variants in YSX
   tests, at minimum:
   `rg "C-OR-ZCA|C-EXT|ZCA|ZCMP|RV64IZCMP|RV32|LP64E|XTHEAD|RV64IA-TSO" llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`.
4. Re-run focused YSX lit suites, smoke compiles, unsupported negative probes,
   `git diff --check`, and `git diff -- llvm/lib/Target/RISCV | wc -l`.

Blocking Side Issues:

1. AC-4 stale inactive removed-extension check blocks in YSX-owned tests.

Queued Side Issues:

1. Immutable tracker AC-list drift; continue reviewing against `docs/plan.md`.
2. CPU/tune target-attribute diagnostics; keep queued unless feature leakage is
   observed or the intended policy becomes hard-error-only.

REQUIRES MORE WORK
