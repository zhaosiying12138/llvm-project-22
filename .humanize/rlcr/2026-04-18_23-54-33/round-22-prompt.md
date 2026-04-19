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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-22-contract.md

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
# Round 21 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 21 advanced the mainline by closing the reviewed `.insn` removed-opcode
acceptance path. Removed named FP/vector/custom opcode forms now reject,
removed numeric major opcode values now reject, `.insn r4` now rejects, and
retained rv64ima `.insn` forms still assemble. However, the original
`docs/plan.md` goal is still not complete: YSX still carries dead selector
scaffolding for removed source paths.

## Goal Alignment Summary

ACs: 4/4 addressed, 3/4 met | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MET. YSX remains standalone and RISCV source diff is still zero.
- AC-2: MET for the reviewed surface. The `.insn` opcode leak is fixed, and
  quick probes still reject unsupported vector, FP, `.option arch`, CSR, and
  unsupported IR paths.
- AC-3: PARTIAL. The Round 21 `.insn` format/opcode scaffolding is gone, but
  copied dead selector scaffolding for removed Zba-style SHXADD/addressing,
  SiFive, and vector-combine paths remains.
- AC-4: MET for current coverage. The new MC negative coverage exists and the
  focused unsupported-feature test passes; full revalidation is still needed
  after the remaining AC-3 deletion.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 44 records this Round 21 review, keeps task3/task6 active, replaces
the resolved `.insn` blocker with the selector-scaffolding blocker, and marks
Round 21 as review-partial. The immutable section was not modified.

## Mainline Gaps

1. AC-3 remains incomplete: YSX still contains dead selector scaffolding for
   removed Zba-style SHXADD/scaled-address, SiFive, and vector-combine paths.

   `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:438` still defines
   `AddrRegRegScale`, and `YSXInstrInfo.td:440` still defines
   `AddrRegZextRegScale`, even though the retained rv64ima instruction set has
   no scaled register-register addressing form using these complex patterns.
   The corresponding selector declarations remain in
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h:55` through
   `YSXISelDAGToDAG.h:71`, and the implementations remain in
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:1248` through
   `YSXISelDAGToDAG.cpp:1342`.

   The same file also retains Zba-style SHXADD pattern helpers:
   `YSXISelDAGToDAG.h:110` through `YSXISelDAGToDAG.h:118` declare
   `selectSHXADDOp` and `selectSHXADD_UWOp`, while
   `YSXISelDAGToDAG.cpp:1554` through `YSXISelDAGToDAG.cpp:1710` implement
   them with comments explicitly describing SHXADD/SHXADD_UW folding. These
   helpers are not part of rv64ima and should not remain as copied
   removed-feature source scaffolding.

   There is also a no-op copied SiFive selector hook at
   `YSXISelDAGToDAG.h:133` and `YSXISelDAGToDAG.cpp:232`
   (`selectSF_VC_X_SE`), plus a stale vector-combine declaration at
   `YSXISelDAGToDAG.h:163` (`performCombineVMergeAndVOps`). These are not
   user-visible acceptance leaks, but they directly contradict AC-3's
   requirement that removed-feature support code be pruned.

## Blocking Side Issues

None separate from the mainline gap above. The remaining issue is plan-derived
AC-3 source pruning work, not an unrelated side issue.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. Current
   probes still do not show unsupported feature leakage through those warnings.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. Delete the unused TableGen complex-pattern classes `AddrRegRegScale` and
   `AddrRegZextRegScale` from `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`.
2. Delete the corresponding declarations and template wrappers from
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h`.
3. Delete `YSXDAGToDAGISel::SelectAddrRegRegScale` and
   `YSXDAGToDAGISel::SelectAddrRegZextRegScale` from
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`.
4. Delete `selectSHXADDOp`, `selectSHXADD_UWOp`, `selectSF_VC_X_SE`, and the
   stale `performCombineVMergeAndVOps` declaration/definition surface from
   `YSXISelDAGToDAG.h` and `YSXISelDAGToDAG.cpp`.
5. Run `rg "AddrRegRegScale|AddrRegZextRegScale|selectSHXADD|SHXADD|selectSF_VC_X_SE|performCombineVMergeAndVOps" llvm/lib/Target/YuShuXin`
   and require no matches outside any intentionally retained negative test
   text. Do not replace these helpers with stubs.
6. Rebuild both configured target sets, rerun the focused YSX lit suites, run
   the retained rv64ima smoke tests and unsupported-feature probes, then run
   `git diff --check` and `git diff -- llvm/lib/Target/RISCV | wc -l`.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-22-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-22-summary.md
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
