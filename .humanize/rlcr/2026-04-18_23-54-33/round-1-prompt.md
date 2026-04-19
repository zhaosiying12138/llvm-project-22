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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-1-contract.md

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
# Round 0 Review

Mainline Progress Verdict: ADVANCED

Goal Alignment Summary
`ACs: 3/4 addressed | Forgotten items: 2 | Unjustified deferrals: 1`

## Mainline Gaps

1. `task3` / AC-3 is not complete. The implementation added front-door `rv64ima` guards, but it did not prune the backend itself to an `rv64ima`-only implementation. The committed YSX tree still retains large amounts of removed-feature support code:
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:2351-2397` still includes FP, vector, compressed, bitmanip, P-extension, and vendor TD fragments such as `YSXInstrInfoF.td`, `YSXInstrInfoD.td`, `YSXInstrInfoQ.td`, `YSXInstrInfoZfh.td`, `YSXInstrInfoV.td`, `YSXInstrInfoZvk.td`, `YSXInstrInfoP.td`, `YSXInstrInfoC.td`, `YSXInstrInfoZc.td`, `YSXInstrInfoXTHead.td`, `YSXInstrInfoXSf.td`, `YSXInstrInfoXqci.td`, `YSXInstrInfoXMips.td`, `YSXInstrInfoXRivos.td`, and `YSXInstrInfoXAndes.td`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:289-315`, `390-430`, `481-510`, `688-705`, and `1128-1144` still define F/D/Q, C/Zc, Zb, V, and vendor extensions.
   - `llvm/lib/Target/YuShuXin/YSXSchedule.td:49-140` still defines floating-point scheduler resources.
   - `llvm/lib/Target/YuShuXin/YSXSubtarget.h:171-304` and `YSXSubtarget.cpp:38-74,242-247` still expose compressed/FP/vector/vendor helpers and options such as RVV LMUL knobs, P-extension SIMD, and MIPS vendor toggles.
   - `llvm/lib/Target/YuShuXin/YSXTargetMachine.cpp:23,55-67,91-99` still carries GlobalISel residue and RVV-specific target options.
   This directly conflicts with `docs/plan.md:41-52` and `docs/plan.md:79-95`, which require removal of RV32, FP, C, RVV, vendor, and other non-IMA support code rather than preserving it behind validation.

2. The round summary and tracker misclassify unfinished pruning as a non-blocking line-count follow-up. That is not a valid deferral. `round-0-summary.md` says the backend was pruned to an `rv64ima`-only surface and only exceeds a “historical rough size target,” but the actual gap is functional scope in the backend sources, not just size. The mutable tracker repeated the same mistake by marking `task3` complete and queueing a size-only follow-up in `goal-tracker.md:69-97`. I updated the mutable section to reopen `task3` and `task6`; do not treat this as optional cleanup.

### Required implementation plan

1. Reduce the YSX TableGen and subtarget surface to the retained `rv64ima` subset. Edit `YSXInstrInfo.td`, `YSXSchedule.td`, `YSX.td`, `YSXFeatures.td`, `YSXProcessors.td`, `YSXSubtarget.h`, `YSXSubtarget.cpp`, and `YSXTargetMachine.cpp` so YSX only defines the 64-bit integer core plus `M`/`A` and the required helper extensions that remain part of `rv64ima` lowering (`zmmul`, `zaamo`, `zalrsc`). Remove all FP, compressed, vector, RV32, bitmanip, P-extension, and vendor feature definitions, scheduler resources, subtarget helpers, hidden options, and TD includes.
2. Delete the now-unreachable YSX files that only serve removed features, and then fix any stale references exposed by that deletion. The pruning pass must cover TD fragments, scheduling files, lowering helpers, MC helpers, and any C++ source that still assumes RV32/FP/C/V/vendor state. The end state is a self-contained backend whose source tree matches the supported surface instead of depending on `YSXFeatures::validate` to reject copied functionality.
3. Re-run validation after pruning, not before. Rebuild the YSX-only and RISCV+YSX configurations, rerun the YSX smoke tests, and rerun the YSX LLVM/Clang test directories. Add focused negative coverage proving that removed surfaces stay rejected after the code deletion, including `-mattr=+f`, `-mattr=+c`, `-mattr=+zbb`, and `.option arch, rv64gc`.

## Blocking Side Issues

1. Goal-tracker drift was real. The immutable acceptance-criteria section in `goal-tracker.md:28-57` only contains AC-1 and AC-2, so AC-3 and AC-4 were effectively forgotten from the tracker contract even though they remain in `docs/plan.md`. I did not edit the immutable section per instructions, but the next round must continue treating `docs/plan.md` as the source of truth and must not declare completion until AC-3 and AC-4 are satisfied.

## Queued Side Issues

1. `ysx64` still reuses generic RISC-V frontend/parser surfaces instead of owning a minimized YSX-specific layer. `clang/lib/Basic/Targets.cpp:480-512` returns `RISCV64TargetInfo` for `ysx64`, `clang/lib/Basic/Targets/RISCV.h:37-52` enables generic RISC-V float/vector capabilities in that target info, and `llvm/include/llvm/TargetParser/YSXISAInfo.h:12-18` is only an alias to the RISCV parser. Current driver/backend guards make this non-blocking for the reopened pruning round, but it is still larger than the minimal target-plumbing surface described in the plan.

## Goal Alignment Notes

- AC-1: addressed. I confirmed the combined build contains both `libLLVMYSXCodeGen.a` and `libLLVMRISCVCodeGen.a`, and the current tree has distinct YSX target registration/plumbing.
- AC-2: addressed, but mostly via guardrails. I confirmed `clang --target=ysx64 -### -march=rv64gc` and `-mabi=lp64d` fail, and `llvm-mc` rejects `.option arch, rv64gc` plus `-mattr=+f/+c`. The guardrail implementation is not enough to satisfy AC-3.
- AC-3: not addressed. The backend still contains extensive removed-feature code, so the plan’s pruning milestone is incomplete.
- AC-4: addressed at the test-tree level, but forgotten in the immutable tracker AC list. The copied YSX test directories exist and the original RISCV test trees were not modified in the round commit.
- Forgotten items: AC-3 and AC-4 were dropped from the tracker’s immutable acceptance-criteria section.
- Deferred items: the “historical 30k LOC” deferral is unjustified because the real gap is retained unsupported functionality, which is mainline work.
- Plan evolution: the previous mutable tracker evolution was invalid. I updated `goal-tracker.md` to reopen the missing mainline work.

## Validation Notes

- Confirmed locally: driver rejection of bad `-march`/`-mabi`, assembler rejection of bad `.option arch`, assembler rejection of `-mattr=+f/+c`, and coexistence of YSX and RISCV static libraries in the combined build.
- Not rerun here: the full `llvm-lit` YSX suite. In this sandbox, `clang/test/lit.cfg.py` fails while creating temporary files under the build tree test exec root, so the claimed 129-test pass remains unverified by this review session.
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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-1-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-1-summary.md
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
