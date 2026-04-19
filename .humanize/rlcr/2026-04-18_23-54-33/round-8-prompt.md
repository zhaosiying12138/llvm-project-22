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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-8-contract.md

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
# Round 7 Review Result

Mainline Progress Verdict: ADVANCED

Round 7 correctly advances the narrow frontend hardening objective from the
Round 7 contract. The inherited Clang feature paths called out in Round 6 are
now fenced by YSX-owned feature-map handling, target-feature validation,
target-attribute parsing, feature-name validation, and FP/vector asm constraint
rejection. However, the original `docs/plan.md` is still not complete: AC-3
backend source pruning remains active and was explicitly placed out of scope for
this round. Do not stop the loop.

## Goal Alignment Summary

```text
ACs: 4/4 addressed, 1/4 still incomplete | Forgotten items: 0 | Unjustified deferrals: 1
```

- AC-1: Preserved. `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`,
  and the Round 7 change did not touch the RISCV backend.
- AC-2: Advanced. Manual probes confirm the reviewed Clang target-attribute,
  explicit cc1 target-feature, and FP/vector asm-constraint leaks now reject
  before unsupported IR/backend emission.
- AC-3: Still incomplete. The YSX backend still retains copied vector, FP,
  compressed, vendor, RV32, VSETVLI/VXRM, and compatibility-helper surfaces.
- AC-4: Advanced. `clang/test/Driver/YSX/target-options.c` now contains
  negative coverage for `target("arch=+v")`, `target("arch=rv64imaf")`,
  `target("arch=+64bit")`, explicit cc1 `+v`, and `"f"`/`"vr"` asm
  constraints.

I updated the mutable section of `goal-tracker.md` to Plan Version 16: Round 7
frontend hardening is marked review-verified, task3/task6 remain active, and a
non-blocking CPU/tune target-attribute diagnostic note is queued.

## Mainline Gaps

### 1. AC-3 backend source pruning remains incomplete and was deferred out of Round 7

Severity: Mainline Gap, blocks full plan completion.

Claude's Round 7 frontend work is real, but the original plan requires the
copied backend to remove unsupported implementation code, not merely hide it
behind frontend rejection and false-return helpers. Current representative
evidence:

- `llvm/include/llvm/TargetParser/YSXISAInfo.h:127` still parses `rv32`, `g`,
  `c`, `f`, `d`, `v`, and arbitrary `z`/`s`/`x` extension groups before other
  callers reject them.
- `llvm/include/llvm/TargetParser/YSXISAInfo.h:206` still forwards `YSXVType`
  directly to `RISCVVType` vector helpers.
- `llvm/lib/Target/YuShuXin/YSX.h:53` still declares compressed, gather/scatter,
  vector peephole, VSETVLI, and VXRM passes.
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h:163` still contains false-return
  compatibility APIs for compressed, FP, vector, bitmanip, and other removed
  extensions.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:243` still reserves TSFlags for
  vector round mode, VXRM, overlap constraints, VL/mask dependencies, EEW,
  reads-past-VL, and altfmt metadata.
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:423` still carries YSXVec stack
  size/padding paths and scalable-vector callee-save splitting; line 523 leaves
  an unreachable YSXVec stack-probing body in place.
- `YSXInstrInfo.cpp`, `YSXISelDAGToDAG.cpp`, and `YSXISelLowering.cpp` still
  contain extensive `YSXVType`, vector load/store/VSETVLI, FP, vendor, RV32, and
  removed-extension lowering/selection logic. A targeted `rg` over these files
  produced thousands of hits.
- Line-count status is still far above a minimal rv64ima backend:
  `llvm/lib/Target/YuShuXin` has 60,045 lines across 86 files, versus 136,068
  lines across 188 RISCV files.

Directive implementation plan for the next round:

1. Make AC-3 backend source deletion the only mainline objective. Do not spend
   the next round on copied test check-prefix cleanup or target-attribute policy
   nits.
2. Replace `YSXISAInfo::parseArchString` with an exact `rv64ima` parser and
   delete the remaining `YSXVType` forwarding layer. Fix all YSX build failures
   by deleting unsupported users, not by adding new compatibility shims.
3. Remove vector/compressed/VXRM/VSETVLI pass declarations, registrations,
   initialization calls, and target-machine pipeline hooks.
4. Prune TableGen metadata and generated dependencies for removed vector, FP,
   compressed, vendor, RV32, and non-IMA instruction surfaces. Remove vector
   TSFlags, VTYPE/VL operands, VXRM metadata, and overlap metadata from
   `YSXInstrFormats.td` and dependent helpers.
5. Reduce `YSXISelLowering.*`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.*`,
   frame-lowering, register-info, and calling-convention code to scalar RV64
   GPR plus I/M/A/Zmmul/Zaamo/Zalrsc behavior. Delete scalable-vector frame
   paths, FP lowering, vector lowering, vendor paths, RV32 pair handling, and
   false-return subtarget APIs after their callers are gone.
6. Rebuild both YSX-only and RISCV+YSX static configurations, rerun the focused
   LLVM/Clang YSX suites, and add negative tests that fail if removed backend
   pass names, metadata, or instruction surfaces become reachable again.

## Blocking Side Issues

- The retained backend compatibility stubs and copied vector/FP/vendor/RV32/
  compressed code block AC-3 because the plan requires unsupported source
  surfaces to be deleted.
- No new Round 7 frontend hardening blocker was found. Manual probes for
  `target("arch=+v")`, `target("arch=rv64imaf")`, `target("arch=+64bit")`,
  explicit cc1 `-target-feature +v`, and `"f"`/`"vr"` asm constraints now fail
  before unsupported IR/backend emission.

## Queued Side Issues

- `target("cpu=sifive-u74")` and `target("tune=generic-rv64")` still produce
  generic ignored-attribute warnings instead of YSX hard errors. This does not
  leak unsupported features into IR, so it should not displace the next AC-3
  pruning round.
- Stale copied YSX CodeGen check-prefix blocks remain queued until the source
  pruning pass removes the corresponding implementation surfaces.

## Verification Notes

Manual review commands against the existing YSX-only build verified:

- `target("arch=+v")` and `target("arch=rv64imaf")` reject with
  `invalid feature combination: YSX only supports the rv64ima ISA`.
- Explicit cc1 `-target-feature +v` rejects before dumping predefines.
- `target("arch=+64bit")` rejects as an unsupported user arch extension.
- `target("arch=rv64ima")` emits only
  `+64bit,+a,+i,+m,+relax,+zaamo,+zalrsc,+zmmul`.
- `"f"` and `"vr"` inline asm constraints reject, while ordinary `"r"` inline
  asm still accepts.
- The default YSX driver `-###` emits only the expected rv64ima feature set plus
  `+relax` and `-target-abi lp64`.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

I could not rerun the full `llvm-lit` suite in this review sandbox because the
external build test exec root is read-only here; lit failed while trying to
create a temporary file under `build_ysx_only_host_llvm/tools/clang/test`.

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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-8-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-8-summary.md
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
