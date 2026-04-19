# Round 11 Contract

## Mainline Objective

Collapse `YSXInstrInfo.cpp` to scalar `rv64ima` instruction-info behavior by
deleting retained vector/FP verifier, comment-printer, pseudo opcode family,
commutation/opcode-change, and disabled compatibility bodies.

## Target Acceptance Criteria

- AC-3: YSX implementation is materially smaller and no longer carries support
  code for removed vector/FP instruction-info behavior.
- AC-4: Existing YSX-owned regression coverage continues to pass after this
  source-pruning slice.

## Blocking Issues

- `YSXInstrInfo.cpp` still handles vector-specific operand kinds in verifier
  paths and then keeps explicit VL/SEW/policy rejection logic as compatibility
  stubs.
- `YSXInstrInfo.cpp` still prints vector operand comments for VTYPE, SEW, and
  vector policy operands.
- `YSXInstrInfo.cpp` still carries generated vector/FP pseudo opcode macro
  families and disabled `#if 0` bodies for unsupported instruction behavior.

## Queued Out Of Scope

- `YSXBaseInfo.h` and TableGen vector TSFlag/operand/pseudo metadata deletion.
- `YSXISelLowering.*`, `YSXSelectionDAGInfo.h`, and `YSXISelDAGToDAG.cpp`
  vector lowering/selection deletion.
- Scalable-vector frame/register/disassembler cleanup.
- Stale copied CodeGen check-prefix trimming.
- YSX `cpu`/`tune` policy hardening beyond current warn-and-ignore behavior.

## Success Criteria

- `YSXInstrInfo.cpp` no longer contains `CASE_YSXVec_*`, `CASE_VMA_*`, or
  `CASE_VFMA_*` pseudo macro families.
- `YSXInstrInfo.cpp` no longer contains VTYPE/SEW/vector-policy comment
  printing or explicit VL/SEW/vector-policy verifier compatibility stubs.
- `YSXInstrInfo.cpp` has no retained `#if 0` bodies for copied unsupported
  vector/FP instruction behavior.
- YSX-only `LLVMYSXCodeGen llvm-mc clang` build passes.
- Focused YSX lit suite passes.
- Combined `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` build passes.
- YSX and RISCV smoke compiles still produce ELF64 RISC-V soft-float objects.
- `git diff --check` passes and `git diff -- llvm/lib/Target/RISCV | wc -l`
  remains `0`.

## BitLesson

- Selector result: `NONE`.
- Note: `bitlesson-selector` returned its placeholder output, so no lesson was
  applied.
