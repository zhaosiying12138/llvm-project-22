# Round 21 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 21 advanced the mainline by closing the reviewed `.insn` removed-opcode
acceptance path. Removed named FP/vector/custom opcode forms now reject,
removed numeric major opcode values now reject, `.insn r4` now rejects, and
retained rv64ima `.insn` forms still assemble. However, the original
`docs/plan.md` goal is still not complete: YSX still carries dead selector
scaffolding for removed source paths.

## Goal Alignment Summary

ACs: 4/4 addressed, 3/4 met | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MET. YSX remains standalone and RISCV source diff is still zero.
- AC-2: MET for the reviewed surface. The `.insn` opcode leak is fixed, and
  quick probes still reject unsupported vector, FP, `.option arch`, CSR, and
  unsupported IR paths.
- AC-3: PARTIAL. The Round 21 `.insn` format/opcode scaffolding is gone, but
  copied dead selector scaffolding for removed Zba-style SHXADD/addressing,
  SiFive, and vector-combine paths remains.
- AC-4: MET for current coverage. The new MC negative coverage exists and the
  focused unsupported-feature test passes; full revalidation is still needed
  after the remaining AC-3 deletion.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 44 records this Round 21 review, keeps task3/task6 active, replaces
the resolved `.insn` blocker with the selector-scaffolding blocker, and marks
Round 21 as review-partial. The immutable section was not modified.

## Mainline Gaps

1. AC-3 remains incomplete: YSX still contains dead selector scaffolding for
   removed Zba-style SHXADD/scaled-address, SiFive, and vector-combine paths.

   `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:438` still defines
   `AddrRegRegScale`, and `YSXInstrInfo.td:440` still defines
   `AddrRegZextRegScale`, even though the retained rv64ima instruction set has
   no scaled register-register addressing form using these complex patterns.
   The corresponding selector declarations remain in
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h:55` through
   `YSXISelDAGToDAG.h:71`, and the implementations remain in
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:1248` through
   `YSXISelDAGToDAG.cpp:1342`.

   The same file also retains Zba-style SHXADD pattern helpers:
   `YSXISelDAGToDAG.h:110` through `YSXISelDAGToDAG.h:118` declare
   `selectSHXADDOp` and `selectSHXADD_UWOp`, while
   `YSXISelDAGToDAG.cpp:1554` through `YSXISelDAGToDAG.cpp:1710` implement
   them with comments explicitly describing SHXADD/SHXADD_UW folding. These
   helpers are not part of rv64ima and should not remain as copied
   removed-feature source scaffolding.

   There is also a no-op copied SiFive selector hook at
   `YSXISelDAGToDAG.h:133` and `YSXISelDAGToDAG.cpp:232`
   (`selectSF_VC_X_SE`), plus a stale vector-combine declaration at
   `YSXISelDAGToDAG.h:163` (`performCombineVMergeAndVOps`). These are not
   user-visible acceptance leaks, but they directly contradict AC-3's
   requirement that removed-feature support code be pruned.

## Blocking Side Issues

None separate from the mainline gap above. The remaining issue is plan-derived
AC-3 source pruning work, not an unrelated side issue.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. Current
   probes still do not show unsupported feature leakage through those warnings.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. Delete the unused TableGen complex-pattern classes `AddrRegRegScale` and
   `AddrRegZextRegScale` from `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`.
2. Delete the corresponding declarations and template wrappers from
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h`.
3. Delete `YSXDAGToDAGISel::SelectAddrRegRegScale` and
   `YSXDAGToDAGISel::SelectAddrRegZextRegScale` from
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`.
4. Delete `selectSHXADDOp`, `selectSHXADD_UWOp`, `selectSF_VC_X_SE`, and the
   stale `performCombineVMergeAndVOps` declaration/definition surface from
   `YSXISelDAGToDAG.h` and `YSXISelDAGToDAG.cpp`.
5. Run `rg "AddrRegRegScale|AddrRegZextRegScale|selectSHXADD|SHXADD|selectSF_VC_X_SE|performCombineVMergeAndVOps" llvm/lib/Target/YuShuXin`
   and require no matches outside any intentionally retained negative test
   text. Do not replace these helpers with stubs.
6. Rebuild both configured target sets, rerun the focused YSX lit suites, run
   the retained rv64ima smoke tests and unsupported-feature probes, then run
   `git diff --check` and `git diff -- llvm/lib/Target/RISCV | wc -l`.

REQUIRES MORE WORK
