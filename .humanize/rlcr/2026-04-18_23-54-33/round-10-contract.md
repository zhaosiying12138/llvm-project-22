# Round 10 Contract

## Mainline Objective

Delete the YSX vector register/MC/instruction metadata surface that still keeps
copied VTYPE, VLMUL, VL, SEW, policy, and disabled vector-copy bodies alive
outside the large lowering and DAG-selection files.

## Target Acceptance Criteria

- AC-3: YSX source is pruned toward scalar `rv64ima` by removing copied vector
  metadata helpers and dead vector instruction-info bodies instead of keeping
  them as renamed or disabled compatibility code.
- AC-4: Focused YSX tests continue to pass after the metadata deletion slice.

## Blocking Issues

- `YSXRegisterInfo.h`, `MCTargetDesc/YSXBaseInfo.h`, and `YSXInstrInfo.cpp`
  still contain direct copied vector metadata helpers using `RISCVVType` or
  vector operand/TSFlags concepts.
- `YSXInstrInfo.cpp` still contains disabled vector-copy and vector-combine
  bodies that should be deleted rather than left under `#if 0`.

## Queued Out Of Scope

- Full deletion of the large vector lowering and DAG-selection bodies in
  `YSXISelLowering.*` and `YSXISelDAGToDAG.*`.
- Full `YSXInstrFormats.td` bit-layout collapse if TableGen users require a
  separate larger generated-code migration.
- Vector frame/CFI pruning in `YSXFrameLowering.cpp`.
- Stale YSX CodeGen check-prefix cleanup, Clang CPU/tune diagnostic policy,
  and disassembler warning cleanup.

## Success Criteria

- `YSXRegisterInfo.h`, `MCTargetDesc/YSXBaseInfo.h`, and `YSXInstrInfo.cpp`
  no longer reference `RISCVVType`.
- `YSXInstrInfo.cpp` no longer preserves the reviewed vector-copy or
  vector-reassociation implementation bodies under `#if 0`.
- YSX-only `LLVMYSXCodeGen`, `llvm-mc`, and `clang` rebuild successfully.
- Focused YSX lit and combined RISCV+YSX static build validation pass.
- `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.
