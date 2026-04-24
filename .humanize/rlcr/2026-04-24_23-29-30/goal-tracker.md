# Goal Tracker

<!--
This file tracks the ultimate goal, acceptance criteria, and plan evolution.
It prevents goal drift by maintaining a persistent anchor across all rounds.

RULES:
- IMMUTABLE SECTION: Do not modify after initialization
- MUTABLE SECTION: Update each round, but document all changes
- Every task must be in one of: Active, Completed, or Deferred
- Deferred items require explicit justification
-->

## IMMUTABLE SECTION
<!-- Do not modify after initialization -->

### Ultimate Goal

Implement a default-off RISCV RVV register-pressure-aware scheduling experiment in LLVM 22.1.3. The branch adds an experimental Yushuxin vector exponential instruction, a RISCV-specific pre-register-allocation scheduler strategy, a conservative pre-scheduler RVV load rematerialization pass, frame diagnostics, focused CodeGen tests, and a measured report.

The quantitative acceptance criteria are trend-based rather than absolute performance gates. The implementation should maximize spill reduction in the local synthetic workloads while preserving correctness and default behavior.

### Acceptance Criteria
<!-- Each criterion must be independently verifiable -->

- AC-1: Default behavior is unchanged when no new feature or hidden flag is enabled.
- AC-2: Vector `llvm.exp.*` lowers to `yushuxin.vfexp` only for supported RVV floating vector types with `+experimental-yushuxin-vfexp`.
- AC-3: `-riscv-v-reg-pressure-aware-sched` enables a RISCV scheduler strategy that reduces long RVV live ranges in high-pressure regions.
- AC-4: The safe reload rematerialization pass only rewrites provably safe same-block RVV loads.
- AC-5: `-riscv-v-reg-pressure-report` prints useful RVV stack diagnostics.
- AC-6: `docs/report.md` records exact commands, spill/reload counts, diagnostics, and before/after snippets for the required workloads.

## MUTABLE SECTION
<!-- Update each round with justification for changes -->

### Plan Version: 1 (Updated: Round 0)

#### Plan Evolution Log
<!-- Document any changes to the plan with justification -->
| Round | Change | Reason | Impact on AC |
|-------|--------|--------|--------------|
| 0 | Initial plan | Extracted from `docs/plan.md` | Defines AC-1 through AC-6 |
| 0 | Completed full implementation scope in this pass | The active user instruction was to implement the full prior plan in a fresh context, not stop at the narrowed round contract | AC-1 through AC-6 completed and verified locally |

#### Active Tasks
<!-- Mainline tasks only: each task must directly advance the current round objective and carry routing metadata -->
| Task | Target AC | Status | Tag | Owner | Notes |
|------|-----------|--------|-----|-------|-------|

### Blocking Side Issues
<!-- Only issues that directly block current mainline progress belong here -->
| Issue | Discovered Round | Blocking AC | Resolution Path |
|-------|-----------------|-------------|-----------------|

### Queued Side Issues
<!-- Non-blocking issues stay queued and must NOT replace the round objective -->
| Issue | Discovered Round | Why Not Blocking | Revisit Trigger |
|-------|-----------------|------------------|-----------------|
| Full scheduler/reload/report implementation may exceed a single RLCR round because setup was capped at `--max 1`. | 0 | Resolved in this pass because the implementation completed AC-3 through AC-6 before stop-gate review. | No revisit needed unless stop gate asks for another round. |

### Completed and Verified
<!-- Only move tasks here after Codex verification -->
| AC | Task | Completed Round | Verified Round | Evidence |
|----|------|-----------------|----------------|----------|
| AC-1..AC-6 | Planning docs and RLCR setup | 0 | 0 | `docs/plan.md`, `.humanize/rlcr/2026-04-24_23-29-30/` |
| AC-1 | RISCV-only build setup | 0 | 0 | `cmake -S llvm -B build-riscv ...`; `ninja -C build-riscv llc clang lld FileCheck llvm-mc llvm-objdump count not llvm-config llvm-readobj` |
| AC-1, AC-2 | Experimental Yushuxin vector exp lowering and feature gating | 0 | 0 | `llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll`, `llvm/test/MC/RISCV/rvv/yushuxin-vfexp.s`, `llvm/test/CodeGen/RISCV/features-info.ll`; lit passed |
| AC-3 | Pressure-aware scheduler strategy and cluster suppression under hidden flag | 0 | 0 | `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp`, `llvm/lib/Target/RISCV/RISCVTargetMachine.cpp`; workload lit passed |
| AC-4 | Conservative same-block RVV reload rematerialization pass | 0 | 0 | `llvm/lib/Target/RISCV/RISCVVRegPressureReload.cpp`; workload lit passed and measurements recorded |
| AC-5 | RVV frame diagnostics | 0 | 0 | `-riscv-v-reg-pressure-report` checks in `llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll` passed |
| AC-6 | Workload tests and measured report | 0 | 0 | `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll`, `docs/report.md`; lit passed |

### Explicitly Deferred
<!-- Items here require strong justification -->
| Task | Original AC | Deferred Since | Justification | When to Reconsider |
|------|-------------|----------------|---------------|-------------------|
