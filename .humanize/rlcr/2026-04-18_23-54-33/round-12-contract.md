# Round 12 Contract

## Mainline Objective

Delete the YSX vector lowering/selection crash surface by removing copied
vector custom-ISD, RISC-V vector intrinsic, and DAG selector paths from
`YSXISelLowering.*`, `YSXSelectionDAGInfo.h`, and `YSXISelDAGToDAG.cpp`, then
add negative `llc` coverage for unsupported vector IR inputs.

## Target Acceptance Criteria

- AC-2: Unsupported vector IR must not crash through leftover YSX vector
  lowering or selection machinery.
- AC-3: YSX lowering, custom ISD, and DAG selection move closer to the retained
  scalar `rv64ima` source surface.
- AC-4: YSX-owned tests cover the unsupported vector IR rejection behavior.

## Blocking Issues

- Scalable-vector IR currently aborts in `llc` with a legalization crash.
- Direct `llvm.riscv.vsetvli` IR currently aborts in YSX instruction selection.
- `YSXISelLowering.*`, `YSXSelectionDAGInfo.h`, and `YSXISelDAGToDAG.cpp`
  still retain copied vector lowering, custom ISD nodes, and RISC-V vector
  intrinsic selectors.

## Queued Out Of Scope

- `YSXBaseInfo.h` and `YSXInstrFormats.td` vector TSFlag/operand/table cleanup.
- `YSXRegisterInfo.td` RV32/vector metadata cleanup.
- `YSXFrameLowering.cpp` scalable-vector stack cleanup.
- Disassembler decoder helper cleanup.
- Stale copied CodeGen check-prefix trimming.

These are not final deferrals; they remain mainline AC-3 gaps for subsequent
rounds after the lowering/selection crash surface is removed.

## Success Criteria

- `YSXSelectionDAGInfo.h` no longer declares vector custom ISD nodes such as
  `READ_VLENB`, `VMV_*`, `VRGATHER*`, and `VSEXT_VL`.
- `YSXISelDAGToDAG.cpp` no longer contains live `Intrinsic::riscv_v*`,
  `selectVLSEG`, `selectVSSEG`, `selectVLXSEG`, `selectVSXSEG`, VSETVLI,
  VLE/VSE/VMV/VRGATHER, or `RISCVVType` selection paths.
- `YSXISelLowering.*` removes the direct `RISCVVType` and YSX vector custom-ISD
  lowering declarations targeted by the round.
- New YSX CodeGen tests prove unsupported scalable-vector IR and direct
  `llvm.riscv.vsetvli` IR produce deterministic nonzero diagnostics instead of
  backend aborts.
- YSX-only `LLVMYSXCodeGen llvm-mc clang` build passes.
- Focused YSX lit suite passes.
- Combined `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` build
  passes.
- YSX and RISCV smoke compiles still produce ELF64 RISC-V soft-float objects.
- `git diff --check` passes and `git diff -- llvm/lib/Target/RISCV | wc -l`
  remains `0`.

## BitLesson

- Selector result: `NONE`.
- Note: `bitlesson-selector` returned its placeholder output, so no lesson was
  applied.
