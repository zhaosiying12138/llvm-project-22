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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-24-contract.md

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
# Round 23 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 23 advanced the mainline by deleting the exact short-forward-branch,
CCMOV, scheduler, and custom-ISD helper residue called out in Round 22. The
required Round-23 source scan is clean, RISCV backend diff remains zero, and
`git diff --check` is clean. The full `docs/plan.md` goal is still not complete:
YSX still exposes removed compressed/vendor/custom RISC-V relocation surface
through `.reloc`.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MAINTAINED. The RISCV backend diff remains zero, and the reviewed
  Round-23 changes do not add a new RISCV dependency.
- AC-2: PARTIAL. The reviewed unsupported feature/CSR/`.insn` surfaces remain
  closed, but `.reloc` still accepts removed compressed/vendor/custom RISC-V
  relocation names for `ysx64`.
- AC-3: PARTIAL. The Round-23 SFB/CCMOV/scheduler/custom-ISD residue is gone,
  but the YSX MC layer still imports broad RISC-V relocation name tables that
  include removed non-rv64ima surface.
- AC-4: PARTIAL. Existing focused suites reportedly pass, but there is no
  negative YSX MC coverage for rejected `.reloc` compressed/vendor/custom
  relocation names.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 48 records this Round-23 review, keeps task3/task6 active, replaces the
resolved SFB/scheduler blocker with the `.reloc` relocation-surface blocker, and
marks the Round-23 deletion slice as review-partial. The immutable section was
not modified.

## Mainline Gaps

1. AC-2/AC-3 remain incomplete: YSX accepts removed RISC-V relocation names
   through `.reloc`.

   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXAsmBackend.cpp:48` through
   `YSXAsmBackend.cpp:54` imports all names from both `RISCV.def` and
   `RISCV_nonstandard.def` into `YSXAsmBackend::getFixupKind`. That includes
   compressed relocations such as `R_RISCV_RVC_BRANCH`, vendor/nonstandard
   names such as `R_RISCV_QC_ABS20_U`,
   `R_RISCV_NDS_BRANCH_10`, and
   `R_RISCV_CHERIOT1_COMPARTMENT_HI`, plus the generic vendor/custom relocation
   range. `YSXELFObjectWriter.cpp:74` then returns relocation fixup kinds
   directly, so the names assemble successfully instead of rejecting.

   Manual probes with the current YSX-only `llvm-mc` confirm the leak:

   ```text
   .reloc ., R_RISCV_RVC_BRANCH, sym                 # accepted
   .reloc ., R_RISCV_QC_ABS20_U, sym                 # accepted
   .reloc ., R_RISCV_NDS_BRANCH_10, sym              # accepted
   .reloc ., R_RISCV_CHERIOT1_COMPARTMENT_HI, sym    # accepted
   .reloc ., R_RISCV_VENDOR, sym                     # accepted
   ```

   This is the same class of visible removed-surface leak as the earlier `.insn`
   opcode issue. It directly contradicts the plan requirement that YSX reject
   compressed, vendor, and other removed surfaces and not preserve unsupported
   feature support code.

## Blocking Side Issues

1. The `.reloc` relocation-name leak blocks completion of the current mainline
   objective. It is both user-visible AC-2 behavior and copied unsupported MC
   support code under AC-3.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. I found
   no new evidence that they leak unsupported target features into IR/codegen.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. In `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXAsmBackend.cpp`, replace the
   broad `RISCV.def`/`RISCV_nonstandard.def` `StringSwitch` import with an
   explicit whitelist of relocation names that YSX actually supports for
   rv64ima/RISC-V ELF compatibility. Do not include `R_RISCV_RVC_BRANCH`,
   `R_RISCV_RVC_JUMP`, `R_RISCV_VENDOR`, any `R_RISCV_CUSTOM*`, or any
   `RISCV_nonstandard.def` names.
2. Keep existing YSX `.reloc` coverage for retained object-compatible names
   such as `R_RISCV_NONE`, `R_RISCV_32`, `R_RISCV_64`,
   `R_RISCV_32_PCREL`, and `R_RISCV_SET32` passing.
3. Add negative MC tests, preferably in
   `llvm/test/MC/YSX/unsupported-features.s` or a dedicated YSX `.reloc`
   negative test, covering at minimum `R_RISCV_RVC_BRANCH`,
   `R_RISCV_RVC_JUMP`, `R_RISCV_VENDOR`, `R_RISCV_CUSTOM192`,
   `R_RISCV_QC_ABS20_U`, `R_RISCV_NDS_BRANCH_10`, and
   `R_RISCV_CHERIOT1_COMPARTMENT_HI`.
4. Require `rg "RISCV_nonstandard|R_RISCV_VENDOR|R_RISCV_RVC_|R_RISCV_QC_|R_RISCV_NDS_|R_RISCV_CHERIOT|R_RISCV_CUSTOM" llvm/lib/Target/YuShuXin llvm/test/MC/YSX`
   to show no YSX source matches and only intentional negative-test matches.
5. Rebuild the YSX-only and RISCV+YSX configured target sets, rerun the focused
   YSX lit suites, rerun retained rv64ima smoke tests and unsupported-feature
   probes, run `git diff --check`, and verify
   `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-24-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-24-summary.md
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
