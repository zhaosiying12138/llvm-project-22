# FULL GOAL ALIGNMENT CHECK - Round 9

This is a **mandatory checkpoint** (at configurable intervals). You must conduct a comprehensive goal alignment audit.

## Original Implementation Plan

**IMPORTANT**: The original plan that Claude is implementing is located at:
@docs/plan.md

You MUST read this plan file first to understand the full scope of work before conducting your review.

---
## Claude's Work Summary
<!-- CLAUDE's WORK SUMMARY START -->
# Round 9 Summary

## Work Completed

- Wrote the Round 9 contract with one mainline objective: remove the
  `YSXVType` forwarding layer and the MC-visible copied tune/profile feature
  surface.
- Removed `namespace YSXVType` from `YSXISAInfo.h` and eliminated all
  `YSXVType` references under YSX.
- Disabled YSX MC VTYPE text parsing and vector VTYPE pretty-printing paths.
- Removed copied vector/vendor/profile tune feature definitions for
  `log-vrgather`, `enable-vsetvli-sched-heuristic`,
  `optimized-zero-stride-load`, `optimized-nf*-segment-load-store`,
  `vl-dependent-latency`, `dlen-factor-2`, `no-sink-splat-operands`,
  `conditional-cmv-fusion`, `single-element-vec-fp64`,
  `vxrm-pipeline-flush`, `prefer-vsetvli-over-read-vlenb`, and vendor profile
  selectors.
- Tightened both YSX feature filters so enabled MC/subtarget `-mattr` features
  outside `64bit`, `i`, `m`, `a`, `zmmul`, `zaamo`, `zalrsc`, `relax`, and
  `exact-asm` report `YSX only supports the rv64ima ISA`.
- Added negative MC coverage for `+vxrm-pipeline-flush`, `+log-vrgather`,
  `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and
  `+andes45`.

## Files Changed

- `.humanize/rlcr/2026-04-18_23-54-33/round-9-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `llvm/include/llvm/TargetParser/YSXISAInfo.h`
- `llvm/lib/Target/YuShuXin/YSXFeatures.td`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.{h,cpp}`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp`
- `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXInstPrinter.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.{h,cpp}`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `llvm/lib/Target/YuShuXin/YSXExpandPseudoInsts.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrPredicates.td`
- `llvm/lib/Target/YuShuXin/{MCTargetDesc/YSXBaseInfo.h,YSXRegisterInfo.h}`
- `llvm/test/MC/YSX/unsupported-features.s`

## Validation

- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX/unsupported-features.s`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` discovered 130 tests.
- PASS: YSX-only clang smoke compile for `--target=ysx64-unknown-elf` produced an ELF64 RISC-V soft-float relocatable.
- PASS: YSX-only `-###` emits only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax`.
- PASS: Manual YSX-only and combined `llvm-mc` probes reject `+vxrm-pipeline-flush`, `+log-vrgather`, `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and `+andes45`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: Combined YSX and RISCV clang smoke compiles both produced ELF64 RISC-V soft-float relocatables.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Line Counts

- Original RISCV backend: 136,068 lines across 188 files.
- Current YSX backend: 59,449 lines across 86 files.
- Current reduction from original RISCV: 76,619 lines, about 56.3%.
- Round 9 YSX backend reduction: 551 lines.

## Remaining Items

- Direct copied vector helper use remains through `RISCVVType` in lowering,
  DAG selection, and register metadata. This is not a YSX forwarding shim, but
  it still represents retained copied vector source that must be deleted in
  later AC-3 pruning.
- Large copied vector/FP/RV32/compressed bodies remain in `YSXISelLowering`,
  `YSXISelDAGToDAG`, `YSXInstrInfo`, TableGen metadata, frame lowering, and
  disassembler helpers.
- The current 59,449-line backend likely needs several more large deletion
  rounds to approach the expected 30,000-line target.

## BitLesson Delta

- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for each Round 9
  task, so it was treated as `NONE`.
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

To implement the original plan at @docs/plan.md, we have completed **10 iterations** (Round 0 to Round 9).

The project's `.humanize/rlcr/2026-04-18_23-54-33/` directory contains the history of each round's iteration:
- Round input prompts: `round-N-prompt.md`
- Round output summaries: `round-N-summary.md`
- Round review prompts: `round-N-review-prompt.md`
- Round review results: `round-N-review-result.md`

**How to Access Historical Files**: Read the historical review results and summaries using file paths like:
- `@.humanize/rlcr/2026-04-18_23-54-33/round-8-review-result.md` (previous round)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-7-review-result.md` (2 rounds ago)
- `@.humanize/rlcr/2026-04-18_23-54-33/round-8-summary.md` (previous summary)

**Your Task**: Review the historical review results, especially the **recent rounds** of development progress and review outcomes, to determine if the development has stalled.

**Signs of Stagnation** (circuit breaker triggers):
- Same issues appearing repeatedly across multiple rounds
- No meaningful progress on Acceptance Criteria over several rounds
- Claude making the same mistakes repeatedly
- Circular discussions without resolution
- No new code changes despite continued iterations
- Codex giving similar feedback repeatedly without Claude addressing it

**If development is stagnating**, write **STOP** (as a single word on its own line) as the last line of your review output @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-9-review-result.md instead of COMPLETE.

## Part 6: Output Requirements

- If issues found OR any AC is NOT MET (including deferred ACs), write your findings to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-9-review-result.md
- Include specific action items for Claude to address, classified into:
  - Mainline Gaps
  - Blocking Side Issues
  - Queued Side Issues
- **If development is stagnating** (see Part 4), write "STOP" as the last line
- **CRITICAL**: Only write "COMPLETE" as the last line if ALL ACs from the original plan are FULLY MET with no deferrals
  - DEFERRED items are considered INCOMPLETE - do NOT output COMPLETE if any AC is deferred
  - The ONLY condition for COMPLETE is: all original plan tasks are done, all ACs are met, no deferrals allowed
