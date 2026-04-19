# Round 13 Contract

## Mainline Objective

Delete the remaining YSX DAG selector vector/tuple selection residue and disabled
compatibility bodies that are already unreachable for the rv64ima-only backend,
without adding new front-door guards and without touching the RISCV backend.

## Target ACs

- AC-3: materially reduce removed-feature implementation surface inside the YSX
  backend.
- AC-4: preserve and rerun focused YSX-owned regression coverage after the
  deletion slice.

## Blocking Issues

- `YSXISelDAGToDAG.cpp` still contains copied vector subvector/tuple selection,
  `RISCVVType` use, and VMV vector-immediate user handling. These block this
  round because they are part of the selected deletion objective.
- YSX backend files still contain disabled `#if 0` and `if (false)` blocks for
  removed vector, compressed, vendor, and RV32 support. These block this round
  because they are copied implementation bodies rather than deleted code.

## Queued Out Of Scope

- Full deletion of the `YSX_UNSUPPORTED_ISD` compatibility namespace remains
  queued until the corresponding `YSXISelLowering.*` vector/FP/vendor callers
  are removed in a dedicated lowering round.
- BaseInfo/TableGen vector TSFlags, vector operand kinds, vector pseudo tables,
  RegisterInfo vector metadata, Frame scalable-vector support, and
  Disassembler helpers remain queued AC-3 deletion work.
- Stale copied CodeGen check-prefix trimming and `target("cpu=...")` /
  `target("tune=...")` warning policy remain queued and must not replace this
  round's objective.

## Success Criteria

- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp` no longer contains
  `ISD::INSERT_SUBVECTOR`, `ISD::EXTRACT_SUBVECTOR`, `YSXISD::TUPLE_INSERT`,
  `YSXISD::TUPLE_EXTRACT`, `RISCVVType`, or `YSXISD::VMV_V_X_VL` special cases.
- `rg "#if 0|if \\(false" llvm/lib/Target/YuShuXin` has no matches.
- Any unsupported custom-ISD compatibility entries made unused by this round are
  removed instead of retained as inert constants.
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
