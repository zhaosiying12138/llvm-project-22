# Round 12 Review Result

Mainline Progress Verdict: ADVANCED

Round 12 made real AC-2/AC-4 progress: the two reviewed unsupported vector IR
paths now fail with deterministic YSX diagnostics instead of LLVM aborts. I
verified:

- `llc -mtriple=ysx64-unknown-elf` on
  `llvm/test/CodeGen/YSX/unsupported-scalable-vector-ir.ll` rejects with
  `YuShuXin only supports rv64ima and does not support scalable vector IR`.
- `llc -mtriple=ysx64-unknown-elf` on
  `llvm/test/CodeGen/YSX/unsupported-riscv-vector-intrinsic.ll` rejects with
  `YuShuXin only supports rv64ima and does not support RISC-V target intrinsics
  other than masked atomics`.
- `git diff --check` is clean.
- `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.

The full plan is still incomplete. More importantly, the Round-12 contract was
not satisfied: it explicitly required `YSXSelectionDAGInfo.h` to stop declaring
vector custom ISD nodes and required the targeted `RISCVVType`/YSX vector
lowering and DAG selection paths to be removed. Those surfaces remain.

## Required Finding Classification

### Mainline Gaps

1. `YSXSelectionDAGInfo.h` still preserves the exact unsupported vector custom
   ISD names that the Round-12 success criteria required to be gone. The file
   defines a compatibility `YSX_UNSUPPORTED_ISD` namespace at
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:19`, including
   `READ_VLENB` at `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:79`,
   `VMV_S_X_VL`/`VMV_V_V_VL`/`VMV_V_X_VL`/`VMV_X_S` at
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:213`, and
   `VRGATHER*`/`VSEXT_VL` at
   `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h:221`. Renaming removed nodes
   into unsupported constexpr stubs is not AC-3 deletion.

2. `YSXISelLowering.*` still carries broad copied RVV/VP lowering. The header
   exposes `RISCVVType::VLMUL`, container-vector helpers, YSXVec predicates,
   fixed-length-vector-to-YSXVec lowering declarations, VP lowering hooks, and
   the generated YSXV intrinsic lookup table at
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:343`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:371`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:382`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:547`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:584`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:607`, and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:656`. The implementation still
   includes `llvm/IR/IntrinsicsRISCV.h`, direct `RISCVVType` logic, vector
   memory intrinsic lowering, vector combines, and generated YSXV intrinsic
   implementation at `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:42`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:1378`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:12561`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:12718`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:19987`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:20155`,
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:20643`, and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:23349`.

3. DAG selection still has copied vector selection paths. The old segment-load
   helpers are gone, but `YSXISelDAGToDAG.cpp` still selects vector
   insert-subvector forms through scalable containers and `RISCVVType` at
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:927`, still calls vector
   register-class helpers at `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:977`,
   and still special-cases `YSXISD::VMV_V_X_VL` users in immediate selection at
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:2024` and
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:2057`. The new IR guard makes
   these paths harder to reach from ordinary IR, but AC-3 requires source
   deletion, not unreachable compatibility.

4. BaseInfo and TableGen still encode removed vector/FP metadata. Vector
   TSFlag fields remain in
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:66`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:72`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:80`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:118`, and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:130`; vector operand
   kinds remain at `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:437`
   and `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:459`; vector pseudo
   structs/tables/null shims remain at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:769` and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:846`. Matching
   TableGen vector constraints, FP/vector opcode slots, and TSFlags remain at
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:64`,
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:142`,
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:160`, and
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:208`.

5. Register metadata is still not rv64ima-only. `YSXRegisterInfo.td` keeps
   RV32/RV64 hardware-mode scaffolding at
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:34` and
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:175`, and the common register
   class still has vector register-class TSFlags `IsVRegClass`, `VLMul`, and
   `NF` at `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:184`.

6. Scalable-vector frame lowering is still present. YSX frame code still
   computes YSXVec padding and stack sizes at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:408`, filters scalable-vector
   callee-saves at `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:447`, adjusts
   the stack with scalable offsets and emits VLENB CFA expressions at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:933`, accepts scalable-vector
   frame objects at `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:1216`,
   assigns vector spill slots at `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:1858`,
   and advertises `TargetStackID::ScalableVector` support at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:2135`.

7. Disabled compatibility blocks still exist outside the files named in
   Claude's summary. Examples remain in
   `llvm/lib/Target/YuShuXin/YSXExpandPseudoInsts.cpp:111`,
   `llvm/lib/Target/YuShuXin/YSXExpandPseudoInsts.cpp:205`,
   `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp:3204`, and
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.cpp:538`. This is still copied
   unsupported implementation surface and should be deleted, not carried as
   `#if 0`.

### Blocking Side Issues

No new non-mainline crash blocker was found in this review. The previous
unsupported scalable-vector/direct-`vsetvli` abort is fixed by the new guard.
The blocker now is mainline AC-3 incompleteness: the source still contains
large removed-feature lowering, selector, metadata, frame, and compatibility
surfaces.

### Queued Side Issues

1. Stale copied CodeGen check-prefix trimming remains a valid follow-up after
   the source surface is actually rv64ima-only. It should not displace the next
   deletion round.
2. The existing `target("cpu=...")` and `target("tune=...")` warn-and-ignore
   behavior remains a non-blocking policy issue unless the project decides every
   unsupported YSX target attribute must be a hard error.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 0 | Unjustified deferrals: 5

- AC-1: Maintained. RISCV backend diff remains zero in this review.
- AC-2: Advanced. The reviewed unsupported scalable-vector/direct-RISC-V-vector
  intrinsic IR paths now reject deterministically instead of aborting.
- AC-3: Still incomplete. Round 12 removed some crash-surface code, but the
  contract's custom-ISD/lowering/DAG-selection deletion criteria are unmet, and
  BaseInfo/TableGen/RegisterInfo/Frame/disassembler/generated-pseudo surfaces
  remain.
- AC-4: Advanced for the two new negative `llc` tests, but final YSX-owned
  coverage still depends on finishing the AC-3 deletion and rerunning the
  focused suites.

Tracker audit: I updated the mutable section of `goal-tracker.md` to Plan
Version 26, added the Round-12 review log entry, kept task3/task6 active,
added the incomplete lowering/custom-ISD deletion as a blocking side issue, and
changed the Round-12 completion row to `12 review partial`. I did not modify
the immutable section.

## Required Implementation Plan

Claude must continue with a single AC-3 deletion round. Do not add another
front-door guard as a substitute for pruning.

1. Delete the compatibility custom-ISD surface. Remove the
   `YSX_UNSUPPORTED_ISD` constexpr block from `YSXSelectionDAGInfo.h`; keep
   only generated scalar YSXISD nodes that are actually required by rv64ima
   lowering. Compile, then remove every caller that depended on the deleted
   vector/FP/RV32/vendor nodes.

2. Collapse `YSXISelLowering.*` to scalar rv64ima. Remove `IntrinsicsRISCV.h`
   unless masked atomics still need it, delete `RISCVVType` use,
   `YSXVIntrinsicsTable`, `getLMUL`, vector-container helpers,
   `useYSXVecForFixedLengthVectorVT`, VP/vector lowering hooks,
   vector load/store/gather/scatter/compress/deinterleave/interleave/reduction
   lowering, and vector combine code. Fixed-length vectors may continue only
   through LLVM generic scalarization where that produces scalar rv64ima code;
   do not lower them through YSX vector nodes.

3. Remove vector DAG selection. Delete the `INSERT_SUBVECTOR`,
   `EXTRACT_SUBVECTOR`, tuple, VMV, VRGATHER, vector bitcast, vector immediate,
   and RISCV vector intrinsic selection/combine cases from
   `YSXISelDAGToDAG.cpp`. If a remaining scalar selector references a deleted
   vector helper, remove that branch instead of leaving `llvm_unreachable`
   stubs.

4. Prune BaseInfo/TableGen/RegisterInfo metadata. Remove vector TSFlags,
   YSXV constraints, VLMUL/SEW/VL/policy/VXRM fields, vector operand kinds,
   generated vector pseudo table declarations, null `get*Pseudo` shims, FP and
   vector opcode slots unused by rv64ima, RV32 hardware modes, and vector
   register-class TSFlags. Keep only integer, M, atomic, branch, call,
   relocation, and exact-asm metadata.

5. Delete scalable-vector frame support. Remove YSXVec padding, scalable stack
   offset adjustment, VLENB CFI expression construction, vector callee-save
   filtering, vector spill/reload stack IDs, scalable stack scavenging, and
   `TargetStackID::ScalableVector` support from YSX frame lowering. If a generic
   LLVM API can still ask for scalable vector stack IDs, make the path fail with
   a direct YSX unsupported diagnostic or `llvm_unreachable`.

6. Remove disabled compatibility blocks in the remaining backend files:
   `YSXExpandPseudoInsts.cpp`, `YSXAsmParser.cpp`, `YSXRegisterInfo.cpp`, and
   any other YSX file found by `rg "#if 0|if \\(false" llvm/lib/Target/YuShuXin`.

7. Revalidate after deletion. Required checks: `git diff --check`;
   `git diff -- llvm/lib/Target/RISCV | wc -l` equals `0`; YSX-only
   `LLVMYSXCodeGen llvm-mc clang`; combined
   `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`; focused
   `llvm/test/MC/YSX`, `llvm/test/CodeGen/YSX`, `clang/test/Driver/YSX`, and
   `clang/test/CodeGen/YSX`; smoke compile for YSX and RISCV; negative probes
   for `+v`, `rv64imaf`, scalable vector IR, and direct
   `llvm.riscv.vsetvli`.

## Verification Performed In This Review

- Read `docs/plan.md`, the Round-12 prompt/contract, and `goal-tracker.md`.
- Reviewed the claimed changed YSX source/test files.
- Ran the two new unsupported-vector `llc` probes.
- Ran `git diff --check`.
- Ran `git diff -- llvm/lib/Target/RISCV | wc -l`.
- Did not rerun the full Ninja/lit matrix in this review; the source findings
  already disprove full completion.

REQUIRES MORE WORK
