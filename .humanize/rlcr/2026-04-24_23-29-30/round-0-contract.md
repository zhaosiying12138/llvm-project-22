# Round 0 Contract

## Mainline Objective

Establish the local RISCV-only build and implement the experimental Yushuxin vector `llvm.exp` lowering path with feature gating and focused tests.

## Target ACs

- AC-1: Default behavior is unchanged without the new feature or hidden flags.
- AC-2: Vector `llvm.exp.*` lowers to `yushuxin.vfexp` only for supported RVV floating vector types with `+experimental-yushuxin-vfexp`.

## Blocking Side Issues In Scope

- Build configuration failures that prevent compiling RISCV TableGen or `llc`.
- TableGen or SelectionDAG integration failures that prevent the Yushuxin instruction from being selected under the feature.

## Queued Side Issues Out Of Scope

- Scheduler strategy tuning for AC-3.
- Safe reload rematerialization for AC-4.
- Frame diagnostics for AC-5.
- Full workload measurement and `docs/report.md` for AC-6.

## Round Success Criteria

- Build directory is configured inside the worktree.
- The Yushuxin feature, instruction, pseudo, and lowering are implemented behind `+experimental-yushuxin-vfexp`.
- Focused CodeGen tests demonstrate both feature-enabled emission and default-off behavior.
- Relevant build/test commands are run or, if blocked, the blocker is recorded.
