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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-9-contract.md

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
# Round 8 Review Result

Mainline Progress Verdict: ADVANCED

Round 8 made real AC-3 progress: the copied broad YSX arch parser is now an
exact `rv64ima` parser, stale pass declarations were removed from `YSX.h`, and
the reviewed `.option arch` probes now reject `rv32ima` and `rv64ima_zbb`.
However, the Round-8 contract is not complete. One of its explicit success
criteria was deleting the direct `YSXVType` forwarding layer, and that layer
still exists with many live callers. The original `docs/plan.md` also remains
incomplete because copied vector/FP/vendor/RV32/compressed lowering,
selection, metadata, frame, subtarget, and tune/profile surfaces are still
present.

## Goal Alignment Summary

```text
ACs: 4/4 addressed | Forgotten items: 0 | Unjustified deferrals: 1
```

- AC-1: Preserved. `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`,
  and this round did not reintroduce a RISCV backend diff.
- AC-2: Advanced for full `.option arch` strings. Manual probes confirm
  `.option arch, rv64ima` accepts, while `.option arch, rv32ima` and
  `.option arch, rv64ima_zbb` reject. AC-2 is still not complete because MC
  accepts copied non-IMA tune/profile `-mattr` flags.
- AC-3: Advanced but still incomplete. The parser and stale declarations were
  pruned, but the required `YSXVType` deletion and broader unsupported source
  deletion remain unfinished.
- AC-4: Preserved for the reviewed MC parser behavior, but needs new negative
  coverage for the remaining MC `-mattr` feature leaks.

I updated the mutable section of `goal-tracker.md` to Plan Version 18: Round 8
is recorded as a partial verified parser/pass-declaration slice, task3/task6
remain active after review, and the MC tune/profile `-mattr` leak is now a
blocking side issue.

## Mainline Gaps

### 1. The Round-8 contract item to remove `YSXVType` was not completed

Severity: Mainline Gap, blocks AC-3 completion.

The Round-8 contract said: "The direct `YSXVType` forwarding layer is removed".
It is still present at `llvm/include/llvm/TargetParser/YSXISAInfo.h:156`,
where YSX continues to re-export RISCV vector helpers such as `decodeVLMUL`,
`encodeVTYPE`, `printVType`, `TAIL_AGNOSTIC`, and the LMUL constants.

This is not just a dead header leftover. A targeted scan still finds 146
`YSXVType` hits under YSX, including live uses in:

- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXInstPrinter.cpp`

Claude's summary correctly admits this was left for a future slice, but that is
an incomplete Round-8 contract item and an incomplete original-plan task. It
must remain mainline work, not a queued cleanup.

### 2. MC still accepts copied non-IMA tune/profile feature flags

Severity: Mainline Gap, blocks AC-2/AC-3 completion.

`YSXFeatures.td` still defines copied vector/compressed/vendor/profile tune
features, including `log-vrgather`, `enable-vsetvli-sched-heuristic`,
`optimized-zero-stride-load`, `optimized-nf*-segment-load-store`,
`vl-dependent-latency`, `dlen-factor-2`, `conditional-cmv-fusion`,
`single-element-vec-fp64`, `vxrm-pipeline-flush`,
`prefer-vsetvli-over-read-vlenb`, and vendor family selectors at
`llvm/lib/Target/YuShuXin/YSXFeatures.td:162`.

The MC feature filter rejects unknown enabled features, but it passes through
any known non-required feature at
`llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp:94`. As a result,
these probes unexpectedly succeed:

```text
llvm-mc -triple=ysx64 -mattr=+vxrm-pipeline-flush
llvm-mc -triple=ysx64 -mattr=+log-vrgather
llvm-mc -triple=ysx64 -mattr=+single-element-vec-fp64
llvm-mc -triple=ysx64 -mattr=+prefer-vsetvli-over-read-vlenb
llvm-mc -triple=ysx64 -mattr=+andes45
```

This contradicts the plan requirement that YSX expose only the `rv64ima` ISA
surface and contain no removed feature/scheduling/profile code.

### 3. Broader AC-3 backend source pruning is still incomplete

Severity: Mainline Gap, blocks full plan completion.

Representative retained source surfaces:

- `llvm/lib/Target/YuShuXin/YSXSubtarget.h:163` still contains false-return
  compatibility APIs for compressed, FP, vector, bitmanip, and other removed
  extensions.
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp` still has YSXVec stack and
  vector CFI paths, including `getYSXVecStackSize` and `hasVInstructions`
  uses.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td` and
  `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h` still carry VXRM,
  VTYPE/VL, overlap, mask, and vector TSFlags metadata.
- `YSXISelLowering`, `YSXISelDAGToDAG`, and `YSXInstrInfo` still contain large
  copied vector/FP/vendor/RV32/compressed lowering and selection bodies.

Round 8 should be counted as progress, not completion.

## Blocking Side Issues

- The `YSXVType` forwarding layer and live vector callers directly block the
  current AC-3 source-pruning objective.
- MC acceptance of copied tune/profile `-mattr` flags blocks the "only
  `rv64ima`" surface and keeps removed scheduling/profile code externally
  reachable.

## Queued Side Issues

- `target("cpu=...")` and `target("tune=...")` still using ignored-attribute
  diagnostics remains queued; it does not leak features into IR and should not
  displace the AC-3 pruning work.
- Stale copied YSX CodeGen check-prefix blocks remain queued until the source
  surfaces they describe have been deleted.

## Directive Implementation Plan

1. Make the next round's only mainline objective: delete the remaining
   `YSXVType` forwarding layer and the MC-visible copied tune/profile feature
   surface.
2. Remove the `namespace YSXVType` block from `YSXISAInfo.h`. Fix every build
   error by deleting the unsupported vector/VTYPE/VL/VXRM callers in
   `YSXISelLowering`, `YSXISelDAGToDAG`, `YSXInstrInfo`,
   `YSXRegisterInfo.h`, `YSXBaseInfo.h`, `YSXInstPrinter.cpp`, and
   `YSXAsmParser.cpp`; do not replace it with a new compatibility shim.
3. Delete vector immediate parsing/printing paths from the asm parser and
   inst printer, including VTYPE, SEW/LMUL, policy, mask, and XSfmm helpers.
   The remaining MC parser/printer surface should cover only scalar rv64ima
   operands and instructions.
4. Prune `YSXInstrFormats.td`, `YSXBaseInfo.h`, and dependent helpers so
   generated TSFlags no longer include vector round mode, VXRM, VL/mask
   dependency, VTYPE, EEW, overlap, reads-past-VL, or alt-format metadata.
5. Delete copied vector/compressed/vendor/non-IMA tune/profile features from
   `YSXFeatures.td`. Keep only `64bit`, `i`, `m`, `a`, `zmmul`, `zaamo`,
   `zalrsc`, `relax`, `exact-asm`, and explicitly justified scalar-only codegen
   toggles that are covered by YSX tests.
6. Tighten `filterYSXFeatureString` so enabled MC `-mattr` flags outside that
   retained set report `YSX only supports the rv64ima ISA`; disabled unknown or
   inherited RISCV flags may continue to be ignored only where required for
   Clang compatibility.
7. Add negative MC tests for `+vxrm-pipeline-flush`, `+log-vrgather`,
   `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and one
   vendor family selector such as `+andes45`.
8. Rebuild YSX-only and RISCV+YSX static configurations, rerun the focused YSX
   LLVM/Clang suites, run smoke compiles for `ysx64-unknown-elf` and
   `riscv64-unknown-elf`, and confirm `git diff -- llvm/lib/Target/RISCV |
   wc -l` remains `0`.

## Verification Notes

Manual review/probes in the existing YSX-only build:

- `.option arch, rv64ima` accepts.
- `.option arch, rv32ima` rejects with `invalid arch name 'rv32ima'`.
- `.option arch, rv64ima_zbb` rejects with `invalid arch name 'rv64ima_zbb'`.
- `llvm-mc -triple=ysx64 -mattr=+zbb` and `+v` reject.
- `llvm-mc -triple=ysx64 -mattr=+vxrm-pipeline-flush`, `+log-vrgather`,
  `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and `+andes45`
  still accept.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

I did not rerun the full ninja/lit validation during this review; the source
inspection and MC probes were sufficient to verify the Round-8 contract miss
and the remaining externally visible feature leak.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-9-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-9-summary.md
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
