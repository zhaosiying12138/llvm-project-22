# Round 26 Review Result

Mainline Progress Verdict: ADVANCED

Round 26 made real AC-4 progress by removing the exact stale inactive removed-surface blocks reported by the Round-25 regex in 13 YSX CodeGen tests. The completion claim is still not valid: the same class of inactive copied removed-surface checks remains in other YSX-owned CodeGen tests outside Claude's stale-prefix regex.

## Mainline Gaps

1. AC-4 stale inactive CCMOV/MIPS select check blocks remain in YSX-owned tests.

   The original plan requires copied YSX tests to exclude or rewrite tests that still require removed ISA, vendor, or tuning surfaces. Round 26 cleaned `CMV-FUSION` from `select-binop-identity.ll`, but did not audit the related inactive select-check prefixes that spell the removed conditional-move surface differently.

   Concrete remaining examples:

   - `llvm/test/CodeGen/YSX/select-and.ll:2` runs only `RV64I`, but `llvm/test/CodeGen/YSX/select-and.ll:19` still has inactive `RV64I-CCMOV` checks, including `mips.ccmov` at line 22.
   - `llvm/test/CodeGen/YSX/select-or.ll:2` runs only `RV64I`, but `llvm/test/CodeGen/YSX/select-or.ll:19` still has inactive `RV64I-CCMOV` checks, including `mips.ccmov` at line 22.
   - `llvm/test/CodeGen/YSX/select-cc.ll:2` runs only `RV64I`, but `llvm/test/CodeGen/YSX/select-cc.ll:88`, `:221`, `:265`, and `:290` still have inactive `RV64I-CCMOV` checks with `mips.ccmov` expectations.
   - `llvm/test/CodeGen/YSX/select-cond.ll:2` runs only `RV64`, but `llvm/test/CodeGen/YSX/select-cond.ll:20` and many later blocks still have inactive `RV64-MIPS` checks with `mips.ccmov` expectations.

   Reproducer:

   ```bash
   rg -n "ccmov|mips\\.ccmov|RV64I-CCMOV|RV64-MIPS" \
     llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX \
     clang/test/Driver/YSX clang/test/CodeGen/YSX
   ```

   This reports only YSX CodeGen test comments, so the active backend source no longer appears to retain this surface; the residue is stale inactive copied test text.

## Blocking Side Issues

1. The AC-4 stale-test blocker remains open. It directly blocks the current mainline objective because YSX-owned tests still preserve inactive copied expectations for a removed conditional-move/MIPS surface.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued. Continue reviewing against `docs/plan.md`; do not edit the immutable tracker section.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued because this review did not find evidence that they leak unsupported feature state.

## Goal Alignment Summary

ACs: 4/4 addressed (3/4 met; AC-4 partial) | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: Still addressed. The Round-26 diff does not touch RISCV source or test directories; `git diff --name-only HEAD~1..HEAD -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV` is empty.
- AC-2: Still addressed by prior rounds; the reviewed issue is inactive test text rather than an accepted YSX codegen path.
- AC-3: Still addressed by prior rounds for this review scope; the CCMOV/MIPS scan does not find live YSX backend source matches.
- AC-4: Advanced but not met. The exact Round-25 regex is clean, but a broader inactive-prefix/mnemonic audit still finds copied removed-surface check blocks.

Forgotten item: the Round-26 cleanup treated the Round-25 regex as complete coverage and missed removed conditional-move checks spelled as `RV64I-CCMOV`, `RV64-MIPS`, and `mips.ccmov`.

Deferred items: none in `Explicitly Deferred`. The queued tracker drift and CPU/tune diagnostics do not justify stopping AC-4 cleanup.

Plan evolution: I updated the mutable tracker to keep task5/task6 active, record this Round-26 review rejection, and expand the blocking issue to include the remaining CCMOV/MIPS stale test blocks.

## Required Implementation Plan

1. Remove all inactive `RV64I-CCMOV` and `RV64-MIPS` check blocks from `llvm/test/CodeGen/YSX/select-and.ll`, `select-or.ll`, `select-cc.ll`, and `select-cond.ll`. Preserve the active `RV64I` and `RV64` checks and the IR bodies.
2. Run a broad inactive-prefix audit across `llvm/test/MC/YSX`, `llvm/test/CodeGen/YSX`, `clang/test/Driver/YSX`, and `clang/test/CodeGen/YSX`; for this round, ensure no removed-surface leftovers remain for `ccmov`, `mips.ccmov`, `RV64I-CCMOV`, `RV64-MIPS`, `CMV-FUSION`, `SFB`, `VTCONDOPS`, `ZICFILP`, `ZABHA`, `ZALASR`, `XANDES`, compressed, FP, RVE, vector, RV32, or vendor prefixes. Ignore only intentional negative-test strings.
3. Re-run the exact Round-25 stale-prefix scan and the added CCMOV/MIPS scan. Both must be clean except for intentional negative coverage.
4. Re-run focused validation for the four changed select tests, then the focused YSX lit suite, smoke/negative probes, `git diff --check`, and RISCV source/test zero-diff.

## Validation Notes

- PASS: exact Round-25 stale-prefix scan for `ZICFILP|XANDESPERF|RV64C|RV64E|RV64IF|RV64ID|RV64IFD|SFB64|VTCONDOPS|CMV-FUSION|ZABHA|ZALASR|LP64E|RV32|XTHEAD|ZCMP|ZCA|RV64IA-TSO` has no matches.
- FAIL: broader CCMOV/MIPS scan still reports the four CodeGen test files listed above.
- PASS: `git diff --check HEAD~1..HEAD`.
- PASS: RISCV source/test zero-diff for the Round-26 commit.

REQUIRES MORE WORK
