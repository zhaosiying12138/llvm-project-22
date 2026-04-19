Your work is not finished. Read and execute the below with ultrathink.

## Original Implementation Plan

**IMPORTANT**: Before proceeding, review the original plan you are implementing:
@docs/plan.md

This plan contains the full scope of work and requirements. Ensure your work aligns with this plan.

---

## Round Re-anchor (REQUIRED FIRST STEP)

Before writing code:
- Re-read @docs/plan.md
- Re-read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md
- Re-read the most recent round summaries/reviews that led to this round
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-26-contract.md

Your round contract must contain:
- Exactly one **mainline objective**
- The 1-2 target ACs for this round
- Which issues are truly **blocking** that mainline objective
- Which issues are **queued** and explicitly out of scope
- Concrete success criteria for this round

Do not start implementation until the round contract exists.

## Task Lane Rules

Use the Task system (TaskCreate, TaskUpdate, TaskList) with one required tag per task:
- `[mainline]` for plan-derived work that directly advances this round's objective
- `[blocking]` for issues that prevent the mainline objective from succeeding safely
- `[queued]` for non-blocking bugs, cleanup, or follow-up work

Rules:
- `[mainline]` work is the round's primary success condition
- `[blocking]` work is allowed only when it truly blocks the mainline objective
- `[queued]` work must be documented but must NOT replace the round objective
- If a new bug does not block the current objective, tag it `[queued]` and keep moving on mainline work

Before executing each task in this round:
1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each task/sub-task
3. Follow selected lesson IDs (or `NONE`) during implementation

---
Below is Codex's review result:
<!-- CODEX's REVIEW RESULT START -->
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
<!-- CODEX's REVIEW RESULT  END  -->
---

## Goal Tracker Reference

Before starting work, **read** @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md to understand:
- The Ultimate Goal and Acceptance Criteria you're working toward
- Which tasks are Active, Completed, or Deferred
- Which side issues are blocking vs queued
- Any Plan Evolution that has occurred
- The latest side-issue state that needs attention

**IMPORTANT**: Keep the mutable section of `goal-tracker.md` up to date during the round.
Do NOT change the immutable section after Round 0.
If you cannot safely reconcile the tracker yourself, include an optional "Goal Tracker Update Request" section in your summary (see below).

## Mainline Guardrails

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-26-contract.md stable for this round
- Do not let queued issues take over the round
- If Codex reported several findings, classify them into:
  - mainline gaps
  - blocking side issues
  - queued side issues
- Only mainline gaps and blocking side issues should drive the next code changes

---

Note: You MUST NOT try to exit by lying, editing loop state files, or executing `cancel-rlcr-loop`.

After completing the work, please:
0. If the `code-simplifier` plugin is installed, use it to review and optimize your code. Invoke via: `/code-simplifier`, `@agent-code-simplifier`, or `@code-simplifier:code-simplifier (agent)`
1. Commit your changes with a descriptive commit message
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-26-summary.md
3. Run `/home/zhaosiying/.codex/skills/humanize/scripts/rlcr-stop-gate.sh` to advance the loop in-session

## Task Tag Routing Reminder

Follow the plan's per-task routing tags strictly:
- `coding` task -> Claude executes directly
- `analyze` task -> execute via `/humanize:ask-codex`, then integrate the result
- Keep Goal Tracker Active Tasks columns `Tag` and `Owner` aligned with execution

**Optional fallback**: if you could not safely update the mutable section of `goal-tracker.md` directly, include this section in your summary:
```markdown
## Goal Tracker Update Request

### Requested Changes:
- [E.g., "Mark Task X as completed with evidence: tests pass"]
- [E.g., "Add to Blocking Side Issues: bug Y blocks AC-2"]
- [E.g., "Add to Queued Side Issues: cleanup Z is non-blocking"]
- [E.g., "Plan Evolution: changed approach from A to B because..."]
- [E.g., "Defer Task Z because... (impact on AC: none/minimal)"]

### Justification:
[Explain why these changes are needed and how they serve the Ultimate Goal]
```

Codex will review your request and reconcile the Goal Tracker if justified.
