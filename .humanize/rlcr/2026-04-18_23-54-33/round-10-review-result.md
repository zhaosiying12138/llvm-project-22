# Round 10 Review Result

Mainline Progress Verdict: ADVANCED

Round 10 made real but narrow AC-3 progress: the named files no longer contain
direct `RISCVVType` references, the `YSXInstrInfo.cpp` reassociation bodies
were reduced, and public MC probes still reject vector instructions/features.
The original plan is not complete. Claude explicitly left large lowering,
DAG-selection, frame, TableGen, generated pseudo, and compatibility-helper
surfaces for later; those are core `docs/plan.md` task3 requirements, not
acceptable deferrals.

## Required Finding Classification

### Mainline Gaps

1. The Round-10 objective says the YSX vector register/MC/instruction metadata
   surface should be deleted, but `YSXBaseInfo.h` still defines the vector
   metadata surface. The file retains VLMUL/SEW/VL/vector-policy/VXRM/overlap
   TSFlag fields and helpers at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:72`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:80`,
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:118`, and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:177`. It also retains
   vector operand kinds such as `OPERAND_VTYPEI10`, `OPERAND_VEC_POLICY`,
   `OPERAND_SEW`, `OPERAND_VEC_RM`, `OPERAND_AVL`, and `OPERAND_VMASK` at
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:419` and
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:437`. Replacing
   `RISCVVType::VLMUL` with raw `unsigned` did not delete the metadata model.

2. `YSXInstrInfo.cpp` still has live vector/FP pseudo metadata code outside the
   large lowering and DAG-selection files. The machine verifier still handles
   VTYPE, vector policy, SEW, VXRM, XSFMM VTYPE, and AVL operands at
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2128`,
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2186`,
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2195`, and
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2252`. It then keeps explicit
   VL/SEW/policy rejection logic at
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2268`, which is a compatibility
   stub, not source pruning. The comment printer still has VTYPE/SEW/policy
   cases at `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2859`.

3. `YSXInstrInfo.cpp` still carries generated vector/FP pseudo opcode families.
   The `CASE_YSXVec_*`, `CASE_VMA_*`, and `CASE_VFMA_*` macro families start at
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2889` and continue into commutation
   and opcode-changing logic. Several bodies are still behind `#if 0`, for
   example `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:2984`,
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:3215`, and
   `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:3628`. Round 10 removed one
   slice, but unsupported vector/FP machine-instruction logic is still retained.

4. Direct copied vector helper use remains in lowering and selection, so AC-3 is
   still materially incomplete. `YSXISelLowering.h` exposes `RISCVVType::VLMUL`
   and vector helper declarations at
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:343` and
   `llvm/lib/Target/YuShuXin/YSXISelLowering.h:371`. The implementation keeps
   `getLMUL` and vector subregister stubs at
   `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:2640`. `rg "RISCVVType"` still
   finds 99 matches across `YSXISelLowering.*` and `YSXISelDAGToDAG.cpp`.

5. DAG selection still selects copied RISCV vector intrinsics and generated
   pseudos. Representative live code includes vector segment load/store
   selection at `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:285`,
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:344`,
   `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:392`, and a disabled VSETVLI
   body at `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:463`. The intrinsic
   switch still routes `Intrinsic::riscv_v*` cases through YSX selection paths
   later in that file.

6. Frame lowering still carries scalable-vector stack/CFI support. The YSXVec
   stack-probe body is left under `#if 0` at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:526`, and live scalable-vector
   CFA expression helpers remain at
   `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:573`. These are plan-derived
   AC-3 pruning tasks.

7. TableGen/generator surfaces remain. `YSXInstrFormats.td` still defines
   vector constraints, vector opcodes, SEW/VL/policy TSFlags, VXRM, EEW, and
   VTYPE metadata; examples include
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:64`,
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:161`, and
   `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:218`. `YSXRegisterInfo.td`
   still carries RV32 modes and `IsVRegClass` metadata at
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:34` and
   `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:184`. Generated pseudo lookup
   declarations remain in `YSXBaseInfo.h` around
   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:773`.

### Blocking Side Issues

None outside the mainline gaps above. The current blockers are not side issues;
they are the remaining AC-3 source-pruning work from the original plan.

### Queued Side Issues

1. Stale copied YSX CodeGen check-prefix blocks should remain queued until the
   source pruning stabilizes.
2. The `target("cpu=...")` and `target("tune=...")` warn-and-ignore policy is
   still a non-blocking policy issue unless the user decides YSX should hard
   error for every unsupported target attribute.
3. Dead disassembler helper warnings should be cleaned with the TableGen decoder
   cleanup, but they should not displace the next AC-3 deletion slice.

## Goal Alignment Summary

ACs: 4/4 addressed, 2/4 fully met | Forgotten items: 0 | Unjustified deferrals: 4

- AC-1: Maintained. YSX remains standalone in the reviewed source state, and
  `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- AC-2: Maintained for the reviewed public probes. Existing build products show
  `llvm-mc -mattr=help` exposes only rv64ima-related features, `vsetvli` is
  rejected as an unrecognized mnemonic, and `llc -mattr=+v` aborts with
  `YSX only supports the rv64ima ISA`.
- AC-3: Still incomplete. Round 10 advanced the source deletion incrementally,
  but the retained vector/FP/RV32/compressed metadata, lowering, selection,
  frame, and generated pseudo surfaces violate the original pruning criterion.
- AC-4: Partial. YSX-owned tests exist and Claude's focused-lit claim is
  plausible, but final coverage cannot be accepted until the retained source
  surface is actually removed and tests are regenerated or trimmed to match the
  final rv64ima-only backend.

Tracker audit: I updated the mutable section of `goal-tracker.md` to Plan
Version 22, added a Round-10 review log entry, kept task3/task6 active, and
marked the Round-10 slice as only partially verified. I did not modify the
immutable section.

## Implementation Plan For Required Follow-Up

Claude must treat the next round as one AC-3 mainline deletion round, not as a
queued cleanup round.

1. Delete the vector/FP/RV32/compressed generated metadata first. Remove vector
   TSFlags, vector operand kinds, VLMUL/SEW/VL/policy/VXRM helpers, vector
   pseudo table declarations, and vector constants from `YSXBaseInfo.h` and the
   TableGen definitions that generate them. Keep only operand types needed by
   rv64ima integer, multiply/divide, atomic, branch, call, relocation, and exact
   asm paths.

2. Collapse `YSXInstrInfo.cpp` to scalar rv64ima behavior. Remove vector/FP
   copy paths, pseudo opcode macro families, commutation/opcode-change cases,
   VL/SEW/policy verifier branches, VTYPE/SEW comment printing, and remaining
   `#if 0` unsupported feature bodies. If a generated opcode no longer exists,
   delete the C++ case instead of replacing it with a false-return stub.

3. Remove vector lowering declarations and custom ISD nodes from
   `YSXISelLowering.h`, `YSXISelLowering.cpp`, and `YSXSelectionDAGInfo.h`.
   Delete `RISCVVType` use, vector tuple handling, vector intrinsic lowering,
   fixed/scalable vector legalization, vector combine code, and unsupported
   FP/vector rounding bodies. Leave generic LLVM vector IR to scalarize or
   reject through normal non-vector target paths.

4. Remove vector DAG-selection code from `YSXISelDAGToDAG.cpp`. Delete
   `IntrinsicsRISCV.h` vector dependence, `selectVSETVLI`, segment load/store
   selection, VLE/VSE/VMV/VRGATHER/VF/XRemovedSfmm cases, and all `#if 0`
   vector selection blocks. After this step, `rg "RISCVVType|riscv_v|PseudoV|VSET|VLSEG|VSSEG|VLE|VSE|VMV|VRGATHER"` under YSX should have no live backend
   lowering/selection hits.

5. Remove scalable-vector frame/register remnants. Delete YSXVec stack-size,
   VLENB, scalable offset, vector register class, and vector spill/reload paths
   from frame, register, machine-function-info, expand-pseudo, and disassembler
   code. Keep fatal diagnostics only where generic LLVM APIs can still hand the
   backend a scalable stack offset; do not preserve copied vector machinery to
   implement that diagnostic.

6. Rebuild after each deletion cluster in both configured builds. Run
   `git diff --check`, `git diff -- llvm/lib/Target/RISCV | wc -l`, YSX-only
   `LLVMYSXCodeGen llvm-mc clang`, combined `LLVMYSXCodeGen LLVMRISCVCodeGen
   llvm-mc llc clang lld`, focused YSX lit, YSX clang smoke compile, combined
   RISCV smoke compile, and negative MC/Clang probes for RV32, F/D, C, V,
   vendor, and direct `-mattr` feature enabling.

## Verification Performed In This Review

- `git diff --check`: clean.
- `git diff -- llvm/lib/Target/RISCV | wc -l`: `0`.
- `rg "RISCVVType"` in `YSXRegisterInfo.h`, `YSXBaseInfo.h`, and
  `YSXInstrInfo.cpp`: no matches.
- `rg "RISCVVType"` in `YSXISelLowering.*` and `YSXISelDAGToDAG.cpp`: 99
  remaining matches.
- Existing `llvm-mc -triple=ysx64 -mattr=help`: only 64bit, I, M, A, Zmmul,
  Zaamo, Zalrsc, relax, and exact-asm are listed.
- Existing `llvm-mc` accepts a scalar `add` object smoke and rejects `vsetvli`.
- Existing `llc -mtriple=ysx64-unknown-elf -mattr=+v` reports
  `YSX only supports the rv64ima ISA`.

I did not rerun full Ninja or lit in this review because the configured build
directories live outside the writable workspace; the review uses source
inspection and lightweight read-only build-product probes instead.

REQUIRES MORE WORK
