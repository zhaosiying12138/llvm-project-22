# RISCV RVV Register-Pressure-Aware Scheduling Plan

## Goal Description

Implement a default-off RISCV RVV register-pressure-aware scheduling experiment in LLVM 22.1.3. The branch adds an experimental Yushuxin vector exponential instruction, a RISCV-specific pre-register-allocation scheduler strategy, a conservative pre-scheduler RVV load rematerialization pass, frame diagnostics, focused CodeGen tests, and a measured report.

CMT:
The quantitative acceptance criteria are trend-based rather than absolute performance gates. The implementation should maximize spill reduction in the local synthetic workloads while preserving correctness and default behavior.
ENDCMT

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
