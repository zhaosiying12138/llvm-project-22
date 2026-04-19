# Code Review Findings

You are in the **Review Phase**. Codex has performed a code review and found issues that need to be addressed.

## Required Re-anchor

Before touching code:
- Re-read the original plan at @docs/plan.md
- Re-read the goal tracker at @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md
- Refresh the current round contract at @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-32-contract.md

The round contract must preserve a single mainline objective. Code review findings do NOT automatically become the new round objective.

## Review Results

## Codex Review Issues

- [P1] Guard YSX tests behind the experimental target — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/test/Driver/YSX/target-options.c:3-6
  Because YSX is registered only as an experimental target, normal `check-clang`/`check-llvm` builds that do not set `LLVM_EXPERIMENTAL_TARGETS_TO_BUILD=YSX` won't have the YSX backend registered. These new tests run unconditionally and several RUN lines compile objects or invoke `llc`/`llvm-mc` with `ysx64`, so the default test suite fails with an unavailable target; add `lit.local.cfg`/`REQUIRES: ysx-registered-target` guards for the YSX test directories.

- [P2] Don't route ysx64 through RV32 multilib selection — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:1115-1116
  By folding `ysx64` into `isRISCV64()`, every `isRISCV()` caller now treats YSX as RISC-V; one existing caller is `Generic_GCC::findRISCVMultilibs`, which still checks `TargetTriple.getArch() == riscv64` and therefore treats `ysx64` as `-m32`. On Linux toolchains with the usual RISC-V multilib layout this prevents selecting the `lib64/lp64` multilib/runtime paths, so the affected call sites need to use `isRISCV64()`/`isArch64Bit()` or YSX should stay out of the generic RISC-V predicate.
The patch introduces unguarded tests that will fail in default builds without the experimental YSX target, and the Triple predicate change misroutes ysx64 through existing RISC-V multilib logic that still distinguishes only riscv64 by enum value.

Full review comments:

- [P1] Guard YSX tests behind the experimental target — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/test/Driver/YSX/target-options.c:3-6
  Because YSX is registered only as an experimental target, normal `check-clang`/`check-llvm` builds that do not set `LLVM_EXPERIMENTAL_TARGETS_TO_BUILD=YSX` won't have the YSX backend registered. These new tests run unconditionally and several RUN lines compile objects or invoke `llc`/`llvm-mc` with `ysx64`, so the default test suite fails with an unavailable target; add `lit.local.cfg`/`REQUIRES: ysx-registered-target` guards for the YSX test directories.

- [P2] Don't route ysx64 through RV32 multilib selection — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:1115-1116
  By folding `ysx64` into `isRISCV64()`, every `isRISCV()` caller now treats YSX as RISC-V; one existing caller is `Generic_GCC::findRISCVMultilibs`, which still checks `TargetTriple.getArch() == riscv64` and therefore treats `ysx64` as `-m32`. On Linux toolchains with the usual RISC-V multilib layout this prevents selecting the `lib64/lp64` multilib/runtime paths, so the affected call sites need to use `isRISCV64()`/`isArch64Bit()` or YSX should stay out of the generic RISC-V predicate.

## Issue Classification

Classify each review finding before acting on it:
- **blocking side issue**: prevents the current mainline objective from succeeding safely or prevents review acceptance
- **queued side issue**: valid follow-up, but does not block the current round objective

Queued issues may be documented, but they must NOT take over the round.

## Task Rules

Every task must use one lane tag:
- `[blocking]` for review findings that must be fixed now
- `[queued]` for non-blocking follow-up work

Do not create new `[mainline]` tasks in review phase unless the review proves the previous mainline objective was incomplete.

## Instructions

1. **Refresh the round contract** at `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-32-contract.md`
2. **Address blocking issues first** and keep the mainline objective stable
3. **Focus on fixes only** - do not add new features or make unrelated changes
4. **Commit your changes** after fixing the issues
5. **Write your summary** to: `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-32-summary.md`

## Summary Template

Your summary should include:
- The mainline objective for this round
- Which blocking issues were fixed
- Which issues were reclassified as queued follow-up
- How each fixed issue was resolved
- Any issues that could not be resolved (with explanation)
- Confirmation that `goal-tracker.md` was updated if the blocking/queued issue lists changed
- A Goal Tracker Update Request only if tracker reconciliation still needs Codex help

## Important Notes

- The COMPLETE signal has no effect during the review phase
- You must address the code review findings to proceed
- After you commit and write your summary, run `/home/zhaosiying/.codex/skills/humanize/scripts/rlcr-stop-gate.sh` to trigger the next code review in-session
- The loop continues until no `[P0-9]` issues are found

## BitLesson Selection (REQUIRED FOR EACH FIX TASK)

Before implementing each fix task, you MUST:

1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each fix task/sub-task to select relevant lesson IDs
3. Follow the selected lesson IDs (or `NONE`) during implementation

Reference: @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/bitlesson.md

## Task Tag Routing Reminder

Follow the plan's per-task routing tags strictly:
- `coding` task -> Claude executes directly
- `analyze` task -> execute via `/humanize:ask-codex`, then integrate the result
- Keep Goal Tracker Active Tasks columns `Tag` and `Owner` aligned with execution
