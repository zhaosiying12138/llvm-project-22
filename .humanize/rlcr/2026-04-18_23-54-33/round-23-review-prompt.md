# Code Review - Round 23

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.
This plan contains the complete requirements and implementation details that Claude should be following.

Based on the original plan and @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-23-prompt.md, Claude claims to have completed the work. Please conduct a thorough critical review to verify this.

---
Below is Claude's summary of the work completed:
<!-- CLAUDE's WORK SUMMARY START -->
# Round 23 Summary

## Work Completed
- Deleted the reviewed short-forward-branch and CCMOV tuning residue from YuShuXin.
- Removed dead FP/vector scheduler predicates and scheduler reads with no rv64ima role.
- Removed vector custom-ISD TSFlag helpers and the unused vector multiply-add helper from `YSXSelectionDAGInfo.h`.
- Removed the SFB-only divide-by-power-of-two lowering override and made select lowering use the retained non-CCMOV path directly.
- Removed the unused `YSXGenExegesis.inc` tablegen target after assertions-enabled `llvm-tblgen` exposed that YSX had no exegesis counter bindings and no YSX source consumed the generated file.

## Files Changed
- `llvm/lib/Target/YuShuXin/CMakeLists.txt`
- `llvm/lib/Target/YuShuXin/YSXFeatures.td`
- `llvm/lib/Target/YuShuXin/YSXSchedule.td`
- `llvm/lib/Target/YuShuXin/YSXInstrPredicates.td`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.h`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-23-contract.md`

## Validation
- `rg "ShortForwardBranch|short-forward-branch|SFB|PseudoCCMOV|PseudoCCSUB|VLDSX0Pred|SingleElementVecFP64SchedPred|ReadFMemBase|ReadFStoreData|HasPassthruOp|HasMaskOp|getMAccOpcode|vector multiply-add" llvm/lib/Target/YuShuXin` produced no matches.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed after CMake regeneration.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed after CMake regeneration.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- Smoke and negative probes passed for YSX-only and combined RISCV+YSX tools: YSX and RISCV smoke compiles produced ELF64 RISC-V soft-float objects, retained `.insn r OP` assembled, and unsupported feature, CSR, Clang, scalable-vector IR, direct `llvm.riscv.vsetvli`, removed `.insn` opcode, and `.insn r4` probes failed as expected.
- Configured `/home/zhaosiying/codebase/compiler/build_ysx_assert_host_llvm` with Ninja, ccache, Clang, `LLVM_ENABLE_ASSERTIONS=ON`, projects `clang;lld`, and experimental target `YSX`; `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_assert_host_llvm LLVMYSXCodeGen llc` passed.
- The first assertions-enabled build attempt failed before the CMake cleanup in `YSXGenExegesis.inc` generation because `llvm-tblgen -gen-exegesis` asserted on missing PFM counter bindings; removing the unused YSX exegesis generation fixed the assertions-enabled build.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `25,987`; reduction `110,081` lines, or `80.90%`; YuShuXin tests `50,754` lines.

## Remaining Items
- No known Round 23 implementation blocker remains before Codex review.
- Final completion still depends on the RLCR stop gate result.
- The `code-simplifier` plugin was not found under the repo or `/home/zhaosiying/.codex`, so no plugin optimization pass was available to run.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector produced the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, so no applicable lesson delta was recorded for this round.
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
- If after your investigation the actual situation does not match what Claude claims to have completed, or there is pending work to be done, output your review comments to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-23-review-result.md.
- **CRITICAL**: Only output "COMPLETE" as the last line if ALL tasks from the original plan are FULLY completed with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any task is deferred
  - UNFINISHED items are considered INCOMPLETE - do NOT output COMPLETE if any task is pending
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals or pending work allowed
- The word COMPLETE on the last line will stop Claude.
