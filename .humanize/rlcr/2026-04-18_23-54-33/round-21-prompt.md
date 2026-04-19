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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-21-contract.md

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
# Round 20 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 20 advanced the mainline by removing the exact Round 19 blocker slice:
the reviewed Zibi/CLUI/VTYPE/RVKR operand residue, dead CSR parser/printer
scaffolding, no-op CSR pass, and copied SiFive CLIC interrupt state/stubs are
gone. However, the original `docs/plan.md` goal is still not complete. YSX
still exposes removed ISA surface through `.insn`, and the corresponding
TableGen format support remains copied in the backend.

## Goal Alignment Summary

ACs: 4/4 addressed, 2/4 met | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MET. YSX remains standalone and RISCV source diff is still zero.
- AC-2: PARTIAL. Normal `-mattr`, `.option arch`, Clang, CSR, RV32, FP, vector,
  compressed, and vendor probes reject, but `.insn` still accepts removed
  FP/vector/custom opcode names and matching numeric major opcodes.
- AC-3: PARTIAL. Round 20 deleted the reviewed dead scaffolding, but copied
  removed-feature `.insn` opcode and format support remains.
- AC-4: PARTIAL. Existing focused suites reportedly pass, but there is no
  negative MC coverage for `.insn` removed opcode names/values.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 42 records this Round 20 review, keeps task3/task6 active, replaces the
resolved Round 19 blocker with the `.insn` blocker, and marks Round 20 as
review-partial. The immutable section was not modified.

## Mainline Gaps

1. AC-2/AC-3 remain incomplete: `.insn` accepts removed FP, vector, and custom
   opcode surface.

   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:63` through
   `YSXInstrFormats.td:88` still define `LOAD_FP`, `STORE_FP`, `MADD`, `MSUB`,
   `NMSUB`, `NMADD`, `OP_FP`, `OP_V`, `OP_VE`, and `CUSTOM_*` opcode names in
   the searchable opcode table. `YSXAsmParser.cpp:1189` then resolves any table
   match as a valid `.insn` opcode. Manual probes show these are assembler
   visible:

   ```text
   .insn r OP_FP, 0, 0, x1, x2, x3      # accepted, encodes opcode 0x53
   .insn r OP_V, 0, 0, x1, x2, x3       # accepted, encodes opcode 0x57
   .insn r4 MADD, 0, 0, x1, x2, x3, x4  # accepted, encodes opcode 0x43
   .insn r CUSTOM_0, 0, 0, x1, x2, x3   # accepted, encodes opcode 0x0b
   ```

   Numeric forms are also accepted, e.g. `.insn r 83, 0, 0, x1, x2, x3`,
   `.insn r 87, ...`, and `.insn r 11, ...`. This contradicts the rv64ima-only
   MC surface and means the removed-feature opcode support was not fully pruned.

2. AC-3 remains incomplete: dead FP/vector format classes remain in TableGen.

   `YSXInstrFormats.td:90` through `YSXInstrFormats.td:98` still define vector
   element dependency classes (`EltDepsVL`, `EltDepsMask`, `EltDepsVLMask`).
   `YSXInstrFormats.td:204` through `YSXInstrFormats.td:253` still defines
   `RVInstRFrm`, `RVInstR4`, and `RVInstR4Frm`; `YSXInstrInfo.td:982` and
   `YSXInstrInfo.td:1022` expose the R4 form through `.insn r4` aliases. These
   classes have no retained rv64ima instruction users and should not remain in
   the minimized backend.

## Blocking Side Issues

1. The `.insn` opcode leak blocks the current mainline objective from
   completing safely. It is both source scaffolding for removed features and a
   user-visible MC acceptance path for removed ISA opcode spaces.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. I found
   no new evidence that they leak unsupported features into codegen.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. In `YSXInstrFormats.td`, delete `YSXOpcode` entries for `LOAD_FP`,
   `STORE_FP`, `MADD`, `MSUB`, `NMSUB`, `NMADD`, `OP_FP`, `OP_V`, `OP_VE`, and
   `CUSTOM_0` through `CUSTOM_3`. Keep only retained rv64ima major opcodes:
   `LOAD`, `MISC_MEM`, `OP_IMM`, `AUIPC`, `OP_IMM_32`, `STORE`, `AMO`, `OP`,
   `LUI`, `OP_32`, `BRANCH`, `JALR`, `JAL`, and `SYSTEM`.
2. Restrict `YSXAsmParser::parseInsnDirectiveOpcode` so numeric `.insn`
   opcodes are accepted only if the value is one of the same retained rv64ima
   major opcodes. Removed opcode values must produce a deterministic assembler
   error.
3. Delete dead R4/FRM/vector dependency scaffolding: `InstFormatR4`,
   `EltDeps*`, `RVInstRFrm`, `RVInstR4`, `RVInstR4Frm`, `DirectiveInsnR4`,
   `InsnR4`, and the `.insn r4`/`.insn_r4` aliases. Update `YSXBaseInfo.h`
   accordingly.
4. Add MC negative tests in `llvm/test/MC/YSX/unsupported-features.s` for named
   and numeric removed opcode forms. At minimum cover `OP_FP`, `OP_V`, `MADD`,
   `CUSTOM_0`, `83`, `87`, `67`, and `11`.
5. Rebuild both configured targets, rerun focused YSX lit suites, smoke
   compiles, unsupported-feature probes, `git diff --check`, and RISCV
   zero-diff.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-21-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-21-summary.md
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
