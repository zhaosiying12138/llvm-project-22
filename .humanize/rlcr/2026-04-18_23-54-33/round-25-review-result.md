# Round 25 Review Result

Mainline Progress Verdict: ADVANCED

Round 25 advanced the immediate AC-4 cleanup by removing the two stale examples
called out in Round 24: inactive C/Zca checks in `llvm/test/MC/YSX/align.s` and
inactive Zcmp checks in `llvm/test/CodeGen/YSX/callee-saved-gprs.ll`. That
work is real, but the completion claim is not valid. The same AC-4 problem
still exists elsewhere in the YSX-owned CodeGen test corpus.

## Mainline Gaps

1. AC-4 stale inactive copied test blocks remain beyond the two files Claude
   edited.

   The original plan requires copied YSX tests to be excluded or rewritten when
   they still require removed ISA features. Round 25 only scanned the exact
   strings from the Round 24 examples, so it missed other inactive prefixes and
   expectations for removed surfaces. Concrete examples:

   - `llvm/test/CodeGen/YSX/add-before-shl.ll:2` only runs `RV64I`, but
     `llvm/test/CodeGen/YSX/add-before-shl.ll:16` still contains inactive
     `RV64C` checks with compressed instructions such as `c.addi`, `c.slli`,
     and `c.jr`.
   - `llvm/test/CodeGen/YSX/calling-conv-preserve-most.ll:2` only runs
     `RV64I`, but `llvm/test/CodeGen/YSX/calling-conv-preserve-most.ll:43`
     still contains inactive `RV64E` checks.
   - `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll:2` only runs `RV64I`, but
     `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll:19` still contains inactive
     `RV64IF`/`RV64ID` checks with `fsw`/`fsd`.
   - `llvm/test/CodeGen/YSX/select-const.ll:2` only runs `RV64`, but
     `llvm/test/CodeGen/YSX/select-const.ll:60` and
     `llvm/test/CodeGen/YSX/select-const.ll:87` still contain inactive
     `RV64IFD` checks, including FP move expectations.
   - `llvm/test/CodeGen/YSX/branch-relaxation-rv64.ll:6` uses default
     `CHECK`, but `llvm/test/CodeGen/YSX/branch-relaxation-rv64.ll:23` still
     contains inactive `CHECK-ZICFILP` checks with `lpad`.
   - `llvm/test/CodeGen/YSX/calls.ll:8` runs `RV64I-LARGE`, but
     `llvm/test/CodeGen/YSX/calls.ll:62` still contains inactive
     `RV64I-LARGE-ZICFILP` checks with `lpad`.
   - `llvm/test/CodeGen/YSX/atomic-cmpxchg.ll:4` runs only `RV64I` and
     `RV64IA`, but `llvm/test/CodeGen/YSX/atomic-cmpxchg.ll:55` still contains
     inactive `RV64IA-ZABHA` checks with `amocas.*`.
   - `llvm/test/CodeGen/YSX/atomic-load-store.ll:9` runs only `RV64IA`, but
     `llvm/test/CodeGen/YSX/atomic-load-store.ll:109` still contains inactive
     `RV64IA-ZALASR` checks with `lb.aq`/`sb.rl` style expectations.
   - `llvm/test/CodeGen/YSX/add_sext_shl_constant.ll:2` only runs `RV64`, but
     `llvm/test/CodeGen/YSX/add_sext_shl_constant.ll:15` still contains
     inactive `XANDESPERF` checks with `nds.lea.*`.
   - `llvm/test/CodeGen/YSX/select-binop-identity.ll:2` only runs `RV64I`, but
     `llvm/test/CodeGen/YSX/select-binop-identity.ll:20` still contains
     inactive `SFB64`, `VTCONDOPS64`, and `CMV-FUSION` copied checks.

   A representative broadened scan that still finds stale removed-surface
   blocks is:

   ```bash
   rg -l "ZICFILP|XANDESPERF|RV64C|RV64E|RV64IF|RV64ID|RV64IFD|SFB64|VTCONDOPS|CMV-FUSION|ZABHA|ZALASR|LP64E|RV32|XTHEAD|ZCMP|ZCA|RV64IA-TSO" \
     llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX
   ```

   It currently reports at least these YSX CodeGen files:
   `add-before-shl.ll`, `add_sext_shl_constant.ll`,
   `atomic-cmpxchg-branch-on-result.ll`, `atomic-cmpxchg.ll`,
   `atomic-load-store.ll`, `atomic-load-zext.ll`,
   `branch-relaxation-rv64.ll`, `calling-conv-preserve-most.ll`, `calls.ll`,
   `inline-asm-clobbers.ll`, `jumptable-swguarded.ll`,
   `select-binop-identity.ll`, and `select-const.ll`.

## Blocking Side Issues

1. The AC-4 stale-test blocker remains open. It directly blocks the current
   mainline objective because the YSX-owned tests still preserve inactive
   copied expectations for removed extensions and removed tuning/vendor paths.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued. Continue reviewing
   against `docs/plan.md`; do not edit the immutable tracker section.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued because
   this review did not find evidence that they leak unsupported feature state.

## Goal Alignment Summary

ACs: 4/4 addressed (3/4 met; AC-4 partial) | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: Still addressed. Round 25 did not modify RISCV backend/test sources;
  `git diff --name-only HEAD^..HEAD -- llvm/lib/Target/RISCV llvm/test/MC/RISCV
  llvm/test/CodeGen/RISCV` is empty.
- AC-2: Still addressed by prior rounds; this round did not change the
  implementation surface.
- AC-3: Still addressed by prior rounds; this round did not change backend
  source pruning.
- AC-4: Advanced but not met. The two reviewed stale blocks are gone, but the
  broader YSX-owned test corpus still contains inactive copied blocks for
  removed surfaces.

Forgotten item: the Round-25 cleanup treated the two Round-24 examples as the
entire stale-test problem instead of auditing all inactive removed-surface
check prefixes in YSX tests.

Deferred items: none in `Explicitly Deferred`. The queued tracker drift and
CPU/tune diagnostics do not justify stopping AC-4 cleanup.

Plan evolution: Claude's Round-25 tracker update marked the stale-test blocker
as addressed pending review. I corrected the mutable tracker to keep task5 and
task6 active, expanded the blocking issue, and added this Round-25 review entry.

## Required Implementation Plan

1. Audit the YSX-owned test corpus for inactive check prefixes whose RUN lines
   do not reference them and whose expectations describe removed YSX surfaces.
   Use both a broad removed-surface scan and per-file RUN-prefix comparison.

2. Regenerate or manually trim the affected generated CodeGen tests so that
   only active YSX rv64ima prefixes remain. At minimum, clean the files listed
   above and remove inactive blocks for `RV64C`, `RV64E`, `RV64IF`,
   `RV64ID`, `RV64IFD`, `CHECK-ZICFILP`, `RV64I-LARGE-ZICFILP`,
   `RV64IA-ZABHA`, `RV64IA-ZALASR`, `XANDESPERF`, `SFB64`, `VTCONDOPS64`, and
   `CMV-FUSION`. Preserve active retained prefixes such as `RV64I`, `RV64`,
   `RV64IA`, `RV64I-ZALRSC`, and the active `NO-ZICFILP` prefix in
   `jumptable-swguarded.ll` unless a file-specific RUN line says otherwise.

3. Reword or remove stale comments that describe removed extensions, vendor
   paths, or disabled tuning modes. Do not remove intentional negative coverage
   from `unsupported-features.s`.

4. Re-run the broadened stale-prefix scan above and the original Round-25 scan.
   Both must be clean except for intentional negative-test strings and active
   retained A-extension split prefixes such as `RV64I-ZALRSC`.

5. Re-run focused validation: the changed tests, `llvm/test/MC/YSX`,
   `llvm/test/CodeGen/YSX`, the YSX Clang test dirs in a writable lit exec
   root, smoke/negative probes, `git diff --check`, and RISCV zero-diff.

## Validation Notes

- The two tests changed by Round 25 pass in this review sandbox:
  `llvm-lit -q llvm/test/MC/YSX/align.s llvm/test/CodeGen/YSX/callee-saved-gprs.ll`.
- A broader LLVM YSX lit run could not be completed in this sandbox because
  the external build test output root is read-only; the observed failure was
  `MC/YSX/ysx64-64b-pcrel.s` failing to write its temporary object file, not a
  checked-code mismatch.
- `git diff --check HEAD^..HEAD` passed.
- RISCV source/test zero-diff checks passed for the reviewed commit.

REQUIRES MORE WORK
