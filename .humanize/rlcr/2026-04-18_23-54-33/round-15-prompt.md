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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-15-contract.md

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
# Round 14 Full Goal Alignment Review

Mainline Progress Verdict: ADVANCED

Round 14 advanced the original `docs/plan.md` mainline by deleting the selected
scalable-vector frame-lowering state from `YSXFrameLowering.*` and
`YSXMachineFunctionInfo.h`. The narrow implementation claim is accurate. The
full plan is still incomplete because large copied unsupported source surfaces
remain outside the frame slice.

## Part 1: Goal Tracker Audit

The immutable section in `goal-tracker.md` still lists only AC-1 and AC-2, while
`docs/plan.md` contains AC-1 through AC-4. This is already tracked as mutable
tracker drift, and I audited against `docs/plan.md` as the source of truth
without editing the immutable section.

| AC | Status | Evidence | Blocker | Justification if deferred |
|----|--------|----------|---------|---------------------------|
| AC-1 | MET | YSX exists as a standalone target; existing YSX-only built binaries run; combined RISCV/YSX build products exist; `git diff -- llvm/lib/Target/RISCV \| wc -l` is `0`; no RISCV target files were touched by Round 14. | None found in this review. Full Ninja rerun was blocked by read-only external build dirs, but existing binaries and source inspection do not contradict AC-1. | Not deferred. |
| AC-2 | PARTIAL | Reviewed probes still reject `+v`, `rv64imaf`, scalable-vector IR, and direct `llvm.riscv.vsetvli` with deterministic YSX diagnostics. YSX/RISCV smoke compiles still produce ELF64 RISC-V soft-float objects. | The backend still contains broad vector/FP/RV32/vendor lowering and metadata surfaces that must be deleted before declaring the rv64ima-only surface complete. | Not deferred. |
| AC-3 | PARTIAL | Round 14 removed the selected YSXVec frame state. Searches for `YSXVec`, `ScalableVector`, `StackOffset::getScalable`, `getYSXVec`, `setYSXVec`, `VLENB`, and `vlenb` in `YSXFrameLowering.*` and `YSXMachineFunctionInfo.h` are clean. All-file line counts match Claude's claim: RISCV `136,068`, YSX `51,831`. | Unsupported source remains in `YSXSelectionDAGInfo.h`, `YSXISelLowering.*`, `YSXBaseInfo.h`, TableGen, register/subtarget metadata, generated vector pseudos, and disassembler FP/vector helpers. | Not deferred. |
| AC-4 | PARTIAL | YSX-owned tests exist, and the reviewed negative probes still behave correctly. Claude's lit claim is plausible, but I could not rerun focused lit because the configured external build tree is read-only in this sandbox. | Final coverage must be rerun after the remaining AC-3 deletion work, and stale copied YSX CodeGen check-prefix blocks still need later cleanup. | Not deferred. |

### Forgotten Items Detection

- No original plan task is missing from the mutable tracker: task1, task2,
  task4, and task5 are completed; task3 and task6 remain active.
- The tracker did not yet have a Round-14 "Completed and Verified" row. I added
  a verified-partial Round-14 row and a Round-14 review plan-evolution entry.
- No Round-14 summary item claimed full plan completion. The narrow frame
  deletion claim was verified.
- The known immutable tracker drift remains: AC-3 and AC-4 are absent from the
  immutable section, but the mutable tracker tracks them against `docs/plan.md`.

### Deferred Items Audit

The "Explicitly Deferred" table is empty. The current queued items are not
accepted deferrals and do not contradict the Ultimate Goal as long as they do
not replace the active AC-3 source-pruning work.

### Goal Completion Summary

```
Acceptance Criteria: 1/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 3-5
Critical blockers: residual unsupported backend source surface under AC-3
```

## Part 2: Mainline Drift Audit

The current round's mainline objective was clear and singular: delete
scalable-vector frame-lowering support and per-function YSXVec frame state.
Claude advanced a mainline AC-3 deletion slice rather than chasing side issues.

Blocking Side Issues: 0 outside mainline. The blocker is the remaining AC-3
source-pruning work itself, not a separate side issue.

Queued Side Issues: 3 non-blocking items remain: immutable tracker AC drift,
stale copied CodeGen check-prefix blocks, and the CPU/tune target-attribute
warn-and-ignore policy. Dead disassembler FP/vector helpers are now better
classified as part of the remaining AC-3 mainline cleanup, not as a side issue.

```
Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 0
Queued Side Issues: 3
```

## Part 3: Implementation Review

The Round-14 frame deletion is coherent. `YSXFrameLowering.h` no longer declares
`getStackSizeWithYSXVecPadding`, `getStackIDForScalableVectors`, YSXVec CFI
helpers, `assignYSXVecStackObjectOffsets`, or YSXVec stack probing. 
`YSXMachineFunctionInfo.h` no longer stores or exposes YSXVec stack
size/alignment/padding. `YSXFrameLowering.cpp` now rejects non-default stack IDs
for frame references, reports only `TargetStackID::Default` as supported, uses
`MFI.getStackSize()`, and removed the scalable CFI/probing/callee-save paths.

Claude's validation claims mostly match reality:

- Verified `git diff --check HEAD^..HEAD` is clean.
- Verified `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`.
- Verified the targeted frame/MFI pattern search has no matches.
- Verified deleted YSXVec frame accessors/helpers have no remaining YSX matches.
- Verified YSX/RISCV smoke compiles using existing build products both produce
  ELF64 RISC-V soft-float relocatable objects.
- Verified negative probes reject `+v`, `rv64imaf`, scalable-vector IR, and
  direct `llvm.riscv.vsetvli`.
- Could not rerun Ninja or focused lit because
  `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm` is read-only in
  this sandbox and Ninja cannot create `.ninja_lock` or regenerate VCS headers.

No new Round-14 frame-specific bug was found. The remaining problems are the
known AC-3 source-surface gaps:

1. `YSXSelectionDAGInfo.h` still preserves a broad unsupported custom-ISD
   namespace instead of deleting removed nodes. The block starts at
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:19`, with examples such as
   `READ_VLENB` at line 75, tuple nodes at lines 158-159, `VMV_*` around lines
   208-211, and `VRGATHER*` around lines 216-219.

2. `YSXISelLowering.*` still carries copied vector/RISCVVType/YSXVec lowering.
   Representative examples include `RISCVVType::VLMUL` in
   `YSXISelLowering.h:346`, YSXVec helper declarations around lines 547, 584,
   and 607, the generated `YSXVIntrinsicsTable` declarations at line 656, and
   extensive `YSXISD::READ_VLENB`, `VMV_V_X_VL`, `VRGATHER`, tuple, VP, and
   vector-intrinsic handling in `YSXISelLowering.cpp`.

3. BaseInfo and TableGen still encode removed vector/FP metadata.
   `YSXBaseInfo.h` retains VLMUL/vector TSFlag fields around lines 72-99,
   vector operand kinds at lines 438 and 444, FP rounding helpers at line 479,
   and YSXVec block constants at line 770. `YSXInstrFormats.td` retains
   YSXVec constraints and vector TSFlags around lines 209-228.

4. Register/subtarget/generated pseudo surfaces remain. `YSXRegisterInfo.td`
   still contains vector register-class fields such as `VLMul` at line 187;
   `YSXSubtarget.h` still exposes VLEN/YSXVec helpers around lines 276 and
   346; `YSXInstrInfo.td` still defines `PROBED_STACKALLOC_YSXVec` at line
   1308. These are remaining AC-3 cleanup targets, even though the frame code no
   longer consumes the YSXVec stack pseudo.

5. Dead FP/vector disassembler helpers remain, including FP rounding decode
   checks in `YSXDisassembler.cpp:412` and `YSXDisassembler.cpp:422`.

## Part 4: Goal Tracker Update

I updated the mutable section of
`.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`:

- Bumped the tracker to Plan Version 30.
- Added a Round-14 review entry to the Plan Evolution Log.
- Added a verified-partial Round-14 row under Completed and Verified.
- Kept task3 and task6 active.
- Did not edit the immutable section.

## Part 5: Progress Stagnation Check

Development is not stagnant. Rounds 10 through 14 repeatedly report the same
overall AC-3 incompleteness, but each round removed a distinct source slice or
fixed a concrete unsupported-input behavior:

- Round 10 removed direct `RISCVVType` metadata/instruction-info users from
  named files.
- Round 11 removed instruction-info vector/FP verifier/comment/pseudo stubs.
- Round 12 added deterministic unsupported-vector IR diagnostics and removed a
  lowering/DAG crash-surface slice.
- Round 13 removed DAG selector vector/tuple residue and disabled blocks.
- Round 14 removed scalable-vector frame state and stack-ID support.

The remaining work is still large, but the recent rounds show meaningful code
deletion and verification rather than circular discussion or repeated failure.
Do not write STOP.

## Required Action Items

### Mainline Gaps

1. Delete `YSX_UNSUPPORTED_ISD` and every unsupported custom-ISD caller that it
   keeps alive. Keep only rv64ima scalar YSXISD nodes.
2. Collapse `YSXISelLowering.h` and `YSXISelLowering.cpp` to scalar rv64ima:
   remove `RISCVVType`, YSXVec container helpers, VP/vector lowering,
   generated vector intrinsic tables, VLEN handling, vector reductions, vector
   combines, and tuple lowering.
3. Prune BaseInfo/TableGen/RegisterInfo/Subtarget metadata: remove vector
   TSFlags, vector operand kinds, FP rounding helpers not used by rv64ima,
   generated vector pseudo tables, RV32 hardware-mode scaffolding, VLEN/YSXVec
   helper APIs, and `PROBED_STACKALLOC_YSXVec`.
4. Remove dead FP/vector disassembler and MC helper surfaces once the generated
   decoder/table definitions no longer require them.
5. Revalidate after each coherent deletion cluster and keep
   `git diff -- llvm/lib/Target/RISCV | wc -l` at `0`.

### Blocking Side Issues

None outside the mainline gaps. The current blocking issue is mainline AC-3
incompleteness.

### Queued Side Issues

1. Trim stale copied YSX CodeGen check-prefix blocks after the source surface is
   rv64ima-only.
2. Decide later whether unsupported `target("cpu=...")` and
   `target("tune=...")` attributes should remain warn-and-ignore or become hard
   errors.
3. Leave the immutable tracker AC omission alone unless the RLCR tool requires
   regenerating the immutable section.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-15-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-15-summary.md
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
