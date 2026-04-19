Your work is not finished. Read and execute the below with ultrathink.

## Original Implementation Plan

**IMPORTANT**: Before proceeding, review the original plan you are implementing:
@docs/plan.md

This plan contains the full scope of work and requirements. Ensure your work aligns with this plan.

---

## Round Re-anchor (REQUIRED FIRST STEP)

Before writing code:
- Re-read @docs/plan.md
- Re-read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md
- Re-read the most recent round summaries/reviews that led to this round
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-19-contract.md

Your round contract must contain:
- Exactly one **mainline objective**
- The 1-2 target ACs for this round
- Which issues are truly **blocking** that mainline objective
- Which issues are **queued** and explicitly out of scope
- Concrete success criteria for this round

Do not start implementation until the round contract exists.

## Task Lane Rules

Use the Task system (TaskCreate, TaskUpdate, TaskList) with one required tag per task:
- `[mainline]` for plan-derived work that directly advances this round's objective
- `[blocking]` for issues that prevent the mainline objective from succeeding safely
- `[queued]` for non-blocking bugs, cleanup, or follow-up work

Rules:
- `[mainline]` work is the round's primary success condition
- `[blocking]` work is allowed only when it truly blocks the mainline objective
- `[queued]` work must be documented but must NOT replace the round objective
- If a new bug does not block the current objective, tag it `[queued]` and keep moving on mainline work

Before executing each task in this round:
1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each task/sub-task
3. Follow selected lesson IDs (or `NONE`) during implementation

---
Below is Codex's review result:
<!-- CODEX's REVIEW RESULT START -->
# Round 18 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 18 substantially advanced the mainline AC-3 pruning slice. The reviewed
RV32/ABI/compressed/vendor source markers are largely gone, and
`git diff -- llvm/lib/Target/RISCV | wc -l` still returns `0`. However, the full
`docs/plan.md` goal is not complete. Claude explicitly deferred stale YSX test
cleanup, and that cleanup is plan-derived AC-4 work. I also found small but
concrete residual FP/GISel/source-metadata remnants in the YSX source tree.

## Mainline Gaps

1. AC-4 is still incomplete, and the Round 18 summary's deferral is not
   acceptable. YSX-owned tests still carry large inactive copied check blocks for
   unsupported targets and extensions, even though the plan requires YSX tests to
   be derived from the `rv64ima`-applicable subset. Examples:
   `llvm/test/CodeGen/YSX/shl-demanded.ll:6` keeps `RV32I` checks;
   `llvm/test/CodeGen/YSX/atomic-rmw.ll:13` keeps RV32 atomic checks;
   `llvm/test/CodeGen/YSX/atomic-rmw.ll:40389` keeps `RV64IA-TSO`;
   `llvm/test/CodeGen/YSX/atomic-rmw.ll:40582` keeps unused WMO/NOZACAS
   placeholders; `llvm/test/CodeGen/YSX/ctlz-cttz-ctpop.ll:53` and
   `llvm/test/CodeGen/YSX/ctlz-cttz-ctpop.ll:63` keep Zbb and XThead check
   blocks; `llvm/test/MC/YSX/elf-header.s:4` keeps an RV32 ELF header block; and
   `llvm/test/MC/YSX/rv64zaamo-valid.s:17` keeps `CHECK-RV32` diagnostics. A
   scan found many affected YSX test files, not isolated leftovers.

2. AC-3 still has residual copied unsupported-feature source metadata. These are
   small compared with earlier rounds, but they still violate the "no support
   code for removed features" requirement: `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:167`
   still defines `PseudoFloatLoad`; `llvm/lib/Target/YuShuXin/YSXInstrInfo.h:346`
   still declares floating-point mask constants; `llvm/lib/Target/YuShuXin/YSXTargetMachine.cpp:23`
   still includes a GlobalISel header after GISel removal; and stale RISC-V/vector
   comments remain at `llvm/lib/Target/YuShuXin/YSXTargetMachine.cpp:9`,
   `llvm/lib/Target/YuShuXin/YSXTargetMachine.cpp:56`, and
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:797`.

3. The mutable tracker treated stale test trimming as queued. I corrected this
   during review: it is now a blocking AC-4 side issue, Plan Version is 38, and
   the Round 18 completed row is marked `18 review partial`.

## Blocking Side Issues

None outside the mainline gaps above. The blockers are AC-3 residual source
metadata and AC-4 stale copied test surfaces.

## Queued Side Issues

- Immutable tracker drift remains documented: review continues against
  `docs/plan.md`, which has AC-1 through AC-4.
- CPU/tune target-attribute diagnostics still use generic warn-and-ignore
  behavior. Keep this queued unless a probe shows unsupported feature leakage.

## Required Implementation Plan

1. Delete the remaining unsupported source metadata: remove `PseudoFloatLoad`,
   remove the floating-point mask constants, drop the stale GlobalISel include,
   and rewrite/delete stale RISC-V/vector/floating-point comments so the YSX
   source describes only rv64ima behavior.

2. Regenerate or manually trim YSX tests so only active `ysx64`/`rv64ima`
   prefixes remain. Remove inactive RV32, ILP32/LP64E, Zbb, XThead,
   RV64IA-TSO, WMO/NOZACAS, and other unsupported copied prefixes. For large
   autogenerated CodeGen files, rerun the LLVM update scripts with only the YSX
   RUN lines/prefixes that should remain, then inspect the diff for stale
   prefixes. For MC files, remove dead `CHECK-RV32` and RV32 ELF blocks while
   preserving retained rv64ima positive checks and explicit unsupported-feature
   negative tests.

3. Add or keep negative tests only where they execute an unsupported YSX input.
   Do not keep inactive check blocks as documentation for unsupported variants.

4. Revalidate after cleanup: `git diff --check`, `git diff -- llvm/lib/Target/RISCV | wc -l == 0`,
   the YSX-only build, the combined RISCV+YSX static build, focused YSX lit
   suites, smoke compiles, and negative probes for `+v`, FP, RV32/`+32bit`,
   compressed/Zc, vendor spelling, scalable-vector IR, and direct
   `llvm.riscv.vsetvli`.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 1 | Unjustified deferrals: 1

- AC-1: Maintained; RISCV source diff remains zero.
- AC-2: Advanced and reported negative probes still reject unsupported
  front-door inputs.
- AC-3: Advanced but incomplete because small FP/GISel/source-metadata remnants
  remain.
- AC-4: Addressed but incomplete. Tests exist and reportedly pass, but many
  YSX-owned files still document unsupported copied variants instead of only the
  retained target surface.

## Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

- Bumped the tracker to Plan Version 38.
- Added a Round 18 review plan-evolution entry.
- Moved stale unsupported YSX check-prefix cleanup from queued to blocking AC-4
  work.
- Added a blocking AC-3 issue for residual copied FP/GISel/source metadata.
- Marked the Round 18 completed row as `18 review partial`.

REQUIRES MORE WORK
<!-- CODEX's REVIEW RESULT  END  -->
---

## Goal Tracker Reference

Before starting work, **read** @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md to understand:
- The Ultimate Goal and Acceptance Criteria you're working toward
- Which tasks are Active, Completed, or Deferred
- Which side issues are blocking vs queued
- Any Plan Evolution that has occurred
- The latest side-issue state that needs attention

**IMPORTANT**: Keep the mutable section of `goal-tracker.md` up to date during the round.
Do NOT change the immutable section after Round 0.
If you cannot safely reconcile the tracker yourself, include an optional "Goal Tracker Update Request" section in your summary (see below).

## Mainline Guardrails

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-19-contract.md stable for this round
- Do not let queued issues take over the round
- If Codex reported several findings, classify them into:
  - mainline gaps
  - blocking side issues
  - queued side issues
- Only mainline gaps and blocking side issues should drive the next code changes

---

Note: You MUST NOT try to exit by lying, editing loop state files, or executing `cancel-rlcr-loop`.

After completing the work, please:
0. If the `code-simplifier` plugin is installed, use it to review and optimize your code. Invoke via: `/code-simplifier`, `@agent-code-simplifier`, or `@code-simplifier:code-simplifier (agent)`
1. Commit your changes with a descriptive commit message
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-19-summary.md
3. Run `/home/zhaosiying/.codex/skills/humanize/scripts/rlcr-stop-gate.sh` to advance the loop in-session

## Task Tag Routing Reminder

Follow the plan's per-task routing tags strictly:
- `coding` task -> Claude executes directly
- `analyze` task -> execute via `/humanize:ask-codex`, then integrate the result
- Keep Goal Tracker Active Tasks columns `Tag` and `Owner` aligned with execution

**Optional fallback**: if you could not safely update the mutable section of `goal-tracker.md` directly, include this section in your summary:
```markdown
## Goal Tracker Update Request

### Requested Changes:
- [E.g., "Mark Task X as completed with evidence: tests pass"]
- [E.g., "Add to Blocking Side Issues: bug Y blocks AC-2"]
- [E.g., "Add to Queued Side Issues: cleanup Z is non-blocking"]
- [E.g., "Plan Evolution: changed approach from A to B because..."]
- [E.g., "Defer Task Z because... (impact on AC: none/minimal)"]

### Justification:
[Explain why these changes are needed and how they serve the Ultimate Goal]
```

Codex will review your request and reconcile the Goal Tracker if justified.
