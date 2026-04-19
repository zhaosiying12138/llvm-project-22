# Code Review - Round 5

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.
This plan contains the complete requirements and implementation details that Claude should be following.

Based on the original plan and @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-5-prompt.md, Claude claims to have completed the work. Please conduct a thorough critical review to verify this.

---
Below is Claude's summary of the work completed:
<!-- CLAUDE's WORK SUMMARY START -->
# Round 5 Summary

## Work Completed
- Fixed the supported YSX Clang compile path by filtering inherited disabled RISCV target features before YSX subtarget parsing.
- Preserved strict rejection for enabled unsupported features while allowing harmless disabled inherited features such as `-f`, `-v`, and `-zbb`.
- Kept `llvm-mc -mattr=help` on the curated YSX help surface so help output does not expose copied 32-bit/vector tuning feature names.
- Added real Clang compile coverage for default `ysx64-unknown-elf` and explicit `-march=rv64ima`.
- Added MC coverage that disabled unsupported inherited features do not reject valid rv64ima assembly.
- Updated the mutable goal tracker to mark the Clang compile-path blocker resolved and keep AC-3 source pruning active.

## Files Changed
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `llvm/test/MC/YSX/unsupported-features.s`
- `.humanize/rlcr/2026-04-18_23-54-33/round-5-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja LLVMYSXCodeGen clang llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `clang --target=ysx64-unknown-elf -c` produces a 64-bit RISC-V ELF relocatable object.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64ima -c` produces a 64-bit RISC-V ELF relocatable object.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64imaf/-march=rv64imac -c` rejects with `YuShuXin only supports -march=rv64ima`.
- PASS: `llvm-mc -triple=ysx64 -mattr=-f,-v,-zbb` assembles valid `add` input.
- PASS: `llvm-mc -triple=ysx64 -mattr=help` lists only the curated YSX CPU/features: rv64, I/M/A, Zmmul, Zaamo, Zalrsc, relax, and exact-asm.
- PASS: `llvm-mc -triple=ysx64 -mattr=+f` and `llc -mtriple=ysx64-unknown-elf -mattr=+v` still reject with `YSX only supports the rv64ima ISA`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `build_ysx_riscv_host_llvm`.
- PASS: Combined build `clang --target=ysx64-unknown-elf` and explicit `-march=rv64ima` smoke compiles both produce ELF objects.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Remaining Items
- AC-3 remains active: `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, `YSXSubtarget.h`, `YSX.h`, frame helpers, and TargetParser/Clang parser aliases still carry copied unsupported source surface.
- AC-4 is improved for the real Clang compile path, but future source-pruning rounds should keep rerunning the same Clang compile tests.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for implementation and validation tasks, so no actionable lesson was selected or added.
<!-- CLAUDE's WORK SUMMARY  END  -->
---

## Part 1: Implementation Review

- Your task is to conduct a deep critical review, focusing on finding implementation issues and identifying gaps between "plan-design" and actual implementation.
- Relevant top-level guidance documents, phased implementation plans, and other important documentation and implementation references are located under @docs.
- If Claude planned to defer any tasks to future phases in its summary, DO NOT follow its lead. Instead, you should force Claude to complete ALL tasks as planned.
  - Such deferred tasks are considered incomplete work and should be flagged in your review comments, requiring Claude to address them.
  - If Claude planned to defer any tasks, please explore the codebase in-depth and draft a detailed implementation plan. This plan should be included in your review comments for Claude to follow.
  - Your review should be meticulous and skeptical. Look for any discrepancies, missing features, incomplete implementations.
- If Claude does not plan to defer any tasks, but honestly admits that some tasks are still pending (not yet completed), you should also include those pending tasks in your review.
  - Your review should elaborate on those unfinished tasks, explore the codebase, and draft an implementation plan.
  - A good engineering implementation plan should be **singular, directive, and definitive**, rather than discussing multiple possible implementation options.
  - The implementation plan should be **unambiguous**, internally consistent, and coherent from beginning to end, so that **Claude can execute the work accurately and without error**.

## Part 2: Goal Alignment Check (MANDATORY)

Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md and verify:

1. **Acceptance Criteria Progress**: For each AC, is progress being made? Are any ACs being ignored?
2. **Forgotten Items**: Are there tasks from the original plan that are not tracked in Active/Completed/Deferred?
3. **Deferred Items**: Are deferrals justified? Do they block any ACs?
4. **Plan Evolution**: If Claude modified the plan, is the justification valid?

Include a brief Goal Alignment Summary in your review:
```
ACs: X/Y addressed | Forgotten items: N | Unjustified deferrals: N
```

## Part 3: Required Finding Classification

You MUST classify your findings into these lanes:
- **Mainline Gaps**: plan-derived work or AC progress that is missing, incomplete, or regressing
- **Blocking Side Issues**: bugs or implementation issues that block the current mainline objective from succeeding safely
- **Queued Side Issues**: valid non-blocking follow-up issues that should be documented but must NOT take over the next round

Also include a one-line verdict:
```
Mainline Progress Verdict: ADVANCED / STALLED / REGRESSED
```

This verdict line is mandatory. If you omit it, the Humanize stop hook will block the round and require the review to be rerun.

If Claude mostly worked on queued side issues and failed to advance the mainline, say so explicitly.

## Part 4: ## Goal Tracker Update Requests (YOUR RESPONSIBILITY)

Claude should normally keep the **mutable section** of `goal-tracker.md` up to date directly. If Claude's summary contains a "Goal Tracker Update Request" section, or if you detect tracker drift during review, YOU must:

1. **Evaluate the tracker state**: Is the mutable section still aligned with the Ultimate Goal and current AC progress?
2. **If correction is needed**: Update @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md yourself with the requested changes:
   - Move tasks between Active/Completed/Deferred sections as appropriate
   - Add entries to "Plan Evolution Log" with round number and justification
   - Add new issues to "Blocking Side Issues" or "Queued Side Issues" as appropriate
   - **NEVER modify the IMMUTABLE SECTION** (Ultimate Goal and Acceptance Criteria)
3. **If you reject a requested tracker change**: Include in your review why it was rejected

Common update requests you should handle:
- Task completion: Move from "Active Tasks" to "Completed and Verified"
- New blocking issues: Add to "Blocking Side Issues"
- New queued issues: Add to "Queued Side Issues"
- Plan changes: Add to "Plan Evolution Log" with your assessment
- Deferrals: Only allow with strong justification; add to "Explicitly Deferred"

## Part 5: Output Requirements

- In short, your review comments can include: problems/findings/blockers; claims that don't match reality; implementation plans for deferred work (to be implemented now); implementation plans for unfinished work; goal alignment issues.
- Your output should be structured so Claude can tell which items are mainline gaps, blocking side issues, and queued side issues.
- If after your investigation the actual situation does not match what Claude claims to have completed, or there is pending work to be done, output your review comments to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-5-review-result.md.
- **CRITICAL**: Only output "COMPLETE" as the last line if ALL tasks from the original plan are FULLY completed with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any task is deferred
  - UNFINISHED items are considered INCOMPLETE - do NOT output COMPLETE if any task is pending
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals or pending work allowed
- The word COMPLETE on the last line will stop Claude.
