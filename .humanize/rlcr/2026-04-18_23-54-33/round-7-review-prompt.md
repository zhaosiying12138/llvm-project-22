# Code Review - Round 7

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.
This plan contains the complete requirements and implementation details that Claude should be following.

Based on the original plan and @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-7-prompt.md, Claude claims to have completed the work. Please conduct a thorough critical review to verify this.

---
Below is Claude's summary of the work completed:
<!-- CLAUDE's WORK SUMMARY START -->
# Round 7 Summary

## Work Completed
- Added YSX-owned Clang feature handling in `YSX64TargetInfo`: feature-map
  initialization, target-feature handling, target-attribute parsing, and
  feature-name validation now use a YSX whitelist instead of inherited RISCV
  parsing.
- Kept the accepted YSX frontend feature set to `rv64ima` plus internal
  `64bit`, optional `relax`, and reserved-GPR target features. User target
  attributes reject unsupported features and do not expose internal `+64bit` as
  an arch extension.
- Rejected unsupported FP/vector inline asm constraints for YSX by overriding
  constraint validation/conversion for the copied RISCV target info subclass.
- Made unsupported YSX target attributes diagnose as frontend errors before IR
  emission instead of being ignored as generic target-attribute warnings or
  reaching the backend.
- Expanded YSX Clang driver tests for `target("arch=+v")`,
  `target("arch=rv64imaf")`, `target("arch=+64bit")`, explicit cc1
  `-target-feature +v`, and unsupported `"f"`/`"vr"` asm constraints.
- Updated the mutable goal tracker to record the Round 7 local fix while
  keeping AC-3 backend source pruning active.

## Files Changed
- `clang/lib/Basic/Targets/RISCV.h`
- `clang/lib/Basic/Targets/RISCV.cpp`
- `clang/lib/Sema/SemaDeclAttr.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `.humanize/rlcr/2026-04-18_23-54-33/round-7-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja clang LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: default and explicit `-march=rv64ima` YSX Clang smoke compiles produce
  ELF64 RISC-V soft-float relocatable objects.
- PASS: `target("arch=+v")`, `target("arch=rv64imaf")`, and
  `target("arch=+64bit")` fail before IR emission with
  `invalid feature combination: YSX only supports the rv64ima ISA`.
- PASS: explicit cc1 `-target-feature +v` fails before predefines and does not
  define `__riscv_vector` or `__riscv_v*` macros.
- PASS: YSX rejects `"f"` and `"vr"` inline asm constraints in Clang
  syntax/semantic checks.
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in
  `build_ysx_riscv_host_llvm`.
- PASS: combined build smoke compiles for both `ysx64-unknown-elf` and
  `riscv64-unknown-elf` produce ELF64 RISC-V relocatable objects.
- PASS: RISCV still defines `__riscv_v_intrinsic`; YSX still does not define
  `__riscv_v_intrinsic` or `__riscv_vector`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Line-count status: `llvm/lib/Target/RISCV` is 136,068 lines across 188
  files; `llvm/lib/Target/YuShuXin` is 60,045 lines across 86 files. Round 7
  did not reduce backend lines because it targeted Clang frontend leaks.

## Remaining Items
- AC-3 remains active: YSX backend sources still retain vector/FP/vendor/RV32/
  compressed lowering, selection, pass declarations, TSFlags, frame helpers,
  false-return compatibility APIs, and vector helper forwarding that must be
  deleted in later pruning rounds.
- Stale copied YSX CodeGen check-prefix cleanup remains queued until the backend
  source-pruning pass removes the corresponding implementation surfaces.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for the Round 7
  contract/hardening, test, and validation tasks, so no actionable lesson was
  selected or added.
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
- If after your investigation the actual situation does not match what Claude claims to have completed, or there is pending work to be done, output your review comments to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-7-review-result.md.
- **CRITICAL**: Only output "COMPLETE" as the last line if ALL tasks from the original plan are FULLY completed with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any task is deferred
  - UNFINISHED items are considered INCOMPLETE - do NOT output COMPLETE if any task is pending
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals or pending work allowed
- The word COMPLETE on the last line will stop Claude.
