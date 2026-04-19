# Round 16 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 16 advanced the selected `YSXISelLowering.*` cleanup slice: the targeted
markers from the contract are gone from `YSXISelLowering.*` (`YSXVIntrinsicsTable`,
generated searchable-table includes, VP combine names, vscale hooks, vector-call
CC markers, FP-immediate hooks, and GlobalISel fallback hooks), `git diff --check`
is clean, and `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.

The full original `docs/plan.md` goal is still incomplete. Claude's "Remaining
Items" are plan-derived AC-3/AC-4 work, not an acceptable completion state, and
the Round 16 lowering contract itself is still slightly overstated because
vector-specific fragments remain in `YSXISelLowering.*`.

## Mainline Gaps

1. `YSXISelLowering.*` is not yet limited to scalar rv64ima lowering, despite
   the Round 16 success criterion saying it should be. Residual examples:
   `YSXISelLowering.h:66` through `YSXISelLowering.h:78` still describe vectors
   being treated as GPR-sized register parts; `YSXISelLowering.cpp:498` through
   `YSXISelLowering.cpp:510` still has a vector truncate path gated by
   `hasVInstructions()`; `YSXISelLowering.cpp:570` through
   `YSXISelLowering.cpp:576` still routes vector `and-not` to `hasStdExtZvkb()`;
   `YSXISelLowering.cpp:2658` through `YSXISelLowering.cpp:2663` still carries
   scalable-vector indirect-argument handling comments; `YSXISelLowering.cpp:3861`
   through `YSXISelLowering.cpp:3866` still has a vector profitability escape;
   and `YSXISelLowering.cpp:4259` through `YSXISelLowering.cpp:4268` still
   preserves vector multiply-accumulate reassociation logic. These are smaller
   than the Round 15 residue, but they are still copied removed-feature logic.

2. The non-Lowering AC-3 source surface remains substantial and must not be
   deferred as "future phase" cleanup. Examples: `MCTargetDesc/YSXBaseInfo.h:72`
   through `YSXBaseInfo.h:145` still defines vector TSFlag fields; `YSXBaseInfo.h:437`
   through `YSXBaseInfo.h:444` still defines vector operands; `YSXBaseInfo.h:478`
   through `YSXBaseInfo.h:603` still defines FP/vector rounding and FP-immediate
   helpers; `YSXBaseInfo.h:770` through `YSXBaseInfo.h:889` plus
   `MCTargetDesc/YSXBaseInfo.cpp:32` through `YSXBaseInfo.cpp:42` still carry
   generated vector segment/load/store searchable-table declarations and
   implementations; `YSXInstrFormats.td:64` through `YSXInstrFormats.td:121`
   and `YSXInstrFormats.td:209` through `YSXInstrFormats.td:276` still encode
   vector constraint/TSFlag metadata; `YSXInstrInfo.cpp:60` through
   `YSXInstrInfo.cpp:72` and `YSXInstrInfo.h:366` through `YSXInstrInfo.h:388`
   still include generated vector/masked pseudo tables.

3. Register, subtarget, machine-function, asm-printer, and disassembler
   leftovers still violate the "no support code for removed features" part of
   AC-3. Examples: `YSXRegisterInfo.h:24` through `YSXRegisterInfo.h:49` and
   `YSXRegisterInfo.h:145` through `YSXRegisterInfo.h:159` still expose vector
   register-class metadata helpers; `YSXRegisterInfo.cpp:208` through
   `YSXRegisterInfo.cpp:211` keeps a segment spill/reload stub; `YSXSubtarget.h:145`
   through `YSXSubtarget.h:237` still has broad unsupported FP/vector/vendor
   false-query surface, and `YSXSubtarget.h:276` through `YSXSubtarget.h:362`
   still exposes VLEN/YSXVec helpers; `YSXMachineFunctionInfo.h:62` through
   `YSXMachineFunctionInfo.h:63` and `YSXMachineFunctionInfo.h:201` through
   `YSXMachineFunctionInfo.h:202` still track vector-call state; `YSXAsmPrinter.cpp:604`
   through `YSXAsmPrinter.cpp:610` still emits `.variant_cc` for that state and
   `YSXAsmPrinter.cpp:1078` through `YSXAsmPrinter.cpp:1086` still has a vector
   lowering hook; `Disassembler/YSXDisassembler.cpp:412` and
   `YSXDisassembler.cpp:422` still decode FP rounding operands.

4. AC-4 is still not final because stale inactive YSX CodeGen check-prefix
   trimming is explicitly queued. That should not displace AC-3 source pruning,
   but the original plan requires YSX-owned tests to cover only the retained
   rv64ima surface before the loop can complete.

## Blocking Side Issues

None outside the mainline gaps above. The blocker is AC-3 incompleteness:
removed FP/vector/RV32/vendor support is still present as copied source,
generated metadata, false compatibility wrappers, stubs, or stale test surface.

## Queued Side Issues

- Immutable tracker drift remains: the immutable tracker section lists only
  AC-1 and AC-2, while `docs/plan.md` has AC-1 through AC-4. Continue auditing
  against `docs/plan.md`; do not edit the immutable section.
- CPU/tune target-attribute diagnostics still use generic warn-and-ignore
  behavior. This does not block the immediate AC-3 source-pruning objective
  unless a probe shows unsupported feature leakage.
- Stale inactive YSX CodeGen check-prefix blocks should be trimmed after the
  source surface is rv64ima-only, then the focused YSX lit suites must be rerun.

## Required Implementation Plan

1. Make the next implementation objective finish the AC-3 source-surface prune,
   not another deferral. Start by deleting the remaining `YSXISelLowering.*`
   vector fragments identified above: remove the unused `VectorUtils.h` include,
   delete the `hasVInstructions()` vector truncate branch, make `hasAndNot`
   scalar-only, remove vector/scalable-vector comments and compatibility paths
   from calling-convention register hooks and `unpackFromMemLoc`, and remove
   vector profitability/reassociation escapes. Keep scalar integer, call,
   address, atomic, TLS, stack-probe, and target-object lowering only.

2. Remove the vector/FP/generated-table cluster coherently. Delete vector TSFlag
   fields, vector operand kinds, FP/vector rounding helper namespaces, FP
   immediate helpers, `YSXVecBitsPerBlock`, vector segment/load/store pseudo
   structs, vector searchable-table declarations/implementations, and vector
   pseudo table includes from `MCTargetDesc/YSXBaseInfo.h/.cpp`,
   `YSXInstrFormats.td`, `YSXInstrInfo.h/.cpp`, and dependent users. Delete
   `PROBED_STACKALLOC_YSXVec` and any generated references that become unused.

3. Collapse register and subtarget metadata to rv64ima. Remove vector register
   class TSFlags/helpers, segment spill/reload stubs, VLEN/YSXVec helpers,
   `hasVInstructions*` false wrappers, broad unsupported FP/vector/vendor
   getter stubs, and RV32 hardware-mode scaffolding once their callers are
   gone. Retain only the feature queries and ABI/triple helpers needed for
   rv64ima, atomics, calls, frame lowering, and codegen validation.

4. Remove residual vector/FP runtime hooks after generated-code cleanup:
   vector-call state in `YSXMachineFunctionInfo`, `.variant_cc` emission in
   `YSXAsmPrinter`, the dummy vector MC lowering hook, FP/vector disassembler
   decode helpers, and FP/vector asm parser or inst-printer operand helpers that
   no generated instruction still references.

5. After source pruning is complete, regenerate or trim YSX CodeGen checks so
   copied inactive RV32/Zbb/XThead/vector/FP check prefixes no longer describe
   unsupported YSX behavior. Do this after the source surface is clean so test
   churn does not mask real backend residue.

6. Validate in both configured builds: run `git diff --check`, confirm
   `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`, rebuild the YSX-only
   targets, rebuild the combined RISCV+YSX static targets, run the focused YSX
   lit suites, and rerun smoke/negative probes for `+v`, FP features,
   scalable-vector IR, and direct `llvm.riscv.vsetvli`.

## Goal Alignment Summary

ACs: 4/4 addressed, 1/4 fully met | Forgotten items: 0 | Unjustified deferrals: 2

- AC-1: Met and not regressed in this review. RISCV diff is still zero.
- AC-2: Addressed but not final until removed-feature source and generated
  surfaces are gone and final negative probes rerun.
- AC-3: Advanced but incomplete. Round 16 removed many targeted Lowering
  helpers, but residual Lowering fragments and broader copied backend metadata
  remain.
- AC-4: Addressed but not final. Focused YSX tests exist and Round 16 reports a
  passing focused lit run, but stale unsupported check-prefix surface remains
  and final validation must rerun after pruning.

## Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

- Bumped the tracker to Plan Version 34.
- Added a Round 16 review plan-evolution entry.
- Changed the Round 16 verification row to `16 review partial`.
- Kept task3 and task6 active after Round 16 review.
- Added the residual `YSXISelLowering.*` vector fragments as a blocking AC-3
  issue.

REQUIRES MORE WORK
