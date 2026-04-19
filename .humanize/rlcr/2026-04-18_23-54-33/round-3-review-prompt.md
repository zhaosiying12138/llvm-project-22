# Code Review - Round 3

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.
This plan contains the complete requirements and implementation details that Claude should be following.

Based on the original plan and @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-3-prompt.md, Claude claims to have completed the work. Please conduct a thorough critical review to verify this.

---
Below is Claude's summary of the work completed:
<!-- CLAUDE's WORK SUMMARY START -->
# Round 3 Summary

## Work Completed
- Removed default MC exposure of unsupported FP/vector CSR aliases: `fflags`, `frm`, `fcsr`, `vstart`, `vxsat`, `vxrm`, `vcsr`, `vl`, `vtype`, and `vlenb` are no longer declared as YSX system-register operands.
- Added negative MC coverage for those CSR names and for raw 16-bit `.insn` emission, which now rejects compressed instruction length for YSX.
- Deleted the copied compressed/vector instruction-format TD includes from `YSXInstrInfo.td` and removed `YSXInstrFormatsC.td` / `YSXInstrFormatsV.td`.
- Pruned `YSXRegisterInfo.td` to remove FP register files, vector registers, vector CSR pseudo-registers, and FP/vector register classes.
- Simplified calling-convention TD/C++ to GPR-only LP64/ILP32 handling and removed FP/vector/RVE callee-saved register variants.
- Replaced the inherited FRM/FFLAGS CSR insertion pass with a no-op pass and changed FP environment lowering to avoid deleted custom CSR ISD nodes.
- Removed or neutralized selected FP/vector references in MC code emitter, disassembler, asm parser, register info, instruction info, and lowering so the pruned register tables compile.
- Preserved `llvm/lib/Target/RISCV` unchanged.

## Files Changed
- YSX backend sources under `llvm/lib/Target/YuShuXin`, including AsmParser, Disassembler, MCTargetDesc, CallingConv, FrameLowering, ISelLowering, InsertReadWriteCSR, InstrInfo, RegisterInfo, and system operands.
- Deleted YSX TD files: `llvm/lib/Target/YuShuXin/YSXInstrFormatsC.td` and `llvm/lib/Target/YuShuXin/YSXInstrFormatsV.td`.
- Updated negative coverage in `llvm/test/MC/YSX/unsupported-features.s`.
- Updated RLCR tracking files in `.humanize/rlcr/2026-04-18_23-54-33/`.

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc` in `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX/unsupported-features.s` from the YSX-only build directory.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` from the YSX-only build directory, with 130 discovered tests.
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## Remaining Items
- AC-3 is not complete. `YSXFeatures.td`, `YSXFrameLowering.*`, `YSXInstrInfo.cpp`, `YSXSubtarget.*`, `YSXISelDAGToDAG.*`, and MC helper files still contain copied feature/vector/FP naming or dead support surfaces that need another pruning slice.
- Some unsupported FP/vector logic is still present behind disabled or unreachable paths. This round focused on removing externally visible CSR leakage, TD register classes, C/V format includes, and enough C++ references to keep the backend buildable.
- The YSX disassembler still reports unused decoder-helper warnings inherited from removed unsupported surfaces.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The BitLesson selector returned the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output again, so it was treated as no usable lesson selection and no lesson file changes were made.
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
- If after your investigation the actual situation does not match what Claude claims to have completed, or there is pending work to be done, output your review comments to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-3-review-result.md.
- **CRITICAL**: Only output "COMPLETE" as the last line if ALL tasks from the original plan are FULLY completed with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any task is deferred
  - UNFINISHED items are considered INCOMPLETE - do NOT output COMPLETE if any task is pending
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals or pending work allowed
- The word COMPLETE on the last line will stop Claude.
