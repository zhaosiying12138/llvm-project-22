# Round 27 Review Result

Mainline Progress Verdict: ADVANCED

Round 27 made real AC-4 progress: the inactive `RV64I-CCMOV` and `RV64-MIPS` select check blocks with `mips.ccmov` expectations are gone from the four reviewed YSX CodeGen tests, and the exact Round-26 blocker scan is clean. The full completion claim is still not valid because a broader source scan found residual copied MIPS/CCMov source scaffolding in the YSX backend.

## Mainline Gaps

1. AC-3 still has copied MIPS/CCMov/load-store-pair source residue.

   The original plan requires YSX to remove support code for removed features and to be materially pruned to the retained `rv64ima` surface. Round 23 claimed the SFB/CCMOV source residue had been deleted, and Round 27 claims the CCMOV/MIPS scan is clean, but the scan used a case-sensitive pattern that misses remaining `MIPS` and `CCMov` spellings.

   Concrete remaining examples:

   - `llvm/lib/Target/YuShuXin/YSXSubtarget.h:147` declares `useMIPSLoadStorePairs()`.
   - `llvm/lib/Target/YuShuXin/YSXSubtarget.h:148` declares `useMIPSCCMovInsn()`.
   - `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:230` and `:234` define both hooks as always returning false.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:1323` still carries a copied MIPS load/store-pairing comment.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.h:310` and `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:1326` keep dead load/store-pair helper declarations/definitions; current review scans found no YSX call sites for these helpers.

   Reproducer:

   ```bash
   rg -n "useMIPSLoadStorePairs|useMIPSCCMovInsn|MIPS|CCMov" llvm/lib/Target/YuShuXin
   ```

2. The Round 27 AC-4 stale-test blocker itself appears fixed.

   These scans are clean:

   ```bash
   rg -n "ccmov|mips\\.ccmov|RV64I-CCMOV|RV64-MIPS" \
     llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX \
     clang/test/Driver/YSX clang/test/CodeGen/YSX

   rg -n "ZICFILP|XANDESPERF|RV64C|RV64E|RV64IF|RV64ID|RV64IFD|SFB64|VTCONDOPS|CMV-FUSION|ZABHA|ZALASR|LP64E|RV32|XTHEAD|ZCMP|ZCA|RV64IA-TSO" \
     llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX
   ```

## Blocking Side Issues

1. The residual copied MIPS/CCMov/load-store-pair source scaffolding blocks full plan completion.

   This is not a new feature request or cosmetic cleanup. It is the same class of removed-surface source residue that AC-3 and prior reviews have been forcing out of the backend. Even though the remaining hooks appear inert, they are plan-derived pruning work and should not be left queued.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued. Continue reviewing against `docs/plan.md`; do not edit the immutable tracker section.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued because this review did not find evidence that they leak unsupported feature state.

## Goal Alignment Summary

ACs: 4/4 addressed (3/4 met; AC-3 partial) | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: Still addressed. `git diff --name-only HEAD^..HEAD -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV clang/test/Driver/RISCV clang/test/CodeGen/RISCV` is empty, and `git diff --check HEAD^..HEAD` passes.
- AC-2: Still addressed for reviewed visible surfaces; the new finding is inert source residue, not an accepted YSX ISA path.
- AC-3: Not met. YSX still carries copied MIPS/CCMov/load-store-pair source scaffolding in `YSXSubtarget.*` and `YSXInstrInfo.*`.
- AC-4: Advanced and the reviewed Round-26 stale select-test blocker is fixed.

Forgotten item: the Round-27 scan did not include case-sensitive `MIPS`/`CCMov` source spellings, so it missed residual source scaffolding while claiming the CCMOV/MIPS surface was clean.

Deferred items: none in `Explicitly Deferred`. The remaining AC-3 source cleanup must be done now; it should not be moved to queued work.

Plan evolution: I updated the mutable tracker to verify the Round-27 AC-4 cleanup, reopen task3/task6 for the residual source cleanup, and replace the resolved stale-test blocker with the MIPS/CCMov source blocker.

## Required Implementation Plan

1. Remove the unused `useMIPSLoadStorePairs()` and `useMIPSCCMovInsn()` declarations from `llvm/lib/Target/YuShuXin/YSXSubtarget.h` and their definitions from `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp`.
2. Remove the dead `YSXInstrInfo::isPairableLdStInstOpc()` and `YSXInstrInfo::isLdStSafeToPair()` declarations from `llvm/lib/Target/YuShuXin/YSXInstrInfo.h` and their definitions from `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`, including the stale MIPS load/store-pairing comment, unless a fresh call-site audit finds a real retained rv64ima caller.
3. Re-run:

   ```bash
   rg -n "MIPS|CCMov|ccmov|mips\\.ccmov|RV64I-CCMOV|RV64-MIPS" \
     llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX \
     clang/test/Driver/YSX clang/test/CodeGen/YSX
   ```

   The scan should have no matches except intentional negative-test strings, if any are added.
4. Re-run the Round-25 stale-prefix scan and the broad removed-surface scan used in Round 27.
5. Re-run focused validation: YSX-only build targets, combined RISCV+YSX build targets, focused YSX lit suite, smoke/negative probes, `git diff --check`, and RISCV source/test zero-diff.

## Validation Notes

- PASS: exact Round-26 CCMOV/MIPS stale-test scan has no matches.
- PASS: Round-25 stale-prefix scan has no matches.
- PASS: `git diff --check HEAD^..HEAD`.
- PASS: RISCV source/test zero-diff for the Round-27 commit.
- FAIL: broader source scan still reports `MIPS`/`CCMov` residue in YSX backend source.

REQUIRES MORE WORK
