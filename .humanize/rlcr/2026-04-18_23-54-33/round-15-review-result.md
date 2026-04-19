# Round 15 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 15 advanced the selected custom-ISD/lowering objective: `YSX_UNSUPPORTED_ISD`
is gone from `YSXSelectionDAGInfo.h`, and the remaining direct `YSXISD::`
lowering/selection references I found are scalar rv64ima nodes for calls,
branches, addresses, atomics, integer legalization, and stack probing. The full
original `docs/plan.md` goal is still incomplete. Claude's own "Remaining Items"
section is a plan-derived deferral, not an acceptable completion state.

## Mainline Gaps

1. `YSXISelLowering.*` is not collapsed to an rv64ima-only lowering surface.
   The summary overstates the prune. The file still carries copied FP,
   vector/VP, scalable-vector, YSXVec, and GlobalISel-only helper surfaces:
   `YSXISelLowering.h:64` and `YSXISelLowering.cpp:319` keep FP immediate and
   FP legalization machinery; `YSXISelLowering.h:67` and
   `YSXISelLowering.cpp:1138` keep extract-subvector/VLEN lowering logic;
   `YSXISelLowering.cpp:700` still registers VP/vector DAG combines behind
   `hasVInstructions()`; `YSXISelLowering.h:520` and
   `YSXISelLowering.cpp:4942` keep generated `YSXVIntrinsicsTable` plumbing;
   `YSXISelLowering.cpp:4818` keeps the GlobalISel fallback hook and RISCV
   vector tuple handling. These are AC-3 source-surface gaps even if most paths
   are currently feature-gated false.

2. BaseInfo/TableGen/RegisterInfo/Subtarget/Disassembler cleanup remains
   mainline AC-3 work, not future-phase cleanup. Examples:
   `MCTargetDesc/YSXBaseInfo.h:72` through `YSXBaseInfo.h:142` still reserve
   vector TSFlag fields; `YSXBaseInfo.h:437` through `YSXBaseInfo.h:444` still
   define vector operand kinds; `YSXBaseInfo.h:479` and
   `YSXBaseInfo.h:535` still define FP/vector rounding namespaces;
   `YSXBaseInfo.h:770` and `YSXBaseInfo.h:846` keep YSXVec constants and
   generated vector segment table declarations; `YSXInstrFormats.td:209`
   through `YSXInstrFormats.td:282` still exposes vector/FP TSFlags;
   `YSXInstrInfo.td:1308` still defines `PROBED_STACKALLOC_YSXVec`;
   `YSXSubtarget.h:145` through `YSXSubtarget.h:238` still exposes a large
   unsupported feature getter surface, and `YSXSubtarget.h:276` through
   `YSXSubtarget.h:362` still exposes VLEN/YSXVec APIs; the FP decoder helpers
   remain in `Disassembler/YSXDisassembler.cpp:409` and
   `YSXDisassembler.cpp:419`.

3. AC-4 is not final because copied YSX CodeGen checks still have stale inactive
   unsupported-prefix surfaces. This should not displace the next AC-3 source
   prune, but it is not a valid final deferral under the original plan's
   requirement that YSX-owned tests cover only the retained rv64ima surface.

## Blocking Side Issues

None outside the mainline gaps above. The blocker is still AC-3 incompleteness:
removed feature code remains renamed or feature-gated instead of deleted.

## Queued Side Issues

- Immutable tracker drift remains: the immutable section lists only AC-1 and
  AC-2, while `docs/plan.md` has AC-1 through AC-4. Continue auditing against
  `docs/plan.md`; do not edit the immutable section.
- CPU/tune target-attribute diagnostics still use warn-and-ignore behavior.
  This is not blocking the AC-3 source-pruning objective because probes have not
  shown unsupported feature leakage.
- Stale inactive YSX CodeGen check-prefix blocks should be trimmed after the
  source surface is rv64ima-only.

## Required Implementation Plan

1. Make the next round's single mainline objective: finish the AC-3 source
   prune, starting with the remaining `YSXISelLowering.*` FP/vector/VP/GISel
   surfaces. Do not spend the next round on queued test-prefix cleanup or
   CPU/tune policy unless a build break makes it necessary.

2. In `YSXISelLowering.h` and `YSXISelLowering.cpp`, delete the remaining FP
   legalization helper declarations/bodies, vector preference hooks, VP/vector
   DAG-combine registration, `isExtractSubvectorCheap`,
   `isVScaleKnownToBeAPowerOfTwo`, `getOptimalMemOpType`, `fallBackToDAGISel`,
   `YSXVIntrinsicsTable`, and generated searchable-table includes. Keep only
   scalar rv64ima integer, call, branch, address, atomics, TLS, stack-probe, and
   ABI lowering. Rebuild and delete or simplify callers rather than adding new
   always-false compatibility wrappers.

3. Prune `YSXSubtarget.h/.cpp` after the lowering cleanup: remove VLEN/YSXVec
   helpers, generated-vector helper APIs, and unsupported feature getter stubs
   that no remaining rv64ima code needs. Keep the retained `rv64ima` feature
   queries and target ABI/triple helpers only.

4. Prune TableGen/BaseInfo in one coherent cluster: remove vector TSFlag fields,
   vector operand kinds, FP/vector rounding helper namespaces, YSXVec constants,
   generated vector segment/load/store searchable-table declarations, and
   `PROBED_STACKALLOC_YSXVec`. Update `YSXInstrFormats.td`,
   `MCTargetDesc/YSXBaseInfo.h`, `YSXInstrInfo.td`, and dependent C++ users
   together so generated headers stay consistent.

5. Convert register metadata to rv64-only where feasible: remove leftover RV32
   hardware-mode scaffolding and dead RV32 patterns from `YSXRegisterInfo.td`
   and `YSXInstrInfo.td` after confirming generated code no longer requires
   them.

6. Remove dead FP/vector disassembler helpers once the generated decoder tables
   no longer reference them.

7. After each coherent cluster, run `git diff --check`, confirm
   `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`, rebuild the YSX-only and
   combined targets, run the focused YSX lit suites, and rerun the smoke and
   negative probes for `+v`, FP features, scalable-vector IR, and direct
   `llvm.riscv.vsetvli`.

## Goal Alignment Summary

ACs: 4/4 addressed, 1/4 fully met | Forgotten items: 0 | Unjustified deferrals: 2

- AC-1: Met and not regressed in this review. RISCV diff is still zero, and the
  existing YSX smoke compile produced an ELF64 RISC-V soft-float object.
- AC-2: Addressed but not final until source pruning completes. Reviewed probes
  still reject `+v`, `+f`, and scalable-vector IR deterministically.
- AC-3: Advanced but incomplete. The custom-ISD slice is improved, but the
  remaining lowering, BaseInfo/TableGen/RegisterInfo/Subtarget/Disassembler
  surfaces still violate the "no support code for removed features" criterion.
- AC-4: Addressed but not final. Focused tests exist and the Round 15 summary
  reports a passing lit suite, but stale copied check-prefix surfaces remain and
  final validation must rerun after the remaining source prune.

## Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

- Bumped the tracker to Plan Version 32.
- Added a Round 15 review plan-evolution entry.
- Changed the Round 15 verification row to `15 review partial`.
- Added the residual `YSXISelLowering.*` FP/vector/VP/GISel source surface as a
  blocking AC-3 issue.
- Moved the disassembler helper cleanup out of queued side issues because it is
  part of the remaining AC-3 source prune.

REQUIRES MORE WORK
