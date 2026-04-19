# Round 7 Contract

## Mainline Objective

Close the remaining YSX Clang frontend feature-surface leaks so unsupported
RISC-V features cannot enter YSX through target attributes, explicit cc1 target
features, or inherited FP/vector inline-asm constraints.

## Target ACs

- AC-2: YSX only accepts the `rv64ima`/`lp64` ISA and ABI surface.
- AC-4: YSX has negative Clang regression coverage for unsupported frontend
  feature paths.

## Blocking Issues

- `YSX64TargetInfo` still inherits RISCV feature hooks that accept unsupported
  target attributes and explicit cc1 `-target-feature` entries.
- The inherited asm-constraint hook can accept FP/vector constraints before YSX
  rejects unsupported code later in the pipeline.

## Queued Out Of Scope

- AC-3 backend source deletion remains queued for a later pruning round:
  vector/FP/vendor/RV32/compressed lowering, selection, pass declarations,
  TSFlags, frame helpers, and compatibility APIs are not the Round 7 objective.
- Stale copied CodeGen check-prefix cleanup remains queued until the backend
  source-pruning pass removes the corresponding implementation surfaces.

## Success Criteria

- `target("arch=+v")` and `target("arch=rv64imaf")` are rejected by Clang for
  `ysx64` before LLVM IR/backend emission.
- `-Xclang -target-feature -Xclang +v` is rejected for `ysx64` and cannot define
  RVV macros.
- Unsupported FP/vector asm constraints such as `"f"` and `"vr"` are rejected
  in Clang syntax/semantic checks for `ysx64`.
- Default and explicit `rv64ima` YSX Clang smoke compiles still produce ELF64
  RISC-V relocatable objects.
- The focused YSX lit suite passes, the combined RISCV+YSX static build still
  links, and `llvm/lib/Target/RISCV` remains unchanged.
