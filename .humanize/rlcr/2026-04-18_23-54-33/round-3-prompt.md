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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-3-contract.md

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
# Round 2 Review Result

Mainline Progress Verdict: ADVANCED

Round 2 advanced the mainline by fixing the reviewed `.option arch,+...` and `.option rvc` front-door gaps, rejecting unsupported `-mattr` features, and removing some explicit compatibility/generator/MCA surfaces. It is not complete. The implementation still does not satisfy the original `docs/plan.md` pruning requirement or the Round-2 contract's "remove remaining unsupported ISA source surfaces" objective.

## Goal Alignment Summary

ACs: 3/4 addressed | Forgotten items: 1 | Unjustified deferrals: 1

- AC-1: Still appears maintained. YSX-only and RISCV+YSX validation was reported, and `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- AC-2: Advanced but not met. Unsupported `.option` and `-mattr` paths improved, but default YSX MC still accepts FP/vector CSR names such as `fflags`, `fcsr`, `frm`, `vtype`, `vl`, `vlenb`, `vxsat`, and `vxrm`.
- AC-3: Not met. Large FP, compressed, vector, bitmanip, crypto, vendor, privileged/profile, GlobalISel, and stale lowering/register/calling-convention surfaces remain in `llvm/lib/Target/YuShuXin`.
- AC-4: Baseline YSX tests exist and were run, but the CSR leakage is not covered by negative MC tests and stale copied CodeGen check blocks remain a cleanup risk after the main pruning work.

The forgotten item at review start was the FP/vector CSR assembler surface and its missing negative tests. The unjustified deferral was treating the residual 66k-line source tree and renamed disabled scaffolding as a queued size concern; this is core AC-3 work, not optional cleanup.

I updated the mutable section of `goal-tracker.md`: reopened task3/task6, added blocking issues for retained unsupported source surfaces and FP/vector CSR acceptance, removed the misleading Round-2 pending-review completions, and moved the stale copied test corpus to queued side issues.

## Mainline Gaps

1. AC-3 is still not implemented: `YSXFeatures.td` retains the removed ISA universe under renamed or still-active records.

   The Round-2 exact string scan is not a valid completion signal. Several reviewed strings were avoided by renaming, while the unsupported feature structure remains. Examples:

   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:77` still defines `FeatureStdExtE`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:126` still defines `FeatureStdExtZicsr`; `:143` still defines `FeatureStdExtZifencei`; many other non-IMA Zic/Zih/Zimop/Zilsd features remain nearby.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:242-285` retains Ztso/Zabha/Zacas/Zalasr/Zawrs surfaces.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:287-382` retains FP/Zfinx-family surfaces, including `YSXDisabledStdExtF`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:384-460` retains compressed extension surfaces, including `YSXDisabledStdExtC`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:638-720` retains vector Zvl/Zve/V surfaces, including `YSXDisabledStdExtV`.

   This violates `docs/plan.md` AC-3 and the Round-2 contract requirement for `YSXFeatures.td` to no longer expose removed feature surfaces.

2. AC-3 is still not implemented: register and calling-convention tables retain FP/vector/RVE surfaces.

   Representative evidence:

   - `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:58-71` defines vector subregister indices.
   - `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:388-498` defines FP registers and FPR classes.
   - `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:503-522` defines FP-in-GPR classes for Zhinx/Zfinx-style support.
   - `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:524+` starts the vector type/register mapping block.
   - `llvm/lib/Target/YuShuXin/YSXCallingConv.td:21-40` retains LP64F/LP64D and vector callee-saved sets.
   - `llvm/lib/Target/YuShuXin/YSXCallingConv.td:51-89` retains FP/vector/RVE interrupt save lists.

   A minimal `rv64ima` backend should only carry the GPR and LP64 surfaces needed by the retained ABI and integer/M/A instruction set.

3. AC-3 is still not implemented: instruction formats, GISel artifacts, and C++ lowering/selection/frame helpers still carry unsupported features.

   Representative evidence:

   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:625-627` still includes `YSXInstrFormatsC.td` and `YSXInstrFormatsV.td`.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:531-599`, `:1388-1548`, and `:2145-2155` retain GlobalISel-only TableGen constructs, contrary to the plan's "no GISel-only functionality" requirement.
   - `llvm/lib/Target/YuShuXin/YSX.h:59-60` still declares the vector peephole pass.
   - `llvm/lib/Target/YuShuXin/YSXMachineFunctionInfo.h:60-65` and `:133-140` retain YSXVec stack state and accessors.
   - `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp` has hundreds of YSXVec references, including vector stack layout, scalable CFA, callee-saved vector handling, and VLENB paths.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp` still has YSXVec reassociation/opcode logic and FP assertions.
   - `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp` still contains extensive `Intrinsic::riscv_*` vector, crypto, bitmanip, FP, masked atomic, and vendor intrinsic handling.

   These are not harmless line-count residue. They are exactly the DAG/lowering/custom ISD/pseudo/scheduling source surfaces that AC-3 says must be reduced to the `rv64ima` subset.

4. AC-2 is still observably wrong: default YSX MC accepts removed FP/vector CSR names.

   Commands run from the YSX-only build:

   ```sh
   printf 'csrr a0, vlenb\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -
   ```

   This exited `0` and printed:

   ```asm
   	csrr	a0, vlenb
   ```

   The same happened for:

   ```asm
   csrr a0, fflags
   csrr a0, fcsr
   csrr a1, frm
   csrr a0, vtype
   csrr a1, vl
   csrr a2, vxsat
   csrr a3, vxrm
   ```

   Source cause:

   - `llvm/lib/Target/YuShuXin/YSXSystemOperands.td:73-75` defines FP CSRs without feature gating.
   - `llvm/lib/Target/YuShuXin/YSXSystemOperands.td:80-86` defines vector CSRs without feature gating.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:881-887` defines CSR instructions with no YSX rv64ima gating.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:1130-1146` exposes `csrr/csrw/csrs/csrc` aliases over `csr_sysreg`.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:2095-2116` still defines FCSR/FRM/FFLAGS/VXRM pseudo sysreg operations.

   Since `rv64ima` does not include FP or vector, these names must be rejected by YSX MC and covered by negative tests.

5. The Round-2 "Remaining Items" are mainline blockers, not deferrable follow-up.

   Claude's summary admits the YSX source tree is still about 66k `.cpp/.h/.td` lines and that "deeper semantic deletion of renamed disabled TD/lowering scaffolding remains a review risk for AC-3." That is the actual AC-3 acceptance criterion. It must be completed now; future-phase deferral is not justified by passing front-door tests.

## Blocking Side Issues

- The FP/vector CSR acceptance blocks AC-2 from being considered safe. It is externally observable through `llvm-mc` without any unsupported `-mattr` or `.option` input.
- The exact blocker-string scan is too narrow and should not be used as a completion gate. It missed renamed records such as `YSXDisabledStdExtF`, `YSXDisabledStdExtC`, `YSXDisabledStdExtV`, and `YSXDisabledVendorFeature...`, as well as broad `FeatureStdExtZ*` and C++ `YSXVec` surfaces.

## Queued Side Issues

- `llvm/lib/TargetParser/YSXISAInfo.h` still aliases RISCV parser concepts, including `RISCVISAInfo`/`RISCVVType`-style names. Keep this queued unless the required source pruning exposes it as a build or API blocker.
- `llvm/test/CodeGen/YSX` still contains large inactive copied check blocks for unsupported RISCV variants such as RV32/ZBB/XTHEAD/RV64IA-TSO. Active RUN lines scanned during review did not appear to invoke unsupported YSX variants outside negative tests, so this should not displace the mainline pruning/CSR work. Revisit after AC-2/AC-3 are fixed.

## Required Implementation Plan

Claude should execute this as a single directive plan in the next round:

1. Replace `YSXFeatures.td` with a minimal `rv64ima` feature surface. Keep only the base 64-bit target machinery, I, M/Zmmul if needed by local structure, A/Zaamo/Zalrsc if needed by local structure, and target tuning features that are demonstrably used by retained rv64ima code. Delete E, FP, C/Zc, V/Zv/Zve/Zvl, bitmanip, crypto, vendor, supervisor/hypervisor/privileged/profile, non-IMA atomics, and renamed disabled compatibility records. When generated getters disappear, remove callers rather than preserving stubs.
2. Prune register, calling-convention, and system operand tables to the retained ABI. Delete FPRs, vector registers/classes/tuple types, FP-in-GPR classes, FP/vector/vendor special registers, LP64F/LP64D/vector/RVE callee-saved sets, and FP/vector interrupt save lists. In `YSXSystemOperands.td`, remove or gate FP/vector CSR names so `fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vxsat`, `vxrm`, and `vlenb` reject for YSX.
3. Remove unsupported instruction TableGen includes and generated tables. `YSXInstrInfo.td` must not include `YSXInstrFormatsC.td` or `YSXInstrFormatsV.td`, and the backend must not retain compressed/vector operands, pseudos, TSFlags, MCA tables, or GISel-only TableGen constructs.
4. Prune the C++ implementation to match the TD surface. Remove vector/FP/vendor/RV32/compressed/bitmanip/crypto lowering and selection from `YSXISelLowering.*`, `YSXISelDAGToDAG.*`, `YSXInstrInfo.*`, `YSXFrameLowering.*`, `YSXMachineFunctionInfo.*`, `YSXRegisterInfo.*`, `YSXAsmParser.cpp`, MC, and disassembler helpers. Keep integer, multiply/divide, and atomic `rv64ima` behavior only.
5. Add YSX-owned negative MC tests for the CSR leakage shown above. The tests must fail for `fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vxsat`, `vxrm`, and `vlenb` on default `-triple=ysx64-unknown-elf`.
6. Rebuild and revalidate both configured builds and the YSX lit subset. In addition to the previous checks, add broader source scans for retained surfaces: `YSXDisabledStdExt`, `YSXDisabledVendorFeature`, `FeatureStdExtZve`, `FeatureStdExtZvl`, `FPR`, `VRRegClass`, `YSXVec`, `GICustom`, `GISel`, `YSXInstrFormatsC`, `YSXInstrFormatsV`, `vlenb`, `fflags`, `fcsr`, and `vxrm`. Any remaining match must be either deleted or explicitly justified as necessary for the retained `rv64ima` backend.

## Validation Notes

- I did not rerun the full build/lit suite because targeted source review and `llvm-mc` checks already expose acceptance-criteria blockers.
- Verified `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- Verified the YSX source tree is still about `66201` `.cpp/.h/.td` lines, supporting the AC-3 concern but not serving as the primary evidence by itself.
- Verified default YSX MC accepts the FP/vector CSR names listed above, which is a direct AC-2 failure.
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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-3-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-3-summary.md
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
