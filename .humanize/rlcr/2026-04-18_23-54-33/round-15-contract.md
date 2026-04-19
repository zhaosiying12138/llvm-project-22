# Round 15 Contract

## Mainline Objective

Delete the copied unsupported custom-ISD compatibility namespace from
`YSXSelectionDAGInfo.h` and remove the direct YSX lowering declarations and
bodies that only exist to reference those unsupported vector/FP/RV32/vendor
custom ISD nodes.

## Target ACs

- AC-3: remove copied support code for features outside rv64ima.
- AC-4: preserve focused regression and negative coverage after the custom-ISD
  deletion slice.

## Blocking Issues

- `YSXSelectionDAGInfo.h` still defines the `YSX_UNSUPPORTED_ISD` macro and a
  large compatibility namespace for removed custom ISD nodes, including vector,
  FP, RV32, crypto/vendor, tuple, and VLEN nodes.
- `YSXISelLowering.*` still has direct callers and helper declarations that
  generate or combine unsupported custom ISD nodes. Those direct callers must be
  deleted with the namespace rather than converted into new compatibility stubs.

## Queued Out Of Scope

- BaseInfo/TableGen vector TSFlags, operand kinds, generated vector pseudo
  tables, RV32 hardware-mode scaffolding, Subtarget VLEN/YSXVec helper APIs,
  and Disassembler FP/vector helper deletion remain queued unless needed to make
  this custom-ISD slice compile.
- Stale copied CodeGen check-prefix trimming remains queued and must not replace
  this round objective.
- CPU/tune target-attribute policy remains queued.

## Success Criteria

- `YSXSelectionDAGInfo.h` no longer contains `YSX_UNSUPPORTED_ISD` or the
  unsupported compatibility enum block.
- `rg "YSX_UNSUPPORTED_ISD" llvm/lib/Target/YuShuXin` has no matches.
- Unsupported custom ISD names deleted from `YSXSelectionDAGInfo.h` do not remain
  as live `DAG.getNode(YSXISD::...)`, combine switch, or lowering helper callers
  in `YSXISelLowering.*`.
- Retained YSX custom ISD references are only generated rv64ima scalar nodes
  needed by the supported integer, multiply/divide, atomic, branch, call, and
  address-lowering paths.
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
