Your work is not finished. Read and execute the below with ultrathink.

## Original Implementation Plan

**IMPORTANT**: Before proceeding, review the original plan you are implementing:
@docs/plan.md

This plan contains the full scope of work and requirements. Ensure your work aligns with this plan.

---

## Round Re-anchor (REQUIRED FIRST STEP)

Before writing code:
- Re-read @docs/plan.md
- Re-read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md
- Re-read the most recent round summaries/reviews that led to this round
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-1-contract.md

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
1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each task/sub-task
3. Follow selected lesson IDs (or `NONE`) during implementation

---
Below is Codex's review result:
<!-- CODEX's REVIEW RESULT START -->
Mainline Progress Verdict: ADVANCED

Goal Alignment Summary

ACs: 6/6 addressed | Forgotten items: 0 | Unjustified deferrals: 0

**Mainline Gaps**

1. `rvv-spill-slots` is not a spill-slot count, so AC-5 is not complete. In `llvm/lib/Target/RISCV/RISCVFrameLowering.cpp:1819-1830`, the report counts every `TargetStackID::ScalableVector` object, not actual spill slots. A simple repro with only an RVV alloca:

   ```sh
   build-riscv/bin/llc -mtriple=riscv64 -mattr=+v,+zvl128b -riscv-v-reg-pressure-report -o /dev/null - <<'EOF'
   define void @sv_alloca() {
   entry:
     %x = alloca <vscale x 4 x i32>, align 16
     ret void
   }
   EOF
   ```

   prints `riscv-v-reg-pressure-report: function=sv_alloca rvv-scalable-stack-bytes=16 rvv-spill-slots=1 fixed-stack-estimate=0`, even though there are zero spills. The tracker claimed AC-5 verified, but the implementation currently overcounts non-spill scalable vector frame objects.

2. AC-3 and AC-4 are under-tested relative to the plan, and the summary overstates verification. The only scheduler/reload regression file, `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll:1-17`, has only flag-enabled RUN lines and positive checks. It does not include the required default-off negative test for `-riscv-v-reg-pressure-aware-sched`, and it does not cover the promised reload-pass safety cases for volatile, atomic, may-alias-store, or call-intervened scenarios. The lit run in the summary was just 4 focused tests, so the claim that existing RISCV CodeGen tests kept passing without the new flags is not actually verified either.

3. The scheduler scope is broader than the planned conservative design. `llvm/lib/Target/RISCV/RISCVTargetMachine.cpp:292-300` disables generic load/store clustering for the whole function whenever `-riscv-v-reg-pressure-aware-sched` is set, while the region-pressure check is computed later in `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:101-106`. That means non-pressure regions lose clustering too, which does not match the plan’s “only for pressure-heavy vector regions” constraint.

4. AC-6 is not fully satisfied by the current report. `docs/report.md:30-55` uses placeholder `INPUT.ll` commands instead of exact reproducible commands, does not explain how the per-workload counts in `docs/report.md:61-68` were isolated from the shared multi-function workload file, and does not include the diagnostics snippets that the plan required alongside the counts.

**Blocking Side Issues**

- None beyond the mainline gaps above.

**Queued Side Issues**

- `goal-tracker.md` had drifted ahead of the evidence by marking AC-3 through AC-6 as completed and verified. I corrected the mutable section directly so the next round starts from an accurate state.

**Directive Next-Step Plan**

1. Fix AC-5 first: change the diagnostics path in `RISCVFrameLowering` to count true spill-slot frame indices rather than every scalable-vector object, then add a regression that proves an RVV alloca does not increment `rvv-spill-slots`.
2. Tighten AC-3 to the planned scope: keep the hidden flag default-off, but only suppress clustering when the current scheduling region is actually classified as high-pressure RVV. Add one default-off negative test and one masked-vector validity check.
3. Finish AC-4 verification with a dedicated reload-pass regression file that contains one same-block safe positive case and explicit negative cases for volatile, atomic, may-alias store, and call-intervened patterns.
4. Rewrite `docs/report.md` with exact commands used for each required workload and optimization level, include the report-flag diagnostics output, and document the exact spill/reload counting procedure used to derive every table row.
<!-- CODEX's REVIEW RESULT  END  -->
---

## Goal Tracker Reference

Before starting work, **read** @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md to understand:
- The Ultimate Goal and Acceptance Criteria you're working toward
- Which tasks are Active, Completed, or Deferred
- Which side issues are blocking vs queued
- Any Plan Evolution that has occurred
- The latest side-issue state that needs attention

**IMPORTANT**: Keep the mutable section of `goal-tracker.md` up to date during the round.
Do NOT change the immutable section after Round 0.
If you cannot safely reconcile the tracker yourself, include an optional "Goal Tracker Update Request" section in your summary (see below).

## Mainline Guardrails

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-1-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-1-summary.md
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
