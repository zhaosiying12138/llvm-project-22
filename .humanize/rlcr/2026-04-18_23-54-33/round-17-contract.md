# Round 17 Contract

## Mainline Objective

Finish the AC-3 removed-feature source-surface prune by deleting the remaining
FP, vector, scalable-vector, RV32, vendor, generated-table, and compatibility
helper remnants from the core YSX backend files while retaining only the
rv64ima scalar integer, multiply/divide, atomic, call, frame, MC, asm,
disassembly, and object-lowering surface.

## Target ACs

- AC-3: YSX contains no support code for removed FP, vector, RV32, vendor, C,
  non-IMA, or GlobalISel features.
- AC-4: focused YSX regression coverage still validates the retained rv64ima
  surface after the source-surface deletion.

## Blocking Issues

- `YSXISelLowering.*` still contains small vector-specific compatibility
  fragments after Round 16.
- `MCTargetDesc/YSXBaseInfo.*`, `YSXInstrFormats.td`, and
  `YSXInstrInfo.*` still expose vector TSFlags, vector operand kinds, FP/vector
  rounding helpers, vector searchable-table declarations, generated vector
  pseudo table includes, and related constants.
- `YSXRegisterInfo.*`, `YSXSubtarget.h`, `YSXMachineFunctionInfo.h`,
  `YSXAsmPrinter.cpp`, and `Disassembler/YSXDisassembler.cpp` still contain
  copied vector/FP/RV32/vendor metadata, false-query wrappers, vector-call
  state, dummy vector lowering hooks, or FP/vector decode helpers.

## Queued Out Of Scope

- Immutable goal-tracker AC drift remains documented; the immutable section is
  not edited.
- CPU/tune target-attribute diagnostics remain queued unless pruning exposes
  unsupported feature leakage.
- Stale inactive YSX CodeGen check-prefix trimming remains queued until the
  source-surface prune has compiled; if source cleanup completes cleanly in this
  round, trim or regenerate the stale checks before final validation.

## Success Criteria

- The reviewed residual `YSXISelLowering.*` vector fragments are gone:
  `VectorUtils.h`, `hasVInstructions()` vector branches, vector `and-not`
  checks, scalable-vector indirect-argument comments, vector profitability
  escapes, and vector multiply-accumulate reassociation logic.
- The reviewed BaseInfo/TableGen/InstrInfo cluster no longer contains vector
  TSFlag fields, vector operand kinds, FP/vector rounding helper namespaces,
  FP-immediate helpers, `YSXVecBitsPerBlock`, vector segment/load/store
  searchable-table declarations/definitions, or generated vector pseudo table
  includes.
- Register, subtarget, machine-function, asm-printer, and disassembler files no
  longer expose vector register-class helpers, segment spill/reload stubs,
  VLEN/YSXVec helpers, `hasVInstructions*` wrappers, broad unsupported
  FP/vector/vendor getter stubs, vector-call state, `.variant_cc` emission,
  dummy vector MC lowering hooks, or FP/vector disassembler decode helpers.
- No new always-false compatibility wrappers are added for removed features.
- `git diff --check` is clean.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- YSX-only build passes for `LLVMYSXCodeGen llvm-mc clang llc`.
- Combined RISCV+YSX build passes for
  `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- Focused YSX lit suites pass:
  `llvm/test/MC/YSX`, `llvm/test/CodeGen/YSX`,
  `clang/test/Driver/YSX`, and `clang/test/CodeGen/YSX`.
- YSX and RISCV smoke compiles still produce ELF64 RISC-V soft-float objects.
- Negative probes still reject YSX `+v`, `rv64imaf`, scalable vector IR, and
  direct `llvm.riscv.vsetvli`.
