# Round 13 Summary

## Work Completed
- Wrote the Round 13 contract with one AC-3 deletion objective: remove the YSX
  DAG selector vector/tuple residue and disabled compatibility bodies.
- Removed `YSXISelDAGToDAG.cpp` vector insert/extract-subvector selection,
  tuple selection, `RISCVVType` use, VMV vector-immediate special cases, and
  unreachable RV32/packed-SIMD selector stubs.
- Deleted disabled compatibility bodies from `YSXExpandPseudoInsts.cpp`,
  `AsmParser/YSXAsmParser.cpp`, `YSXRegisterInfo.cpp`, and
  `YSXISelLowering.cpp`.
- Removed unsupported custom-ISD compatibility entries that became unused in
  this deletion slice.
- Updated the mutable section of `goal-tracker.md` to Plan Version 27.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/round-13-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp`
- `llvm/lib/Target/YuShuXin/YSXExpandPseudoInsts.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXRegisterInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h`

## Validation
- PASS: `rg "#if 0|if \\(false" llvm/lib/Target/YuShuXin` has no matches.
- PASS: `rg "ISD::INSERT_SUBVECTOR|ISD::EXTRACT_SUBVECTOR|YSXISD::TUPLE_INSERT|YSXISD::TUPLE_EXTRACT|RISCVVType|YSXISD::VMV_V_X_VL" llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp` has no matches.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: YSX clang smoke compile produced an ELF64 RISC-V soft-float relocatable.
- PASS: RISCV clang smoke compile with `-march=rv64ima -mabi=lp64` produced an
  ELF64 RISC-V soft-float relocatable in the combined build.
- PASS: `llvm-mc -triple=ysx64 -mattr=+v /dev/null` rejects unsupported vector.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64imaf -c` rejects
  unsupported FP.
- PASS: `llc -mtriple=ysx64-unknown-elf` rejects scalable vector IR without
  crash.
- PASS: `llc -mtriple=ysx64-unknown-elf` rejects direct
  `llvm.riscv.vsetvli` IR without crash.

## Line Counts
- Original RISCV backend: 136,068 lines.
- Current YSX backend: 52,377 lines.
- Current total reduction: 83,691 lines.
- Round 13 net reduction: 839 backend lines.
- Remaining gap to a 30,000-line target: about 22,377 lines.

## Remaining Items
- `YSXSelectionDAGInfo.h` still carries the broader unsupported custom-ISD
  compatibility namespace because many entries still have live
  `YSXISelLowering.*` callers.
- `YSXISelLowering.*` still carries broad `RISCVVType`, YSXVec, vector
  intrinsic, VP/vector lowering, and vector combine code.
- BaseInfo/TableGen/RegisterInfo/Frame/Disassembler/generated-pseudo vector and
  FP metadata remain queued AC-3 deletion work.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned the placeholder response for the
  contract, implementation, build, and summary tasks; no actionable lesson was
  selected or added.
