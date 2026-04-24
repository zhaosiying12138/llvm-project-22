Read and execute below with ultrathink

## Goal Tracker Setup (REQUIRED FIRST STEP)

Before starting implementation, you MUST initialize the Goal Tracker:

1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md
2. If the "Ultimate Goal" section says "[To be extracted...]", extract a clear goal statement from the plan
3. If the "Acceptance Criteria" section says "[To be defined...]", define 3-7 specific, testable criteria
4. Populate the "Active Tasks" table with MAINLINE tasks from the plan, mapping each to an AC and filling Tag/Owner
5. Record any already-known side issues in either "Blocking Side Issues" or "Queued Side Issues"
6. Write the updated goal-tracker.md

## Round Contract Setup (REQUIRED BEFORE CODING)

Before starting implementation, create @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-0-contract.md with:

1. **One mainline objective** for this round
2. **Target ACs** (1-2 ACs only)
3. **Blocking side issues in scope** for this round
4. **Queued side issues out of scope** for this round
5. **Round success criteria**

Use this contract to keep the round focused. Do NOT let non-blocking bugs or cleanup work replace the mainline objective.

**IMPORTANT**: The IMMUTABLE SECTION can only be modified in Round 0. After this round, it becomes read-only.

---

## Implementation Plan

For all tasks that need to be completed, please use the Task system (TaskCreate, TaskUpdate, TaskList).

Every task MUST start with exactly one lane tag:
- `[mainline]` for plan-derived work that directly advances the round objective
- `[blocking]` for issues that prevent the mainline objective from succeeding safely
- `[queued]` for non-blocking bugs, cleanup, or follow-up work

Rules:
- `[mainline]` tasks are the primary success condition for the round
- `[blocking]` tasks may be resolved in the round only if they truly block mainline progress
- `[queued]` tasks must NOT become the round objective and do NOT need to be cleared before moving on
- If a new issue is not blocking the current objective, tag it `[queued]` and keep moving on the mainline

## Task Tag Routing (MUST FOLLOW)

Each task must have one routing tag from the plan: `coding` or `analyze`.

- Tag `coding`: Claude executes the task directly.
- Tag `analyze`: Claude must execute via `/humanize:ask-codex`, then integrate Codex output.
- Keep Goal Tracker "Active Tasks" columns **Tag** and **Owner** aligned with execution (`coding -> claude`, `analyze -> codex`).
- If a task has no explicit tag, default to `coding` (Claude executes directly).

# RISCV RVV Register-Pressure-Aware Scheduling Plan

## Goal Description

Implement a default-off RISCV RVV register-pressure-aware scheduling experiment in LLVM 22.1.3. The branch adds an experimental Yushuxin vector exponential instruction, a RISCV-specific pre-register-allocation scheduler strategy, a conservative pre-scheduler RVV load rematerialization pass, frame diagnostics, focused CodeGen tests, and a measured report.

The quantitative acceptance criteria are trend-based rather than absolute performance gates. The implementation should maximize spill reduction in the local synthetic workloads while preserving correctness and default behavior.

## Acceptance Criteria

- AC-1: Default behavior is unchanged when no new feature or hidden flag is enabled.
  - Positive Tests (expected to PASS):
    - Existing RISCV CodeGen tests continue to pass without the new flags.
    - New tests check that normal `llvm.exp` lowering does not emit `yushuxin.vfexp` without `+experimental-yushuxin-vfexp`.
  - Negative Tests (expected to FAIL):
    - A test expecting `yushuxin.vfexp` without the feature should fail.

- AC-2: Vector `llvm.exp.*` lowers to `yushuxin.vfexp` only for supported RVV floating vector types with `+experimental-yushuxin-vfexp`.
  - Positive Tests (expected to PASS):
    - A `<128 x float>` `llvm.exp` test emits `yushuxin.vfexp`.
    - Assembler/disassembler tests accept the experimental mnemonic when the feature is present.
  - Negative Tests (expected to FAIL):
    - Unsupported element types or missing feature should not select the Yushuxin instruction.

- AC-3: `-riscv-v-reg-pressure-aware-sched` enables a RISCV scheduler strategy that reduces long RVV live ranges in high-pressure regions.
  - Positive Tests (expected to PASS):
    - MIR or IR tests show fewer RVV whole-register spills/reloads with the flag for high-pressure workloads.
    - Scheduling remains valid for masked vector operations.
  - Negative Tests (expected to FAIL):
    - A test assuming the new scheduling without the hidden flag should fail.

- AC-4: The safe reload rematerialization pass only rewrites provably safe same-block RVV loads.
  - Positive Tests (expected to PASS):
    - A noalias reduce-plus-late-use case gets a cloned RVV load before late uses.
    - Volatile, atomic, ordered, side-effecting, may-alias store, and call-intervened cases are skipped.
  - Negative Tests (expected to FAIL):
    - A may-alias or volatile case expecting cloned reloads should fail.

- AC-5: `-riscv-v-reg-pressure-report` prints useful RVV stack diagnostics.
  - Positive Tests (expected to PASS):
    - Code generation with the report flag prints function name, scalable RVV stack bytes, vector spill-slot count, and fixed stack estimate.
  - Negative Tests (expected to FAIL):
    - The same diagnostics should not appear without the flag.

- AC-6: `docs/report.md` records the exact commands, spill/reload counts, diagnostics, and before/after snippets for the required workloads.
  - Positive Tests (expected to PASS):
    - The report includes baseline and optimized `-O2` and `-O3` measurements with `VLEN=1024`.
  - Negative Tests (expected to FAIL):
    - Missing command lines or missing spill/reload counts should be considered incomplete.

## Path Boundaries

### Upper Bound (Maximum Scope)

Implement SelectionDAG vector exp lowering, experimental instruction encoding, a RISCV pre-RA scheduler strategy, a conservative RVV load rematerialization pass, stack diagnostics, regression tests, benchmark-style lit inputs, local build configuration, and a report. Tune synthetic workloads when needed to show clear spill/reload trends.

### Lower Bound (Minimum Scope)

Implement default-off hooks for the feature and hidden flags, compile the target, add tests that prove gating and safety, and provide a report that honestly records any cases where spill reduction is not achieved.

### Allowed Choices

- Can use: existing RISCV TableGen RVV pseudo patterns, SelectionDAG lowering, existing MachineScheduler extension points, MachineFunction passes, MachineMemOperand alias checks, `FileCheck`, `llvm-lit`, local helper scripts, and `docs/` artifacts.
- Cannot use: default-on target changes, unsafe memory speculation, untracked generated build products in the source tree, pushed commits, or changes to unrelated targets.

## Feasibility Hints and Suggestions

- Start from existing `VFSQRT` and upstream SiFive `sf.vfexp` structures instead of inventing new RVV pseudo machinery.
- Keep the new scheduler conservative: bias ordering and cluster suppression only under the flag, and only for pressure-heavy vector regions.
- Treat the reload pass as a best-effort optimization. Skipping ambiguous memory is correct.
- Use synthetic fully unrolled IR to make the pressure effect measurable and reproducible.
- Strict `fexp`, GlobalISel lowering, dynamic loop-vectorized cases, and unsafe alias cases are out of scope for this branch.

## Dependencies and Sequence

### Milestones

1. Milestone 1: Worktree, build, and planning setup
   - Phase A: Create the requested worktree and branch from `llvmorg-22.1.3`.
   - Phase B: Configure a RISCV-only build with clang, lld, ninja, ccache, Release+Assertions.
   - Phase C: Add draft, annotated plan, refined plan, and RLCR state.

2. Milestone 2: Yushuxin vector exp lowering
   - Phase A: Add feature and TableGen instruction/pseudos.
   - Phase B: Wire SelectionDAG action/selection for supported RVV float vectors.
   - Phase C: Add CodeGen and assembler tests.

3. Milestone 3: Scheduler strategy
   - Phase A: Add hidden option and RISCV `GenericScheduler` subclass.
   - Phase B: Detect high vector register pressure and bias scheduling.
   - Phase C: Suppress generic load/store clustering under the flag.

4. Milestone 4: Safe reload rematerialization
   - Phase A: Add the pre-scheduler pass after the VL optimizer.
   - Phase B: Restrict transformations to simple same-block RVV loads with proven alias safety.
   - Phase C: Add safety and positive tests.

5. Milestone 5: Diagnostics, workloads, and report
   - Phase A: Add stack diagnostics under the report flag.
   - Phase B: Add the four required workload tests.
   - Phase C: Run baseline/optimized measurements and write `docs/report.md`.

## Task Breakdown

- [coding] Add planning docs and start RLCR.
- [coding] Configure the local build directory.
- [coding] Implement `+experimental-yushuxin-vfexp` and `yushuxin.vfexp`.
- [coding] Implement the scheduler option and strategy.
- [coding] Implement the safe reload pass and pipeline placement.
- [coding] Implement frame diagnostics.
- [coding] Add regression/workload tests and update checks.
- [analyze] Collect spill/reload counts and write the report.

## Claude-Codex Deliberation

The design favors isolated, opt-in behavior over broad scheduler changes. The main implementation risk is overfitting the scheduler heuristic; tests and report should describe trends rather than claim universal improvement. The reload pass must be correct first and profitable second.

## Pending User Decisions

None. The implementation assumes the reserved Yushuxin encoding is temporary, trend metrics are acceptable, and unsafe cases are skipped.

## Implementation Notes

- Code should not contain plan-specific labels such as AC identifiers.
- New command-line options should be hidden and default-off.
- The final report should include exact commands and measured counts, even when a workload does not improve as expected.

---

## BitLesson Selection (REQUIRED FOR EACH TASK)

Before executing each task or sub-task, you MUST:

1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each task/sub-task to select relevant lesson IDs
3. Follow the selected lesson IDs (or `NONE`) during implementation

Include a `## BitLesson Delta` section in your summary with:
- Action: none|add|update
- Lesson ID(s): NONE or comma-separated IDs
- Notes: what changed and why (required if action is add or update)

Reference: @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/bitlesson.md

---

## Goal Tracker Rules

Throughout your work, you MUST maintain the Goal Tracker:

1. **Before starting a round**: Re-anchor on the original plan and current round contract
2. **Before starting a task**: Mark the relevant mainline task as "in_progress" in Active Tasks
   - Confirm Tag/Owner routing is correct before execution
3. **Active Tasks** are MAINLINE tasks only - side issues do not belong there
4. **Blocking Side Issues** are reserved for issues that truly stop mainline progress
5. **Queued Side Issues** are non-blocking and must not take over the round
6. **After completing a mainline task**: Move it to "Completed and Verified" with evidence (but mark as "pending verification")
7. **If you discover the plan has errors**:
   - Do NOT silently change direction
   - Add entry to "Plan Evolution Log" with justification
   - Explain how the change still serves the Ultimate Goal
8. **If you need to defer a task**:
   - Move it to "Explicitly Deferred" section
   - Provide strong justification
   - Explain impact on Acceptance Criteria
9. **If you discover new issues**:
   - Add to "Blocking Side Issues" only if mainline progress is blocked
   - Otherwise add to "Queued Side Issues" or keep them as `[queued]` tasks/backlog

---

Note: You MUST NOT try to exit `start-rlcr-loop` loop by lying or edit loop state file or try to execute `cancel-rlcr-loop`

After completing the work, please:
0. If you have access to the `code-simplifier` agent, use it to review and optimize the code you just wrote
1. Finalize @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md (this is Round 0, so you are initializing it - see "Goal Tracker Setup" above)
2. Write your round contract into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-0-contract.md
3. Commit your changes with a descriptive commit message
4. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-0-summary.md
5. Run `/home/zhaosiying/.codex/skills/humanize/scripts/rlcr-stop-gate.sh` to advance the loop in-session
