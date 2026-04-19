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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-20-contract.md

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
# Round 19 Goal Alignment Review

Mainline Progress Verdict: ADVANCED

Round 19 advanced the mainline cleanup: the Round 18 stale `PseudoFloatLoad`,
FP mask, GlobalISel include/CSE hook, stale vector-stack comment, and inactive
unsupported YSX check-prefix blocks are gone. The visible target surface is now
much tighter: the YSX-only `llc -mtriple=ysx64-unknown-elf` smoke emits
`.attribute 5, "rv64ima"`, `clang --target=ysx64-unknown-elf` emits an ELF64
RISC-V soft-float object, unsupported `+v`, vendor `+xventanacondops`,
`rv64imaf`, and `.option arch, rv64ima_zbb` reject, numeric CSR/Zifencei/
privileged probes reject, `-mattr=help` lists only the curated rv64ima-related
features, `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`, and
`git diff --check` is clean.

The original `docs/plan.md` goal is still not complete. AC-3 requires no support
code for removed features, and YSX still carries dead copied removed-feature
scaffolding even though front-door probes reject the corresponding inputs.

## Acceptance Criteria Status

| AC | Status | Evidence / Blocker |
|----|--------|--------------------|
| AC-1 | MET | YSX-only built binaries initialize `llc -mtriple=ysx64-unknown-elf`; smoke codegen succeeds and RISCV source diff is `0`. Claude's combined static-build claim could not be rerun in this sandbox because the external build dir is read-only, but existing combined smoke evidence has not regressed. |
| AC-2 | MET | Default Clang emits only `+i,+m,+a,+zmmul,+zaamo,+zalrsc,+relax` and `lp64`; unsupported RV32/FP/vector/vendor/compressed/CSR/Zifencei probes reject deterministically. |
| AC-3 | PARTIAL | Major pruning is real, but residual dead removed-feature source remains: `MCTargetDesc/YSXBaseInfo.h:127` and `YSXInstrInfo.cpp:1313` keep Zibi operand metadata; `YSXBaseInfo.h:144` through `YSXBaseInfo.h:149` and `YSXInstrInfo.cpp:1352` through `YSXInstrInfo.cpp:1361` keep VTYPE/RVKR metadata; `AsmParser/YSXAsmParser.cpp:652` keeps a compressed `CLUI` matcher; `YSXInsertReadWriteCSR.cpp:15` through `YSXInsertReadWriteCSR.cpp:45` plus `YSXTargetMachine.cpp:96` and `YSXTargetMachine.cpp:380` keep a no-op CSR pass; `YSXMachineFunctionInfo.h:111` through `YSXMachineFunctionInfo.h:134` and `YSXFrameLowering.cpp:170` through `YSXFrameLowering.cpp:200` keep copied SiFive CLIC interrupt state/stubs despite `YSXISelLowering.cpp:2280` rejecting interrupt handlers. |
| AC-4 | PARTIAL | YSX-owned tests exist and the reviewed stale inactive unsupported prefixes are gone; targeted MC/CodeGen negative tests passed. Full focused Clang lit could not be rerun because `clang/test/lit.cfg.py` needs to write under the read-only external build test root. Full completion still depends on AC-3 cleanup plus final full revalidation. |

## Forgotten Items Detection

No original plan task is missing from Active/Completed/Deferred tracking after
the tracker update. The tracker did drift before this review: Round 19 treated
the reviewed cleanup as review-pending completion but did not track the newly
found dead operand/CSR/SiFive scaffolding. I updated the mutable section to keep
task3/task6 active and add the current AC-3 blocker.

No task should be considered fully complete solely from the Round 19 summary:
the reported build/lit results are implementation evidence, but this review
could not rerun Ninja or full Clang lit because both configured external build
directories are read-only in the review sandbox.

## Deferred Items Audit

There are no rows in Explicitly Deferred. The CPU/tune target-attribute
warn-and-ignore policy remains a queued side issue, not a deferral. That remains
valid for now because a manual `target("cpu=sifive-p670")` probe produced a
warning, ignored the attribute, and emitted only generic-rv64 rv64ima features.

## Goal Completion Summary

Acceptance Criteria: 2/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 1
Critical blockers: residual dead removed-feature source scaffolding blocks AC-3

## Drift Summary

Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 1
Queued Side Issues: 2

Claude is still serving the original plan. The current round had a clear
mainline objective and removed the exact Round 18 blockers. The remaining issue
is a blocking side issue only because it is still AC-3 source-pruning work, not
because the visible target surface regressed. Queued side issues remain the
immutable tracker AC-list drift and CPU/tune warn-and-ignore policy.

## Implementation Findings

1. AC-3 remains incomplete: dead removed-extension operand metadata and
   validators are still present. `YSXBaseInfo.h` still enumerates VTYPE, Zibi,
   CLUI, and RVKR operand kinds, and `YSXInstrInfo.cpp` still validates them,
   even though no retained rv64ima instruction should need vector, compressed,
   Zibi, or crypto operands. Delete the unused enum values, validator switch
   cases, and any parser helpers made dead by that deletion.

2. AC-3 remains incomplete: the CSR insertion pass is still registered and run
   even though it is a no-op and Zicsr/CSR instructions are intentionally
   rejected. Remove `YSXInsertReadWriteCSR.cpp` and its declarations/
   registration/pipeline hook unless a retained rv64ima path demonstrably needs
   it.

3. AC-3 remains incomplete: copied SiFive CLIC interrupt frame state and stub
   calls remain after interrupt lowering was changed to reject handlers. Remove
   the `InterruptStackKind` SiFive cases, helper accessors, no-op frame-lowering
   stubs, and comments/call sites such as `emitSiFiveCLICStackSwap` and
   `emitSiFiveCLICPreemptibleSaves`.

## Required Action Items

Mainline Gaps:
1. Delete the residual dead removed-feature scaffolding above and simplify any
   generated or switch users to the rv64ima-only set.
2. Rebuild YSX-only and combined RISCV+YSX static targets after the deletion.
3. Rerun focused YSX lit suites, smoke compiles, and negative probes.

Blocking Side Issues:
1. AC-3 residual dead unsupported-source scaffolding after Round 19.

Queued Side Issues:
1. Immutable tracker AC-list drift; continue reviewing against `docs/plan.md`.
2. CPU/tune target-attribute diagnostics; keep queued unless feature leakage is
   observed or the desired policy becomes hard-error-only.

## Goal Tracker Update

I updated the mutable tracker to Plan Version 40, added a Round 19 review
evolution entry, kept task3/task6 active, collapsed stale resolved blocking
rows into the current AC-3 blocker, and marked the Round 19 completed row as
`19 review partial`. I did not modify the immutable section.

## Stagnation Check

Development is not stagnating. Rounds 15 through 19 repeatedly removed distinct
source/test surfaces and reduced YSX from roughly 33k backend lines to 27,237.
The repeated theme is AC-3 pruning, but the concrete blockers have changed and
Claude has addressed the prior reviews rather than cycling on the same exact
defect. Do not trigger the STOP circuit breaker.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-20-contract.md stable for this round
- Do not let queued issues take over the round
- If Codex reported several findings, classify them into:
  - mainline gaps
  - blocking side issues
  - queued side issues
- Only mainline gaps and blocking side issues should drive the next code changes

### Post-Alignment Check Action Items

This round follows a Full Goal Alignment Check. Pay special attention to:
- **Forgotten Items**: Codex may have identified tasks that were being ignored. Address them.
- **AC Status**: If any Acceptance Criteria were marked NOT MET, prioritize work toward those.
- **Deferred Items**: If any deferrals were flagged as unjustified, un-defer them now.
- **Queued Issues**: Keep non-blocking follow-up work queued unless it now clearly blocks mainline progress.

---

Note: You MUST NOT try to exit by lying, editing loop state files, or executing `cancel-rlcr-loop`.

After completing the work, please:
0. If the `code-simplifier` plugin is installed, use it to review and optimize your code. Invoke via: `/code-simplifier`, `@agent-code-simplifier`, or `@code-simplifier:code-simplifier (agent)`
1. Commit your changes with a descriptive commit message
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-20-summary.md
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
