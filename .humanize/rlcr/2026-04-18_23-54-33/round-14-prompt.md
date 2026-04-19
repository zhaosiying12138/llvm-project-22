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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-14-contract.md

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
# Round 13 Review Result

Mainline Progress Verdict: ADVANCED

Round 13 made real AC-3 progress for its narrow selector/disabled-block slice, but
the original `docs/plan.md` scope is still incomplete. I verified the targeted
claims that `YSXISelDAGToDAG.cpp` no longer contains the searched vector
subvector/tuple/`RISCVVType`/`VMV_V_X_VL` selector residue, and
`rg "#if 0|if \\(false" llvm/lib/Target/YuShuXin` has no matches. I also verified
`git diff --check`, `git diff -- llvm/lib/Target/RISCV | wc -l == 0`, and quick
negative probes for `+v`, `rv64imaf`, scalable-vector IR, and direct
`llvm.riscv.vsetvli`.

Do not treat the remaining items in Claude's summary as acceptable deferrals.
They are original-plan AC-3 work and must drive the next implementation round.

## Mainline Gaps

1. `YSXSelectionDAGInfo.h` still preserves a broad unsupported custom-ISD
   compatibility namespace instead of deleting unsupported nodes. The block
   starts at `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:19` and still
   declares FP, RV32, crypto/vendor, vector, tuple, and VLEN nodes, including
   `READ_VLENB` at line 75, `TUPLE_INSERT`/`TUPLE_EXTRACT` at lines 158-159,
   `VMV_*` at lines 208-211, and `VRGATHER*`/`VSEXT_VL` at lines 216-219.
   AC-3 requires removed custom ISD nodes to be gone, not preserved as inert
   constants.

2. `YSXISelLowering.*` still carries the copied RVV/VP/vector-intrinsic
   lowering surface. Examples include `RISCVVType::VLMUL` APIs in
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:343` and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:1378`, fixed/scalable YSXVec
   container helpers at `YSXISelLowering.h:378`, VP/vector lowering hooks at
   `YSXISelLowering.h:519`, the generated `YSXVIntrinsicsTable` declarations at
   `YSXISelLowering.h:656`, `IntrinsicsRISCV.h` at
   `YSXISelLowering.cpp:42`, and generated vector intrinsic implementation at
   `YSXISelLowering.cpp:23346`. The current IR guard reduces reachability, but
   it is not a substitute for source deletion.

3. BaseInfo, TableGen, and generated pseudo metadata are still not rv64ima-only.
   Vector TSFlags and helpers remain in
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:72`,
   vector operand kinds at `YSXBaseInfo.h:437`, segment/vector pseudo structs
   and null shims at `YSXBaseInfo.h:773`, generated table includes in
   `YSXBaseInfo.cpp:33`, vector pseudo table declarations in
   `YSXInstrInfo.h:366`, and generated pseudo table implementation in
   `YSXInstrInfo.cpp:60`. `YSXInstrFormats.td` still carries vector constraint
   commentary, FP opcode slots, `VLMul`, `HasSEWOp`, `HasVLOp`, `UsesVXRM`, and
   VL dependency TSFlags around lines 74, 143, 160, 212, 218, 221, 248, and 265.

4. Register, subtarget, and frame metadata still carry removed vector/RV32
   concepts. `YSXRegisterInfo.td` still has RV32/RV64 hardware-mode scaffolding
   at lines 34 and 175 plus vector register-class TSFlags at lines 186-191.
   `YSXSubtarget.h`/`.cpp` still expose VLEN/YSXVec helper APIs at
   `YSXSubtarget.h:276`, `YSXSubtarget.h:292`, `YSXSubtarget.h:345`, and
   `YSXSubtarget.cpp:187`. `YSXFrameLowering.cpp` still computes YSXVec stack
   state at line 413, filters scalable-vector callee saves at line 454, emits
   VLENB CFA expressions at line 513, adjusts scalable stack offsets at line
   928, accepts scalable-vector stack IDs at line 1217, assigns vector spill
   stack IDs at line 1859, and advertises `TargetStackID::ScalableVector` at
   line 2149.

5. Dead FP/vector decode and MC helper surfaces remain. For example,
   `llvm/lib/Target/YuShuXin/Disassembler/YSXDisassembler.cpp:412` still
   validates FP rounding immediates, and `YSXBaseInfo.h:478` still defines
   `YSXFPRndMode`. If no supported rv64ima path uses these helpers, they should
   be deleted with the rest of the unsupported FP/vector metadata.

## Blocking Side Issues

No new non-mainline crash blocker was found. The quick unsupported-vector probes
still reject deterministically rather than aborting. The blocker remains the
mainline AC-3 incompleteness: the backend still preserves large copied
unsupported source surfaces under compatibility names and unreachable helpers.

## Queued Side Issues

1. Stale copied YSX CodeGen check-prefix blocks for unsupported RISCV variants
   remain a valid follow-up after the source is actually rv64ima-only.
2. `target("cpu=...")` and `target("tune=...")` warn-and-ignore policy remains
   non-blocking unless the project decides all unsupported YSX attributes must
   hard-error.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 0 | Unjustified deferrals: 7

- AC-1: Maintained. RISCV backend diff remains zero.
- AC-2: Maintained for the reviewed negative probes; unsupported `+v`,
  `rv64imaf`, scalable-vector IR, and direct RISC-V vector intrinsic IR reject.
- AC-3: Advanced, but still incomplete. Round 13 removed the narrow DAG selector
  residue and disabled blocks, while broad custom-ISD, lowering, metadata,
  frame, subtarget, disassembler, and generated-pseudo surfaces remain.
- AC-4: Maintained for the claimed focused suite and quick probes, but final
  coverage must be rerun after the remaining source deletion.

Tracker audit: I updated the mutable tracker to Plan Version 28, added a Round
13 review log entry, kept task3/task6 active, added the residual backend source
surface as a blocking side issue, and recorded Round 13 as a verified partial
deletion slice. I did not modify the immutable section.

## Required Implementation Plan

Claude must continue with a single AC-3 source-deletion round. Do not add
another front-door guard and do not leave compatibility stubs for removed
features.

1. Delete `YSX_UNSUPPORTED_ISD` from `YSXSelectionDAGInfo.h`. Keep only generated
   scalar rv64ima custom ISD nodes that are actually selected or lowered.
   Compile, then remove every lowering/combine/calling-convention caller that
   referenced a deleted unsupported node.

2. Collapse `YSXISelLowering.h` and `YSXISelLowering.cpp` to scalar rv64ima.
   Remove `IntrinsicsRISCV.h` unless masked atomics still require it, delete
   `RISCVVType`, `YSXVIntrinsicsTable`, YSXVec container helpers, fixed-length
   vector-to-YSXVec lowering, VP hooks, vector memory lowering, vector
   reductions, vector combines, tuple lowering, and VLEN handling. Fixed vectors
   may only use generic LLVM scalarization.

3. Remove vector/FP/RV32 metadata from BaseInfo and TableGen. Delete vector
   TSFlags, vector operand kinds, VL/SEW/policy/VXRM fields, segment/vector
   pseudo structs, generated vector pseudo table declarations/implementations,
   unused FP opcode slots, and RV32-only metadata. Keep integer, M, A, branch,
   call, relocation, and exact-asm metadata.

4. Flatten register and subtarget state to rv64ima. Remove RV32 hardware-mode
   scaffolding, vector register-class TSFlags, VLEN/YSXVec helper APIs, and
   any subtarget method that exists only to satisfy deleted vector lowering.

5. Delete scalable-vector frame support. Remove YSXVec padding, scalable stack
   offsets, VLENB CFA expression construction, scalable-vector callee-save
   filtering, vector spill stack IDs, scalable-vector scavenging, and
   `getStackIDForScalableVectors()` support.

6. Prune dead disassembler, parser, and MC helpers exposed only by removed
   FP/vector instructions, including FP rounding decode helpers when no retained
   instruction uses them.

7. Revalidate with `git diff --check`; `git diff -- llvm/lib/Target/RISCV |
   wc -l == 0`; YSX-only `LLVMYSXCodeGen llvm-mc clang llc`; combined
   `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`; focused YSX lit
   suites; YSX and RISCV smoke compiles; and negative probes for `+v`,
   `rv64imaf`, scalable-vector IR, direct `llvm.riscv.vsetvli`, and any newly
   deleted MC/parser surfaces.
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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-14-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-14-summary.md
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
