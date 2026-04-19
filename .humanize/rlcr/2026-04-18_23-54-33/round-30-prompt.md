# Code Review Findings

You are in the **Review Phase**. Codex has performed a code review and found issues that need to be addressed.

## Required Re-anchor

Before touching code:
- Re-read the original plan at @docs/plan.md
- Re-read the goal tracker at @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md
- Refresh the current round contract at @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-30-contract.md

The round contract must preserve a single mainline objective. Code review findings do NOT automatically become the new round objective.

## Review Results

## Codex Review Issues

- [P2] Preserve reserve-x features after validation — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:47-49
  When compiling YSX with `-ffixed-xN` or passing `+reserve-xN`, the driver/frontend deliberately emit `+reserve-xN` and `YSXSubtarget` has `UserReservedRegister` bits to consume them, but this whitelist drops/fatal-errors every enabled feature that is not required/relax/exact-asm. As a result `--target=ysx64 ... -ffixed-x5` fails with “YSX only supports the rv64ima ISA” instead of reserving x5; include `reserve-x*` in the retained feature set and mirror it in the MC copy.

- [P2] Keep LastArchType at the final arch enumerator — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:114-114
  With `ysx64` inserted before `sparc`, setting `LastArchType` to `ysx64` makes loops over `FirstArchType..LastArchType` skip every existing architecture from `sparc` through `ve`. Any code enumerating all architectures, including the in-tree triple tests and downstream users, no longer sees those targets; leave `LastArchType` at the last enum value or append `ysx64` at the end.
The patch introduces a new target but breaks architecture enumeration metadata and rejects the reserved-register features that the driver itself emits for YSX. These are functional issues that should be fixed before considering the patch correct.

Full review comments:

- [P2] Preserve reserve-x features after validation — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:47-49
  When compiling YSX with `-ffixed-xN` or passing `+reserve-xN`, the driver/frontend deliberately emit `+reserve-xN` and `YSXSubtarget` has `UserReservedRegister` bits to consume them, but this whitelist drops/fatal-errors every enabled feature that is not required/relax/exact-asm. As a result `--target=ysx64 ... -ffixed-x5` fails with “YSX only supports the rv64ima ISA” instead of reserving x5; include `reserve-x*` in the retained feature set and mirror it in the MC copy.

- [P2] Keep LastArchType at the final arch enumerator — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:114-114
  With `ysx64` inserted before `sparc`, setting `LastArchType` to `ysx64` makes loops over `FirstArchType..LastArchType` skip every existing architecture from `sparc` through `ve`. Any code enumerating all architectures, including the in-tree triple tests and downstream users, no longer sees those targets; leave `LastArchType` at the last enum value or append `ysx64` at the end.

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

1. **Refresh the round contract** at `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-30-contract.md`
2. **Address blocking issues first** and keep the mainline objective stable
3. **Focus on fixes only** - do not add new features or make unrelated changes
4. **Commit your changes** after fixing the issues
5. **Write your summary** to: `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-30-summary.md`

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
