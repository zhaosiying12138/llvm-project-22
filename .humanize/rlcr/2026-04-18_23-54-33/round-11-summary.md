# Round 11 Summary

## Work Completed

- Wrote the Round 11 contract with one mainline objective: collapse
  `YSXInstrInfo.cpp` to scalar `rv64ima` instruction-info behavior.
- Removed vector/FP verifier handling for VTYPE, SEW, vector policy, vector
  rounding mode, XSFMM VTYPE, and AVL operands from `YSXInstrInfo.cpp`.
- Removed vector/FP MIR operand comment printing.
- Deleted generated vector/FP pseudo macro families used by commutation,
  opcode-changing, and three-address conversion helpers.
- Deleted all retained `#if 0` compatibility bodies from `YSXInstrInfo.cpp`.
- Removed unused vector-copy statistics/options, vector copy helper declarations,
  vector reassociation helper stubs, FP/SHXADD machine-combiner stubs, and unused
  vector helper declarations/definitions from `YSXInstrInfo.{h,cpp}`.
- Updated the mutable goal tracker to Plan Version 23 with the Round 11
  implementation status and remaining AC-3 gaps.

## Files Changed

- `.humanize/rlcr/2026-04-18_23-54-33/round-11-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-11-summary.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.h`

## Validation

- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` discovered 130 tests.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: YSX-only clang smoke compile for `--target=ysx64-unknown-elf` produced an ELF64 RISC-V soft-float relocatable.
- PASS: Combined RISCV clang smoke compile for `--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64` produced an ELF64 RISC-V soft-float relocatable.
- PASS: `llvm-mc -triple=ysx64 -mattr=+v /dev/null` rejects with `YSX only supports the rv64ima ISA`.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64imaf -c` rejects with `YuShuXin only supports -march=rv64ima`.
- PASS: `clang --target=ysx64-unknown-elf -### -c` emits only retained rv64ima-related positive target features.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: `rg "CASE_YSXVec|CASE_VMA|CASE_VFMA|CASE_WIDEOP|CASE_FP_WIDEOP|#if 0|OPERAND_VTYPE|OPERAND_SEW|OPERAND_VEC|OPERAND_AVL|hasVLOp|hasSEWOp|hasVecPolicyOp" llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp llvm/lib/Target/YuShuXin/YSXInstrInfo.h` returned no matches.

## Line Counts

- Original RISCV backend: 136,068 lines across 188 files.
- Current YSX backend: 57,482 lines across 86 files.
- Current reduction from original RISCV: 78,586 lines, about 57.8%.
- Round 11 YSX backend reduction: 1,639 lines.
- Remaining reduction to reach about 30,000 lines: about 27,482 lines.

## Remaining Items

- `YSXBaseInfo.h` and TableGen still retain vector TSFlag, operand, and pseudo
  metadata.
- `YSXISelLowering.*`, `YSXSelectionDAGInfo.h`, and `YSXISelDAGToDAG.cpp` still
  retain copied vector lowering/selection and direct `RISCVVType` users.
- `YSXFrameLowering.cpp` and related register/frame/disassembler surfaces still
  retain scalable-vector remnants.
- Stale copied CodeGen check-prefix blocks remain queued until source pruning
  stabilizes.
- `code-simplifier` was not found in the local plugin/skill paths checked, so
  no plugin review was run.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for each Round 11
  task, so it was treated as `NONE`.
