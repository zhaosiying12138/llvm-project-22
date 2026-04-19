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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-23-contract.md

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
# Round 22 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 22 advanced the mainline by deleting the exact selector residue called
out in Round 21: the scaled-address complex patterns, SHXADD helpers, SiFive
selector hook, and vector-combine declaration are gone, and the targeted scan
has no YSX matches. The original `docs/plan.md` goal is still not complete,
because YSX still carries copied removed-feature/tuning scaffolding outside the
selector slice.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MAINTAINED. RISCV source diff remains zero, and the available built
  YSX/RISCV tools still produce ELF64 RISC-V soft-float smoke objects.
- AC-2: MAINTAINED for reviewed probes. Unsupported `rv64imaf`, explicit
  `+v`, FP CSR assembly, scalable-vector IR, direct `llvm.riscv.vsetvli`,
  removed `.insn` opcode names/values, and `.insn r4` reject.
- AC-3: PARTIAL. The Round-22 selector residue is gone, but residual
  short-forward-branch/CCMOV tuning scaffolding and dead vector/FP
  scheduler/custom-ISD helper scaffolding remain.
- AC-4: MAINTAINED for probes, but full lit could not be rerun in this Codex
  sandbox because `clang/test/lit.cfg.py` tries to create files under the
  external read-only build test exec root.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 46 records this Round-22 review, keeps task3/task6 active, replaces the
resolved selector blocker with the residual SFB/vector-scaffolding blocker, and
marks the Round-22 selector slice as review-partial. The immutable section was
not modified.

## Mainline Gaps

1. AC-3 remains incomplete: YSX still has copied short-forward-branch/CCMOV
   tuning scaffolding after the corresponding pseudo opcode surface was removed.

   `llvm/lib/Target/YuShuXin/YSXFeatures.td:179`,
   `YSXFeatures.td:186`, `YSXFeatures.td:193`, and
   `YSXFeatures.td:200` still define short-forward-branch tuning features and
   predicates. `llvm/lib/Target/YuShuXin/YSXSchedule.td:49` still carries the
   copied "Bullet" SFB scheduler resources. The C++ hooks are also still live
   in source: `YSXISelLowering.cpp:158` gates ABS lowering on
   `hasShortForwardBranchIALU`, `YSXISelLowering.cpp:3529` keeps an SFB-only
   `BuildSDIVPow2` override, `YSXSubtarget.h:139` wires conditional-move
   fusion to the SFB flag, and `YSXInstrInfo.cpp:860` through
   `YSXInstrInfo.cpp:945` keep the predicated-op/select optimization helpers.

   This is not just cosmetic. `YSXInstrInfo.cpp:915` and
   `YSXInstrInfo.cpp:937` still assert against `YSX::PseudoCCMOVGPR`, but
   `rg "PseudoCCMOVGPR|PseudoCCSUB" llvm/lib/Target/YuShuXin` shows no
   TableGen definition for that removed pseudo surface. The current configured
   Release builds have `LLVM_ENABLE_ASSERTIONS=OFF`, so this stale reference is
   compiled out there; an assertions-enabled YSX build would compile the assert
   expression and expose the broken residue. A backend that only meets the plan
   in assertion-free builds is not safely complete.

2. AC-3 remains incomplete: dead vector/FP scheduler and custom-ISD helper
   scaffolding remains.

   `llvm/lib/Target/YuShuXin/YSXInstrPredicates.td:13` through
   `YSXInstrPredicates.td:17` still define vector load/store and single-element
   vector-FP scheduling predicates as false placeholders. `YSXSchedule.td:58`
   and `YSXSchedule.td:60` still define FP memory scheduler reads that have no
   rv64ima role. `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:21` through
   `YSXSelectionDAGInfo.h:52` still carries vector custom-ISD TSFlag helpers
   and a `getMAccOpcode` method whose only behavior is an unreachable message
   about vector multiply-add nodes. These are the same kind of copied
   removed-feature scaffolding that the plan and prior reviews require pruning.

## Blocking Side Issues

None separate from the mainline gaps above. The remaining issues are
plan-derived AC-3 source-pruning work.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. The
   reviewed probes still do not show unsupported feature leakage through those
   warnings.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. Delete the short-forward-branch feature surface from
   `llvm/lib/Target/YuShuXin/YSXFeatures.td`: remove
   `TuneShortForwardBranchIALU`, `TuneShortForwardBranchIMinMax`,
   `TuneShortForwardBranchIMul`, `TuneShortForwardBranchILoad`, and their
   `Has*`/`NoShortForwardBranch` predicates.
2. Delete the SFB scheduler resources from `YSXSchedule.td` and the dead
   vector/FP scheduler placeholders from `YSXInstrPredicates.td` and
   `YSXSchedule.td`.
3. Delete the SFB/CCMOV C++ hooks: remove `hasConditionalMoveFusion` from
   `YSXSubtarget.h`; remove `getPredicatedOpcode`, `canFoldAsPredicatedOp`,
   `YSXInstrInfo::analyzeSelect`, and `YSXInstrInfo::optimizeSelect` from
   `YSXInstrInfo.cpp`; remove the matching declarations from `YSXInstrInfo.h`.
4. Remove the SFB lowering hooks from `YSXISelLowering.*`: always use the
   existing non-SFB ABS path, and delete the SFB-only `BuildSDIVPow2` override
   declaration/definition.
5. Delete the unused vector custom-ISD helper surface from
   `YSXSelectionDAGInfo.h` and the corresponding `HasPassthruOp`/`HasMaskOp`
   bits from the YSX SDNode TableGen base if no retained node sets them.
6. Require this scan to have no matches outside intentionally retained negative
   diagnostic text:
   `rg "ShortForwardBranch|short-forward-branch|SFB|PseudoCCMOV|PseudoCCSUB|VLDSX0Pred|SingleElementVecFP64SchedPred|ReadFMemBase|ReadFStoreData|HasPassthruOp|HasMaskOp|getMAccOpcode|vector multiply-add" llvm/lib/Target/YuShuXin`.
7. Rebuild both configured target sets, rerun the focused YSX lit suites, rerun
   the retained rv64ima smoke tests and unsupported-feature probes, run
   `git diff --check`, and verify `git diff -- llvm/lib/Target/RISCV | wc -l`
   returns `0`. Because the stale `PseudoCCMOVGPR` references are assertion-only
   in the current Release builds, also run an assertions-enabled YSX codegen
   compile/build check for the touched files.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-23-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-23-summary.md
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
