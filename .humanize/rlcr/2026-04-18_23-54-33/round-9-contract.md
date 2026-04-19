# Round 9 Contract

## Mainline Objective

Delete the remaining YSX vector helper forwarding surface and the MC-visible
copied non-IMA tune/profile feature surface so YSX moves closer to an actual
`rv64ima`-only backend instead of a gated copy of RISCV.

## Target Acceptance Criteria

- AC-2: YSX exposes only the `rv64ima` ISA/ABI surface through MC feature
  parsing and assembler options.
- AC-3: YSX source no longer preserves vector VTYPE helper forwarding or
  externally selectable copied vector/vendor tune/profile state.

## Blocking Issues

- Live `YSXVType` callers in lowering, DAG selection, instruction info, asm
  parsing, and instruction printing prevent removal of the forwarding namespace.
- `YSXFeatures.td` and the feature filters still accept copied non-IMA
  tune/profile/vendor feature flags such as `+vxrm-pipeline-flush`,
  `+log-vrgather`, `+single-element-vec-fp64`,
  `+prefer-vsetvli-over-read-vlenb`, and `+andes45`.

## Queued Out Of Scope

- Full deletion of all copied vector/FP/RV32/compressed lowering bodies not
  needed to build after this round's targeted VTYPE and feature-surface edits.
- Clang `target("cpu=...")` and `target("tune=...")` warning policy.
- Stale copied CodeGen check-prefix cleanup for unsupported variants.
- Dead disassembler helper warnings that remain after TableGen-level pruning.

## Success Criteria

- `llvm/include/llvm/TargetParser/YSXISAInfo.h` no longer defines
  `namespace YSXVType`, and remaining buildable YSX code does not depend on a
  YSX-owned forwarding shim for RISCV VTYPE helpers.
- MC rejects enabled non-IMA copied tune/profile/vendor `-mattr` flags with the
  existing `YSX only supports the rv64ima ISA` diagnostic.
- `llvm/test/MC/YSX/unsupported-features.s` contains negative coverage for
  `+vxrm-pipeline-flush`, `+log-vrgather`, `+single-element-vec-fp64`,
  `+prefer-vsetvli-over-read-vlenb`, and `+andes45`.
- YSX-only build validation passes for at least `LLVMYSXCodeGen`, `llvm-mc`,
  and `clang`; the combined RISCV+YSX static build still links the reviewed
  targets.
- `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.
