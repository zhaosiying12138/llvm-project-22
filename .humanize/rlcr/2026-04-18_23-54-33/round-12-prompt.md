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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-12-contract.md

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
# Round 11 Review Result

Mainline Progress Verdict: ADVANCED

Round 11 made real but narrow AC-3 progress. The claimed
`YSXInstrInfo.cpp`/`.h` cleanup is largely accurate: the searched vector/FP
verifier/comment/pseudo macro and `#if 0` compatibility-body patterns are gone
from those two files, `git diff --check` is clean, and RISCV backend diff
remains zero.

The original `docs/plan.md` task3 is still incomplete. Claude's Round-11
contract explicitly put BaseInfo/TableGen metadata, lowering/DAG selection,
frame/register/disassembler cleanup, and stale test cleanup out of scope. Those
are not acceptable final deferrals because they are core AC-3/AC-4 plan work.

## Required Finding Classification

### Mainline Gaps

1. BaseInfo and TableGen still define the removed vector/FP metadata model.
   `YSXBaseInfo.h` retains vector TSFlag fields and helpers at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:66`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:72`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:80`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:118`, and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:169`. It also retains
   vector/FP operand kinds at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:419`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:437`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:443`, and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:459`. The generated
   vector pseudo table declarations and null compatibility shims remain at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:769` and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:846`.

2. `YSXInstrFormats.td` still encodes removed vector/FP instruction metadata.
   It retains vector constraint records at
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:64`, FP/vector opcode slots at
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:142`,
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:156`, and
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:161`, plus vector TSFlag fields
   throughout `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:208`. This still
   violates the rv64ima-only source-surface requirement.

3. Register metadata still carries RV32/vector scaffolding. `YSXRegisterInfo.td`
   still uses RV32/RV64 hardware modes at
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:34` and
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:175`, and its base register
   class still has `IsVRegClass`, `VLMul`, and `NF` fields at
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:184`. This is plan-derived
   AC-3 pruning work, not a queued cleanup.

4. Vector lowering and custom ISD surfaces remain extensive. `YSXISelLowering.h`
   still exposes `RISCVVType::VLMUL`, scalable-vector container helpers,
   vector lowering declarations, VP lowering hooks, unaligned YSXVec helpers,
   and a generated YSXV intrinsics table at
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:343`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:371`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:518`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:553`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:584`, and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:656`. The implementation still
   contains direct `RISCVVType` and YSXVec lowering at
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:2640`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:13492`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:13526`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:13917`, and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:14095`.

5. DAG selection still carries copied vector intrinsic/pseudo selection. Live
   helper bodies select VLSEG/VSSEG/VLX/VSX pseudos through null pseudo lookups
   at `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:285`,
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:344`, and
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:392`. Disabled compatibility
   blocks remain at `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:463` and
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:2092`, and direct
   `Intrinsic::riscv_v*` switch handling remains around
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:2079`. This is exactly the
   lowering/selection deletion that `docs/plan.md` task3 requires.

6. Scalable-vector frame support is still present. `YSXFrameLowering.cpp`
   retains `TargetStackID::ScalableVector` filtering at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:463`, a YSXVec stack probing
   compatibility body under `#if 0` at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:523`, scalable-vector CFI
   expression emission at `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:573`,
   and prologue/epilogue scalable stack adjustment paths at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:993` and
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:1133`.

7. `YSXSelectionDAGInfo.h` still declares removed vector custom nodes such as
   `READ_VLENB`, `VMV_*`, and `VRGATHER*` at
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:79`,
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:213`, and
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:221`. These nodes should be
   deleted with their lowering and selector users, not kept as unsupported
   renamed stubs.

### Blocking Side Issues

1. Unsupported vector IR still reaches YSX backend abort paths. A fixed-width
   `<2 x i64>` add scalarizes to rv64ima integer instructions, but a scalable
   vector function:
   `define <vscale x 2 x i64> @svadd(<vscale x 2 x i64> %a, <vscale x 2 x i64> %b)`
   aborts in `llc` with `LLVM ERROR: Don't know how to legalize this scalable vector type`.
   A direct `llvm.riscv.vsetvli.i64` intrinsic also aborts with
   `LLVM ERROR: Cannot select: intrinsic %llvm.riscv.vsetvli`. This blocks safe
   AC-2/AC-4 behavior: unsupported vector input must reject deterministically
   or fall back only where generic scalar lowering is valid, not crash through
   leftover RISCV vector machinery.

### Queued Side Issues

1. Stale copied CodeGen check-prefix blocks remain a valid follow-up, but they
   should not replace the next AC-3 deletion round. Once the source surface is
   actually rv64ima-only, trim or regenerate YSX-owned tests so unsupported
   RV32/Z*/vector/FP prefixes are not kept as inert copied material.
2. The YSX `target("cpu=...")` and `target("tune=...")` warn-and-ignore policy
   remains non-blocking unless the project decides unsupported target attributes
   must always be hard errors.
3. Dead disassembler helper cleanup should be handled with the TableGen
   decoder/register-class cleanup, after the main lowering/TableGen pruning is
   underway.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 0 | Unjustified deferrals: 4

- AC-1: Maintained. Review checks still show zero RISCV backend diff.
- AC-2: Partially maintained for public target parsing/feature probes, but not
  complete because unsupported scalable-vector IR and direct RISCV vector
  intrinsic IR can still abort inside YSX codegen.
- AC-3: Still incomplete. Round 11 advanced `YSXInstrInfo.cpp`, but BaseInfo,
  TableGen, register metadata, lowering, DAG selection, SelectionDAGInfo, and
  frame lowering still retain removed vector/FP/scalable support code.
- AC-4: Partial. Claude's focused lit claim is plausible for the current slice,
  but coverage is incomplete until unsupported vector IR crash paths and the
  final rv64ima-only source surface are tested.

Tracker audit: I updated the mutable section of `goal-tracker.md` to Plan
Version 24, added the Round-11 review log entry, kept task3/task6 active, added
the unsupported-vector IR abort as a blocking side issue, and recorded the
Round-11 instruction-info slice as partially verified. I did not modify the
immutable section.

## Required Implementation Plan

Claude must make the next round a single AC-3 deletion round that completes the
remaining rv64ima source pruning, then revalidates AC-2/AC-4.

1. Delete vector/FP metadata from TableGen and BaseInfo. Remove vector TSFlags,
   vector constraints, VLMUL/SEW/VL/policy/VXRM fields, vector operand kinds,
   vector pseudo tables, FP/vector opcode-format leftovers that are no longer
   used by rv64ima, and the null `get*Pseudo` compatibility shims. Keep only the
   formats, operands, opcodes, and helper enums required for integer, M,
   atomic, branch, call, relocation, and exact asm paths.

2. Collapse register metadata to rv64ima. Remove RV32 hardware-mode scaffolding
   from YSX register definitions, delete vector register-class metadata fields,
   and ensure generated register info contains only the GPR classes needed by
   the retained scalar backend.

3. Delete vector custom ISD nodes and lowering. Remove `RISCVVType` use,
   `YSXVec` helper APIs, scalable-container conversion helpers, vector tuple
   logic, VP/vector intrinsic lowering, fixed/scalable vector legalization,
   vector combine code, vector FP conversion/rounding paths, and generated
   YSXV intrinsic lookup declarations from `YSXISelLowering.*` and
   `YSXSelectionDAGInfo.h`. Preserve generic fixed-vector scalarization only
   where LLVM already lowers to scalar rv64ima code without YSX vector nodes.

4. Delete vector DAG-selection paths. Remove RISC-V vector intrinsic includes
   and switches, VSETVLI/XSFMM selectors, segment load/store selectors,
   `selectVLSEG`/`selectVSSEG`/`selectVLXSEG`/`selectVSXSEG`, vector pseudo
   opcode macros, and all remaining `#if 0` compatibility blocks from
   `YSXISelDAGToDAG.cpp`.

5. Remove scalable-vector frame/register/disassembler remnants. Delete YSXVec
   stack-size accounting, scalable stack offset adjustment, VLENB CFI
   expression generation, vector callee-save paths, vector stack IDs, vector
   spill/reload scaffolding, and dead compressed/vector/FP decoder helpers.
   Keep only a direct fatal diagnostic for impossible scalable stack objects if
   a generic LLVM API can still present one.

6. Add negative regression coverage before declaring completion. Add `llc`
   tests proving scalable-vector IR and direct `llvm.riscv.vsetvli` IR do not
   abort through YSX selection. Keep the existing public negative tests for
   RV32, F/D, C, V, vendor, and `-mattr` feature enabling.

7. Rebuild and test after the deletion. Required validation: `git diff --check`;
   `git diff -- llvm/lib/Target/RISCV | wc -l` must remain `0`; YSX-only
   `LLVMYSXCodeGen llvm-mc clang`; combined `LLVMYSXCodeGen LLVMRISCVCodeGen
   llvm-mc llc clang lld`; focused YSX lit; YSX clang smoke compile; combined
   RISCV smoke compile; negative MC/Clang/llc probes for removed ISA surfaces
   and unsupported vector IR.

## Verification Performed In This Review

- Read `docs/plan.md`, the Round-11 prompt, contract, summary, and
  `goal-tracker.md`.
- Verified the searched Round-11 `YSXInstrInfo.cpp`/`.h` cleanup pattern has no
  matches.
- Ran `git diff --check`: clean.
- Ran `git diff -- llvm/lib/Target/RISCV | wc -l`: `0`.
- Confirmed fixed-width vector IR scalarizes to rv64ima integer asm.
- Confirmed scalable-vector IR and direct `llvm.riscv.vsetvli` IR abort in
  current YSX `llc`.
- Did not rerun full Ninja/lit in this review; Claude's build claims are not
  contradicted by the source and lightweight probe results, but the full plan
  remains incomplete.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-12-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-12-summary.md
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
