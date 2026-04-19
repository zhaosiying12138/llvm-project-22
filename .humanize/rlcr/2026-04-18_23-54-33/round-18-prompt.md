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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-18-contract.md

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
# Round 17 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 17 advanced the selected FP/vector/generated-surface deletion slice. The
large reviewed markers from the Round 16 review are gone or inert across the
searched FP/vector surface, `git diff --check HEAD~1..HEAD` is clean, and
`git diff -- llvm/lib/Target/RISCV | wc -l` still returns `0`. Quick probes also
confirmed `ysx32`, explicit `+32bit`, `lp64f`, `+zca`, and a vendor extension
spelling are rejected by the current YSX tools.

The full original `docs/plan.md` goal is still incomplete. Claude's remaining
RV32/vendor source work and stale YSX check-prefix work are plan-derived AC-3
and AC-4 obligations, not an acceptable completion state.

## Mainline Gaps

1. RV32 and unsupported ABI source metadata remains in the backend. This is not
   just harmless wording: `llvm/lib/Target/YuShuXin/YSXFeatures.td:87` through
   `YSXFeatures.td:99` still define `Feature32Bit`, `IsRV32`, `RV32`, and RV64
   hardware modes; `MCTargetDesc/YSXBaseInfo.h:254` through
   `YSXBaseInfo.h:265` still enumerates `ILP32`, `ILP32F`, `ILP32D`,
   `ILP32E`, `LP64F`, `LP64D`, and `LP64E`; `MCTargetDesc/YSXBaseInfo.cpp:41`
   through `YSXBaseInfo.cpp:83` still contains generic non-YSX ABI fallback
   computation; and `YSXISelLowering.cpp:73` through `YSXISelLowering.cpp:98`
   still handles hard-float and ILP32/LP64E ABI cases. A target whose public
   surface is only `ysx64`/`rv64ima`/`lp64` should not keep this copied mode and
   ABI lattice.

2. Removed-extension false-query compatibility branches still drive lowering
   and selection. `YSXSubtarget.h:139` through `YSXSubtarget.h:187` and
   `YSXSubtarget.h:209` through `YSXSubtarget.h:237` still expose broad
   always-false C/FP/vector/Z*/vendor query wrappers. Live users remain in
   `YSXISelLowering.cpp:129` through `YSXISelLowering.cpp:153`,
   `YSXISelLowering.cpp:188` through `YSXISelLowering.cpp:219`,
   `YSXISelLowering.cpp:251` through `YSXISelLowering.cpp:337`,
   `YSXISelLowering.cpp:692` through `YSXISelLowering.cpp:726`,
   `YSXISelLowering.cpp:797` through `YSXISelLowering.cpp:810`,
   `YSXISelLowering.cpp:3513`, `YSXISelLowering.cpp:3571`, and
   `YSXISelLowering.cpp:3680` through `YSXISelLowering.cpp:3681`. Selection has
   the same pattern at `YSXISelDAGToDAG.cpp:807` through
   `YSXISelDAGToDAG.cpp:815`, `YSXISelDAGToDAG.cpp:1270` through
   `YSXISelDAGToDAG.cpp:1279`, and `YSXISelDAGToDAG.cpp:1572` through
   `YSXISelDAGToDAG.cpp:1574`.

3. Compressed/Zc and vendor MC relocation support is still present despite the
   `rv64ima` boundary. Examples: `MCTargetDesc/YSXFixupKinds.h:38` through
   `YSXFixupKinds.h:64` defines RVC, Qualcomm, and Andes fixups;
   `MCTargetDesc/YSXAsmBackend.cpp:85` through `YSXAsmBackend.cpp:97` describes
   those fixups, while `YSXAsmBackend.cpp:660` through `YSXAsmBackend.cpp:700`
   emits `R_RISCV_VENDOR`; `MCTargetDesc/YSXMCCodeEmitter.cpp:564` through
   `YSXMCCodeEmitter.cpp:589` still chooses RVC/QC/Andes fixups; and
   `MCTargetDesc/YSXBaseInfo.h:296` through `YSXBaseInfo.h:320` plus
   `MCTargetDesc/YSXBaseInfo.cpp:162` through `YSXBaseInfo.cpp:170` keep
   compressed/Zc helper declarations and inert compression stubs.

4. AC-4 is still not final. Many YSX-owned copied tests still retain inactive
   unsupported-variant check blocks, such as `llvm/test/CodeGen/YSX/shl-demanded.ll:6`,
   `llvm/test/CodeGen/YSX/atomic-rmw.ll:13`, `llvm/test/MC/YSX/elf-header.s:4`,
   and `llvm/test/MC/YSX/rv64zaamo-valid.s:17`. This should not replace the
   next AC-3 source-pruning step, but the original plan requires YSX tests to
   describe only the retained target surface before the loop can complete.

## Blocking Side Issues

None outside the mainline gaps above. The blocker is AC-3 incompleteness:
removed RV32, unsupported ABI, compressed/Zc, and vendor support remains as
metadata, generated modes, false wrappers, dead stubs, or MC relocation plumbing.

## Queued Side Issues

- Immutable tracker drift remains documented: the immutable section only lists
  AC-1 and AC-2, while `docs/plan.md` has AC-1 through AC-4. Continue reviewing
  against `docs/plan.md` and do not edit the immutable section.
- CPU/tune target-attribute diagnostics are still generic warn-and-ignore
  behavior. Keep this queued unless a probe shows feature leakage.

## Required Implementation Plan

1. Collapse YSX to a single 64-bit hardware mode. Delete `Feature32Bit`,
   `IsRV32`, `RV32`, and the RV32 entries from TableGen hardware-mode
   definitions in `YSXFeatures.td`, `YSXRegisterInfo.td`, `YSXInstrInfo.td`,
   and `YSXInstrInfoA.td`. Regenerate/fix generated users by making XLen and
   GPR register info unconditionally 64-bit.

2. Reduce ABI handling to `lp64` only. Remove ILP32 and hard-float ABI enum
   values from `YSXBaseInfo.h`, simplify `computeTargetABI` and `getTargetABI`
   to accept only empty/`lp64`, simplify `isSoftFPABI`, frame alignment, ELF
   eflags, and lowering ABI validation, then remove fallback hard-float warning
   paths.

3. Delete the remaining always-false removed-extension query surface. For each
   `hasStdExt*` and `hasVendor*` wrapper not needed for retained I/M/A/Zmmul/
   Zaamo/Zalrsc/relax behavior, remove the wrapper and simplify every caller to
   the scalar rv64ima branch. Do not replace them with new false wrappers.

4. Remove compressed/Zc and vendor MC plumbing. Delete RVC/QC/Andes fixup kinds,
   `R_RISCV_VENDOR` emission, QC MC expression support, RVC compression stubs,
   RVC target-streamer state, RVC/QC/Andes code-emitter cases, and corresponding
   ELF object-writer mappings. Keep only fixups and relocations reachable from
   rv64ima code, calls, TLS, and normal RISC-V ELF compatibility.

5. After the source surface is clean, trim or regenerate YSX tests so inactive
   RV32/Zbb/XThead/RV64IA-TSO/unsupported-prefix blocks are gone. Preserve the
   positive rv64ima checks and the existing negative unsupported-feature probes.

6. Revalidate both builds and focused tests: `git diff --check`,
   `git diff -- llvm/lib/Target/RISCV | wc -l == 0`, YSX-only build, combined
   RISCV+YSX static build, focused YSX lit suites, smoke compiles, and negative
   probes for `+v`, FP, RV32/`+32bit`, compressed/Zc, vendor spelling,
   scalable-vector IR, and direct `llvm.riscv.vsetvli`.

## Goal Alignment Summary

ACs: 4/4 addressed, 1/4 fully met | Forgotten items: 0 | Unjustified deferrals: 1

- AC-1: Maintained for this review; RISCV diff remains zero.
- AC-2: Addressed and quick negative probes still reject unsupported front-door
  inputs, but final status depends on deleting the remaining removed-feature
  source surface.
- AC-3: Advanced but incomplete. Round 17 removed the reviewed FP/vector slice,
  but RV32, unsupported ABI, compressed/Zc, and vendor support code remains.
- AC-4: Addressed but incomplete. Focused tests exist and are reported passing,
  but stale unsupported check-prefix blocks still need trimming after AC-3.

## Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

- Bumped the tracker to Plan Version 36.
- Added a Round 17 review plan-evolution entry.
- Changed the Round 17 verification row to `17 review partial`.
- Kept task3 and task6 active.
- Refined the Round 17 blocking issue to include RV32 hardware modes,
  unsupported ABI metadata, compressed/Zc stubs, and vendor MC relocation
  plumbing.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-18-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-18-summary.md
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
