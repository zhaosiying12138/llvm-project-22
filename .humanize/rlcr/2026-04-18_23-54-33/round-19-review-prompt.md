# FULL GOAL ALIGNMENT CHECK - Round 19

This is a **mandatory checkpoint** (at configurable intervals). You must conduct a comprehensive goal alignment audit.

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.

---
## Claude's Work Summary
<!-- CLAUDE's WORK SUMMARY START -->
# Round 19 Summary

## Work Completed
- Removed the residual AC-3 source metadata called out by Round 18 review: unused floating-point pseudo/constant definitions, the stale GlobalISel CSE hook/include, and the stale vector stack comment.
- Trimmed YSX-owned tests so inactive copied RV32, ILP32/LP64F/LP64D/LP64E, Zb*, XThead/XVT, Zicond, RV64IA-TSO, WMO, NOZACAS, and related unsupported check-prefix blocks are gone.
- Preserved executed negative unsupported-feature tests for `+zbb`, vendor spellings, RV32 arch strings, compressed/vector/FP features, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- Fixed generated-test fallout by simplifying active RUN prefixes and deleting unused FileCheck placeholder blocks.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`, `YSXInstrInfo.h`, `YSXTargetMachine.cpp`, and `YSXFrameLowering.cpp` for the residual source metadata cleanup.
- `llvm/test/CodeGen/YSX/*.ll` and selected `llvm/test/MC/YSX/*.s` files for stale unsupported check-prefix removal and regenerated active rv64ima checks.
- RLCR files for Round 18 review, Round 19 prompt/contract/summary, and the mutable goal tracker/state updates.

## Validation
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` (`132` tests).
- PASS: YSX clang smoke compile produced `ELF 64-bit LSB relocatable, UCB RISC-V, soft-float ABI`; combined RISCV smoke compile also produced an ELF64 RISC-V soft-float object.
- PASS: negative probes rejected `+v`, `+32bit`, `+zca`, vendor `+xventanacondops`, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: source/test scans no longer find the reviewed stale unsupported markers except executed negative unsupported-feature tests.

## Remaining Items
- CPU/tune target-attribute warn-and-ignore policy remains queued because no feature leakage was observed.
- Current backend line counts: RISCV `136,068`; YSX `27,237`; reduction `108,831` lines, about `79.98%`.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `.humanize/bitlesson.md` has no concrete entries; the selector returned the placeholder `NONE` result for this round's tasks.
<!-- CLAUDE's WORK SUMMARY  END  -->
---

## Part 1: Goal Tracker Audit (MANDATORY)

Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md and verify:

### 1.1 Acceptance Criteria Status
For EACH Acceptance Criterion in the IMMUTABLE SECTION:
| AC | Status | Evidence (if MET) | Blocker (if NOT MET) | Justification (if DEFERRED) |
|----|--------|-------------------|---------------------|----------------------------|
| AC-1 | MET / PARTIAL / NOT MET / DEFERRED | ... | ... | ... |
| ... | ... | ... | ... | ... |

### 1.2 Forgotten Items Detection
Compare the original plan (@docs/plan.md) with the current goal-tracker:
- Are there tasks that are neither in "Active", "Completed", nor "Deferred"?
- Are there tasks marked "complete" in summaries but not verified?
- List any forgotten items found.

### 1.3 Deferred Items Audit
For each item in "Explicitly Deferred":
- Is the deferral justification still valid?
- Should it be un-deferred based on current progress?
- Does it contradict the Ultimate Goal?

### 1.4 Goal Completion Summary
```
Acceptance Criteria: X/Y met (Z deferred)
Active Tasks: N remaining
Estimated remaining rounds: ?
Critical blockers: [list if any]
```

## Part 2: Mainline Drift Audit (MANDATORY)

Determine whether the recent rounds are still serving the original plan:
- Is the current round's mainline objective clear and singular?
- Has Claude been advancing mainline ACs, or mostly clearing side issues?
- Which findings are true **blocking side issues** versus merely **queued side issues**?

Include a short drift summary:
```
Mainline Progress Verdict: ADVANCED / STALLED / REGRESSED
Blocking Side Issues: N
Queued Side Issues: N
```

The `Mainline Progress Verdict` line is mandatory. If you omit it, the Humanize stop hook will block the round and require the review to be rerun.

## Part 3: Implementation Review

- Conduct a deep critical review of the implementation
- Verify Claude's claims match reality
- Identify any gaps, bugs, or incomplete work
- Reference @docs for design documents

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

## Part 5: Progress Stagnation Check (MANDATORY for Full Alignment Rounds)

To implement the original plan at @docs/plan.md, we have completed **20 iterations** (Round 0 to Round 19).

The project's `.humanize/rlcr/2026-04-18_23-54-33/` directory contains the history of each round's iteration:
- Round input prompts: `round-N-prompt.md`
- Round output summaries: `round-N-summary.md`
- Round review prompts: `round-N-review-prompt.md`
- Round review results: `round-N-review-result.md`

**How to Access Historical Files**: Read the historical review results and summaries using file paths like:
- `@.humanize/rlcr/2026-04-18_23-54-33/round-18-review-result.md` (previous round)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-17-review-result.md` (2 rounds ago)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-18-summary.md` (previous summary)

**Your Task**: Review the historical review results, especially the **recent rounds** of development progress and review outcomes, to determine if the development has stalled.

**Signs of Stagnation** (circuit breaker triggers):
- Same issues appearing repeatedly across multiple rounds
- No meaningful progress on Acceptance Criteria over several rounds
- Claude making the same mistakes repeatedly
- Circular discussions without resolution
- No new code changes despite continued iterations
- Codex giving similar feedback repeatedly without Claude addressing it

**If development is stagnating**, write **STOP** (as a single word on its own line) as the last line of your review output @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-19-review-result.md instead of COMPLETE.

## Part 6: Output Requirements

- If issues found OR any AC is NOT MET (including deferred ACs), write your findings to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-19-review-result.md
- Include specific action items for Claude to address, classified into:
  - Mainline Gaps
  - Blocking Side Issues
  - Queued Side Issues
- **If development is stagnating** (see Part 4), write "STOP" as the last line
- **CRITICAL**: Only write "COMPLETE" as the last line if ALL ACs from the original plan are FULLY MET with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any AC is deferred
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals allowed
