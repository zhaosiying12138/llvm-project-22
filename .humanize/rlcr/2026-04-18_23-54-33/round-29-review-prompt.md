# FULL GOAL ALIGNMENT CHECK - Round 29

This is a **mandatory checkpoint** (at configurable intervals). You must conduct a comprehensive goal alignment audit.

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.

---
## Claude's Work Summary
<!-- CLAUDE's WORK SUMMARY START -->
# Round 29 Summary

## Work Completed
- Restricted YSX Clang GCC register names and aliases to retained rv64ima GPRs.
- Added YSX Clang negative coverage for removed FP/vector/CSR clobber names and a positive retained `x9`/`s1` clobber probe.
- Removed active copied `~{f8},~{f9}` and `~{vtype},~{vl},~{vxsat},~{vxrm}` clobbers from YSX CodeGen tests.
- Added a YSX backend IR guard and negative CodeGen test for removed inline-asm clobbers that bypass Clang.
- Backend size is now 26,043 lines versus RISCV's 136,068 lines; focused YSX tests are now 48,111 lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-29-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-29-summary.md`
- `clang/lib/Basic/Targets/RISCV.h`
- `clang/lib/Basic/Targets/RISCV.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `llvm/lib/Target/YuShuXin/YSXCodeGenPrepare.cpp`
- `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll`
- `llvm/test/CodeGen/YSX/inline-asm-mem-constraint.ll`
- `llvm/test/CodeGen/YSX/unsupported-inline-asm-clobbers.ll`

## Validation
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: targeted lit for `clang/test/Driver/YSX/target-options.c`, `inline-asm-clobbers.ll`, `inline-asm-mem-constraint.ll`, and `unsupported-inline-asm-clobbers.ll`, 4 tests.
- PASS: `llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`, 133 tests.
- PASS: direct Clang clobber probes reject `f8`, `fs0`, `v0`, `vtype`, `vl`, `vxsat`, and `vxrm`, while accepting `x9` and `s1`.
- PASS: smoke/negative probes for YSX and RISCV object generation, retained `.insn`/`.reloc`, rejected removed `.reloc`, unsupported `-mattr`, `.option arch rv64ima_zbb`, numeric CSR, removed `.insn` forms, `rv64imaf`, `ysx32`, removed clobbers, scalable-vector IR, direct `llvm.riscv.vsetvli`, and backend IR removed-clobber rejection.
- PASS: `rg -n "MIPS|CCMov|ccmov|mips\.ccmov|RV64I-CCMOV|RV64-MIPS" ...` has no YSX source or test matches.
- PASS: Round-25 stale-prefix scan has no YSX test matches.
- PASS: removed-clobber scan only reports intentional negative coverage in `unsupported-inline-asm-clobbers.ll`.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV clang/test/Driver/RISCV clang/test/CodeGen/RISCV | wc -l` reports `0`.
- INFO: `code-simplifier` plugin lookup found no local plugin path under the repo or `/home/zhaosiying/.codex`.

## Remaining Items
- No known Round-29 implementation blockers. Completion remains pending Codex stop-gate review.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: BitLesson selector returned only the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output, so no lesson was applied or updated.
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

To implement the original plan at @docs/plan.md, we have completed **30 iterations** (Round 0 to Round 29).

The project's `.humanize/rlcr/2026-04-18_23-54-33/` directory contains the history of each round's iteration:
- Round input prompts: `round-N-prompt.md`
- Round output summaries: `round-N-summary.md`
- Round review prompts: `round-N-review-prompt.md`
- Round review results: `round-N-review-result.md`

**How to Access Historical Files**: Read the historical review results and summaries using file paths like:
- `@.humanize/rlcr/2026-04-18_23-54-33/round-28-review-result.md` (previous round)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-27-review-result.md` (2 rounds ago)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-28-summary.md` (previous summary)

**Your Task**: Review the historical review results, especially the **recent rounds** of development progress and review outcomes, to determine if the development has stalled.

**Signs of Stagnation** (circuit breaker triggers):
- Same issues appearing repeatedly across multiple rounds
- No meaningful progress on Acceptance Criteria over several rounds
- Claude making the same mistakes repeatedly
- Circular discussions without resolution
- No new code changes despite continued iterations
- Codex giving similar feedback repeatedly without Claude addressing it

**If development is stagnating**, write **STOP** (as a single word on its own line) as the last line of your review output @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-29-review-result.md instead of COMPLETE.

## Part 6: Output Requirements

- If issues found OR any AC is NOT MET (including deferred ACs), write your findings to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-29-review-result.md
- Include specific action items for Claude to address, classified into:
  - Mainline Gaps
  - Blocking Side Issues
  - Queued Side Issues
- **If development is stagnating** (see Part 4), write "STOP" as the last line
- **CRITICAL**: Only write "COMPLETE" as the last line if ALL ACs from the original plan are FULLY MET with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any AC is deferred
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals allowed
