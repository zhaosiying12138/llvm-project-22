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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-27-contract.md

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-27-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-27-summary.md
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
