# FULL GOAL ALIGNMENT CHECK - Round 4

This is a **mandatory checkpoint** (at configurable intervals). You must conduct a comprehensive goal alignment audit.

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.

---
## Claude's Work Summary
<!-- CLAUDE's WORK SUMMARY START -->
# Round 4 Summary

## Work Completed
- Removed the remaining default MC-visible non-rv64ima instruction surface identified by Round 3 review:
  `fence.i`, CSR instructions/aliases, counter aliases, privileged/debug/hypervisor mnemonics, and their SelectionDAG patterns.
- Replaced YSX symbolic CSR generation with an empty hand-written lookup path. `YSXSystemOperands.td` now carries no `SysReg` records.
- Replaced `YSXFeatures.td` with a 293-line rv64ima-only feature surface: I, M/Zmmul, A/Zaamo/Zalrsc, 64bit, relax/exact-asm, register reservation, and required LLVM tuning/internal controls.
- Removed direct C++ feature-bit dependencies for unsupported E/C/F/D/Zfinx/V/Ztso/Zalasr/vendor surfaces and fixed unsupported getters to return YSX v1 constants.
- Disabled unsupported interrupt/counter lowering paths so they no longer require CSR or privileged opcodes.
- Updated YSX MC tests for the rv64ima surface, including negative coverage for all Round 3 review examples.

## Files Changed
- Core YSX backend/MC files under `llvm/lib/Target/YuShuXin`, especially `YSXInstrInfo.td`, `YSXFeatures.td`, `YSXSystemOperands.td`, `YSXISelLowering.cpp`, `YSXFrameLowering.cpp`, `YSXSubtarget.h`, and MC parser/printer/streamer helpers.
- YSX MC tests: `unsupported-features.s`, `align.s`, `debug-valid.s`, and `rv64i-aliases-invalid.s`.
- Round metadata: `round-4-contract.md`, this summary, and stop-gate generated tracker/review files.

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `build_ysx_riscv_host_llvm`.
- PASS: manual MC rejection check for `fence.i`, `csrr ...`, `rdcycle/rdtime/rdinstret`, `mret/sret/wfi/dret`, `sfence.vma`, and `hfence.vvma`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: source scan found no backend matches for the removed CSR/privileged opcode defs, `YSXDisabled`, or removed feature records such as Zicsr/Zifencei/Zve/Zvl/F/D/Zca/Zbb.
- Line counts: RISCV backend remains 136,068 lines / 188 files; YSX backend is now 59,953 lines / 86 files. This round removed a net 2,643 lines from YSX/test diff and reduced YSX by about 2,653 backend lines from the previous 62,606-line checkpoint.

## Remaining Items
- AC-2 is materially addressed for the reviewed MC leaks.
- AC-3 is advanced but not complete: large copied `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrFormats.td`, frame/vector metadata, and false-return compatibility APIs still need deletion in later rounds.
- Some unsupported code remains unreachable through fixed false helpers; this is intentionally left for the next pruning round rather than blocking this round's MC/feature-surface objective.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output rather than actionable lesson IDs; no lesson change was made.
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

To implement the original plan at @docs/plan.md, we have completed **5 iterations** (Round 0 to Round 4).

The project's `.humanize/rlcr/2026-04-18_23-54-33/` directory contains the history of each round's iteration:
- Round input prompts: `round-N-prompt.md`
- Round output summaries: `round-N-summary.md`
- Round review prompts: `round-N-review-prompt.md`
- Round review results: `round-N-review-result.md`

**How to Access Historical Files**: Read the historical review results and summaries using file paths like:
- `@.humanize/rlcr/2026-04-18_23-54-33/round-3-review-result.md` (previous round)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-2-review-result.md` (2 rounds ago)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-3-summary.md` (previous summary)

**Your Task**: Review the historical review results, especially the **recent rounds** of development progress and review outcomes, to determine if the development has stalled.

**Signs of Stagnation** (circuit breaker triggers):
- Same issues appearing repeatedly across multiple rounds
- No meaningful progress on Acceptance Criteria over several rounds
- Claude making the same mistakes repeatedly
- Circular discussions without resolution
- No new code changes despite continued iterations
- Codex giving similar feedback repeatedly without Claude addressing it

**If development is stagnating**, write **STOP** (as a single word on its own line) as the last line of your review output @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-4-review-result.md instead of COMPLETE.

## Part 6: Output Requirements

- If issues found OR any AC is NOT MET (including deferred ACs), write your findings to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-4-review-result.md
- Include specific action items for Claude to address, classified into:
  - Mainline Gaps
  - Blocking Side Issues
  - Queued Side Issues
- **If development is stagnating** (see Part 4), write "STOP" as the last line
- **CRITICAL**: Only write "COMPLETE" as the last line if ALL ACs from the original plan are FULLY MET with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any AC is deferred
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals allowed
