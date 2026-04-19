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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-5-contract.md

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
# Round 4 Review Result

Mainline Progress Verdict: ADVANCED

Round 4 materially advanced the mainline by removing the reviewed default MC-visible `fence.i`, CSR/counter, privileged/debug, and hypervisor instruction leaks, replacing the copied feature-record universe with a small rv64ima-oriented `YSXFeatures.td`, and adding negative MC tests for the Round 3 examples. It is still not complete. The newly added strict feature validation now makes a real Clang compile for `--target=ysx64-unknown-elf` fail before producing an object, and the backend still carries large copied FP/vector/vendor/RV32/compressed lowering, selection, pass, and metadata surfaces. AC-2, AC-3, and AC-4 remain open.

I updated the mutable section of `goal-tracker.md` to Plan Version 10: the Round 3 MC leak blocker is resolved, task3/task6 remain active, the Clang compile regression is now a blocking side issue, and the Round 4 MC fix is recorded as completed evidence. I did not modify the immutable section.

## Part 1: Goal Tracker Audit

The immutable tracker still contains only AC-1 and AC-2, but `docs/plan.md` contains AC-1 through AC-4. Per the original plan requirement, this review audits all four ACs from `docs/plan.md`.

| AC | Status | Evidence if met/advanced | Blocker if not met | Justification if deferred |
|----|--------|---------------------------|--------------------|---------------------------|
| AC-1 | MET | `llc -mtriple=ysx64-unknown-elf` generated YSX assembly from simple IR; `git diff -- llvm/lib/Target/RISCV \| wc -l` is `0`; source scan found no YSX dependency on RISCV target generated headers/libraries. Claude also reports YSX-only and RISCV+YSX static builds pass. | None found in this round. I did not rerun Ninja because the external build dirs are outside the writable roots. | Not deferred. |
| AC-2 | PARTIAL | MC now rejects `fence.i`, `csrr`, raw CSR reads, `rdcycle/rdtime/rdinstret`, `mret/sret/wfi/dret`, `sfence.vma`, and `hfence.vvma`; retained I/M/A assembly such as `add`, `mul`, `lr.w`, and `sc.w` assembles. Unsupported `llvm-mc -mattr=+f/+c/+zbb/+v/+zicsr/+zifencei` rejects. | A real Clang compile fails for the supported target: `clang --target=ysx64-unknown-elf -c -x c /dev/null` and `clang --target=ysx64-unknown-elf -march=rv64ima -c -x c /dev/null` both fatally report `YSX only supports the rv64ima ISA`. | Not deferred. |
| AC-3 | PARTIAL | `YSXFeatures.td` is now 293 lines and no longer carries the old broad `FeatureStdExtZicsr/Zifencei/Zve/Zvl/F/D/Zca/Zbb` records; symbolic CSR generation is empty. | Large copied unsupported implementation remains in `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, `YSXSubtarget.h`, `YSX.h`, and TargetParser/Clang plumbing. This violates the plan requirement to remove support code for removed features. | Not deferred. Claude's "left for next pruning round" note is not a valid deferral because task3 remains the active AC-3 mainline. |
| AC-4 | PARTIAL | YSX-owned test directories exist; `llvm/test/MC/YSX/unsupported-features.s` now has negative coverage for the reviewed CSR/privileged/Zifencei MC examples. | The Clang driver tests only use `-###` for the positive target path and do not catch that real `%clang --target=ysx64-unknown-elf -c` fails. There is also no positive test for explicit `-march=rv64ima`. | Not deferred. |

Forgotten items:

- The tracker immutable section still omits AC-3 and AC-4 from `docs/plan.md`. This was previously queued because immutable content must not be edited, but reviews must continue using `docs/plan.md` as source of truth.
- Positive Clang compile coverage is missing. `clang/test/Driver/YSX/target-options.c:1` checks only `-###`; it does not execute the backend and missed the current crash/fatal error.
- Explicit `-march=rv64ima` is not tested as an accepted path even though it is the only supported ISA string.
- Task1 remains listed as "pending verification" in Completed and Verified. It is not a current implementation blocker, but the tracker still carries that verification gap.

Deferred items audit:

- The `Explicitly Deferred` table is empty.
- Claude's Round 4 summary says some unsupported code is intentionally left for later; that is not an accepted tracker deferral and does not override AC-3. The tracker correctly keeps task3 active.

Goal completion summary:

```text
Acceptance Criteria: 1/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 2-3
Critical blockers: real Clang YSX compile fails; residual unsupported source surface blocks AC-3
```

## Part 2: Mainline Drift Audit

The current round's mainline objective was clear: close the Round 3 MC leak and shrink the feature surface. That objective served the original plan and advanced AC-2/AC-3 directly. It did not finish the plan because AC-3 still requires source deletion beyond feature-record pruning, and AC-2 regressed/was exposed through the full Clang compile path.

True blocking side issues:

- Clang passes the inherited RISCV negative feature universe to the YSX backend, and YSX rejects those disabled features as fatal. This blocks AC-2 and AC-4 because the supported default target cannot compile.
- Residual unsupported implementation support remains in core backend source. This blocks AC-3 because the plan requires removed feature support code to be deleted, not hidden behind false helpers or `#if 0`.

Queued side issues:

- Immutable tracker AC drift for AC-3/AC-4 remains queued because the immutable section must not be edited.
- Stale copied CodeGen check-prefix blocks remain queued until the source and compile-path blockers are fixed.
- Residual comments saying RISC-V in YSX files are cleanup after structural pruning.

```text
Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 2
Queued Side Issues: 3
```

## Part 3: Implementation Review

### Finding 1: Real Clang YSX compilation fails for the supported target

Severity: Mainline Gap, blocks AC-2 and AC-4.

Commands run against the YSX-only build:

```sh
/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang \
  --target=ysx64-unknown-elf -c -x c /dev/null -o /tmp/ysx-default.o
# RC=1, fatal error: error in backend: YSX only supports the rv64ima ISA

/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang \
  --target=ysx64-unknown-elf -march=rv64ima -c -x c /dev/null -o /tmp/ysx-rv64ima.o
# RC=1, fatal error: error in backend: YSX only supports the rv64ima ISA
```

Source cause:

- `clang/lib/Basic/Targets.cpp:480-512` maps `ysx64` to `RISCV64TargetInfo`, so Clang still uses the RISCV target feature universe for YSX.
- `clang -### --target=ysx64-unknown-elf` emits `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, then many disabled RISCV features such as `-c`, `-f`, `-v`, `-zbb`, `-zicsr`, `-zifencei`, `-zve*`, `-zvl*`, and vendor `-x*` features.
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp:51-61` strips `+` or `-` and fatally rejects any feature name outside the rv64ima allow-list, so harmless disabled inherited features still kill the target machine.
- `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:33-43` has the same sign-blind validation pattern.

The existing positive driver test cannot catch this because `clang/test/Driver/YSX/target-options.c:1` only checks `-###`. AC-2 says `--target=ysx64-unknown-elf` defaults to rv64ima/lp64; that must be verified by an actual compile, not just by cc1 argument printing.

Action: Either stop emitting the inherited RISCV disabled feature universe for YSX by introducing YSX-specific Clang/TargetParser plumbing, or make YSX validation accept disabled unsupported features while still rejecting enabled unsupported features. Add positive tests for `%clang --target=ysx64-unknown-elf -c` and `%clang --target=ysx64-unknown-elf -march=rv64ima -c`.

### Finding 2: AC-3 remains incomplete; unsupported source surfaces are still retained

Severity: Mainline Gap, blocks AC-3.

Round 4 correctly reduced `YSXFeatures.td`, but the backend still contains large copied removed-feature implementations:

- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:482-540` still contains FP/Zfinx/D/Zdinx legalization paths behind false helpers; `:1890-1938` still handles RISCV vector load/store/segment intrinsics.
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:2079-2085`, `:2092-2125`, `:2511-2520`, and `:4070-4255` still contain vector and vendor-vector intrinsic selection, vector VL/splat helpers, and FP-as-int selection helpers.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:41-60` still defines compressed/vendor formats; `:64-70` and `:209-249` still define vector constraints, VL/SEW/vector policy TSFlags, and VXRM metadata.
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:456-574` retains FP/vector copy code inside `#if 0`; `:1961-2135` retains vector reassociation logic; `:4449-4474` keeps false-return YSXVec compatibility APIs.
- `llvm/lib/Target/YuShuXin/YSX.h:53-60` and `:80-90` still declare compress/vector/VSETVLI/VXRM passes that should not be part of an rv64ima backend surface.
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h:163-190` and `:287-324` retain many false unsupported feature and vector-size compatibility APIs.
- `llvm/include/llvm/TargetParser/YSXISAInfo.h:12-18` aliases RISCV ISA and vector type parsers directly into YSX.

This is not merely a line-count concern. The plan explicitly requires DAG lowering, DAG-to-DAG selection, custom ISD nodes, pseudos, scheduling/profile code, and removed feature support to be reduced to the retained rv64ima subset. `#if 0` blocks and false-return compatibility APIs still preserve the removed subsystems as source surface.

Action: Continue AC-3 pruning as the mainline task. Delete unsupported lowering/selection/pass/metadata code and generated callers instead of adding more false stubs. Keep only the I/M/A integer, multiply/divide, atomic, LP64, and minimal target mechanics required by rv64ima.

### Finding 3: Round 4 MC claims are valid but scoped

Severity: Not a blocker after this round; keep as verified evidence.

Manual checks now reject the Round 3 default MC leaks:

```sh
fence.i           # RC=1, unrecognized instruction mnemonic
csrr a0, mstatus # RC=1, unrecognized instruction mnemonic
csrr a0, 0       # RC=1, unrecognized instruction mnemonic
rdcycle a0       # RC=1, unrecognized instruction mnemonic
mret/sret/wfi/dret, sfence.vma, hfence.vvma # all RC=1
```

Retained rv64ima assembly still works:

```asm
add a0, a1, a2
mul a0, a1, a2
lr.w a0, (a1)
sc.w a0, a1, (a2)
```

`llvm/test/MC/YSX/unsupported-features.s:12-35` contains the expected negative coverage for the reviewed CSR, counter, privileged, debug, hypervisor, and Zifencei examples.

## Part 4: Goal Tracker Update Requests

I updated `goal-tracker.md` myself because tracker drift was present after Round 4:

- Plan Version is now 10.
- Added Round 4 and Round 4 review entries to the Plan Evolution Log.
- Kept task3 and task6 active with updated notes.
- Removed the resolved default MC CSR/privileged blocker from Blocking Side Issues.
- Added the real Clang compile failure as a new Blocking Side Issue.
- Recorded the Round 4 MC visible-surface fix in Completed and Verified.

No requested tracker change was rejected.

## Part 5: Progress Stagnation Check

Development is not stagnating. Rounds 2, 3, and 4 show repeated AC-3 feedback, but each round removed material surface area: `.option` feature toggles, FP/vector CSR aliases, compressed/vector TD includes and registers, the broad feature record universe, symbolic CSR records, and default privileged/CSR MC instructions. The remaining AC-3 problem is still large and recurring, but Round 4 made meaningful progress and uncovered a concrete new AC-2/AC-4 compile-path blocker.

## Action Items

Mainline Gaps:

1. Fix `clang --target=ysx64-unknown-elf -c` and `clang --target=ysx64-unknown-elf -march=rv64ima -c` so the supported default target compiles successfully.
2. Add positive Clang tests that execute the backend, not only `-###`, for default `ysx64` and explicit `-march=rv64ima`.
3. Continue AC-3 source pruning in `YSXISelLowering.*`, `YSXISelDAGToDAG.*`, `YSXInstrInfo.*`, `YSXInstrFormats.td`, `YSXSubtarget.*`, `YSX.h`, frame/MC helpers, and TargetParser/Clang plumbing.

Blocking Side Issues:

1. The Clang/RISCV feature-universe leakage into YSX blocks AC-2/AC-4 because default compilation fatally rejects disabled unsupported features.
2. False-return helpers, `#if 0` blocks, vector/FP TSFlags, vector pass declarations, and RISCV parser aliases block AC-3 until removed or replaced by minimal YSX rv64ima code.

Queued Side Issues:

1. Tracker immutable AC drift remains queued because immutable content must not be edited.
2. Stale copied CodeGen check-prefix blocks should be trimmed after the source surface and Clang compile blockers are fixed.
3. Residual "RISC-V" comments in YSX files should be cleaned after structural pruning.

Validation notes:

- Verified `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- Verified `llc -mtriple=ysx64-unknown-elf` emits valid YSX assembly for simple IR.
- Verified the reviewed MC negative cases now reject and retained I/M/A assembly still works.
- Did not rerun Ninja because the configured build directories are outside this session's writable roots.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-5-contract.md stable for this round
- Do not let queued issues take over the round
- If Codex reported several findings, classify them into:
  - mainline gaps
  - blocking side issues
  - queued side issues
- Only mainline gaps and blocking side issues should drive the next code changes

### Post-Alignment Check Action Items

This round follows a Full Goal Alignment Check. Pay special attention to:
- **Forgotten Items**: Codex may have identified tasks that were being ignored. Address them.
- **AC Status**: If any Acceptance Criteria were marked NOT MET, prioritize work toward those.
- **Deferred Items**: If any deferrals were flagged as unjustified, un-defer them now.
- **Queued Issues**: Keep non-blocking follow-up work queued unless it now clearly blocks mainline progress.

---

Note: You MUST NOT try to exit by lying, editing loop state files, or executing `cancel-rlcr-loop`.

After completing the work, please:
0. If the `code-simplifier` plugin is installed, use it to review and optimize your code. Invoke via: `/code-simplifier`, `@agent-code-simplifier`, or `@code-simplifier:code-simplifier (agent)`
1. Commit your changes with a descriptive commit message
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-5-summary.md
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
