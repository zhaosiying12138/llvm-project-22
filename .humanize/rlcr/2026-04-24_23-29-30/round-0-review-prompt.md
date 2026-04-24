# Code Review - Round 0

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.
This plan contains the complete requirements and implementation details that Claude should be following.

Based on the original plan and @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-0-prompt.md, Claude claims to have completed the work. Please conduct a thorough critical review to verify this.

---
Below is Claude's summary of the work completed:
<!-- CLAUDE's WORK SUMMARY START -->
# Round 0 Summary

## What Was Implemented

Configured the RISCV-only build, implemented the default-off Yushuxin vector
`llvm.exp` lowering path, added the hidden RVV pressure-aware scheduler option,
added the conservative RVV load reload pass, wired frame diagnostics, and added
the required workload/report artifacts.

## Files Changed

- `llvm/lib/Target/RISCV/`: feature, instruction, pseudo, lowering, scheduler,
  reload pass, frame diagnostics, and pipeline wiring.
- `llvm/include/llvm/Target/TargetSelectionDAG.td`: SelectionDAG `fexp` pattern
  node exposure for TableGen matching.
- `llvm/test/CodeGen/RISCV/rvv/`: Yushuxin lowering test and pressure workload
  regression file.
- `llvm/test/MC/RISCV/rvv/`: Yushuxin asm/disasm feature-gating test.
- `llvm/test/CodeGen/RISCV/features-info.ll`: feature help coverage.
- `docs/report.md`: build commands, measured spill/reload counts, diagnostics,
  and before/after snippets.

## Validation

- `cmake -S llvm -B build-riscv -G Ninja -DLLVM_TARGETS_TO_BUILD=RISCV -DLLVM_ENABLE_PROJECTS='clang;lld' -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_CCACHE_BUILD=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_USE_LINKER=lld`
- `ninja -C build-riscv llc clang lld FileCheck llvm-mc llvm-objdump count not llvm-config llvm-readobj`
- `build-riscv/bin/llvm-lit -sv llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll llvm/test/MC/RISCV/rvv/yushuxin-vfexp.s llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll llvm/test/CodeGen/RISCV/features-info.ll` passed 4/4.
- `build-riscv/bin/llc -mtriple=riscv64 -mattr=help 2>&1 | rg -n "experimental-yushuxin|Yushuxin"` showed the feature help entry.

## Remaining Items

No implementation items are intentionally deferred. Strict `fexp`, GlobalISel,
dynamic-loop cases, and unsafe alias speculation remain out of scope per plan.

## BitLesson Delta

Action: none
Lesson ID(s): NONE
Notes: No reusable lesson was added; the selector produced placeholder output
and `.humanize/bitlesson.md` had no applicable entries.
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

Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md and verify:

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
2. **If correction is needed**: Update @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/goal-tracker.md yourself with the requested changes:
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
- If after your investigation the actual situation does not match what Claude claims to have completed, or there is pending work to be done, output your review comments to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/round-0-review-result.md.
- **CRITICAL**: Only output "COMPLETE" as the last line if ALL tasks from the original plan are FULLY completed with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any task is deferred
  - UNFINISHED items are considered INCOMPLETE - do NOT output COMPLETE if any task is pending
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals or pending work allowed
- The word COMPLETE on the last line will stop Claude.
