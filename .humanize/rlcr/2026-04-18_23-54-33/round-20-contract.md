# Round 20 Contract

## Mainline Objective

Delete the residual dead removed-feature scaffolding that Round 19 review found
in the YSX backend, while preserving the existing rv64ima-only visible surface
and RISCV zero-diff requirement.

## Target Acceptance Criteria

- AC-3: YSX implementation is materially smaller than RISCV and contains no
  support code for removed features.
- AC-4: YSX-owned tests and validation remain passing after the pruning.

## Blocking Issues

- Dead VTYPE, Zibi, CLUI, and RVKR operand metadata/validator/parser scaffolding
  remains in YSX source.
- The no-op CSR insertion pass is still declared, registered, built, and added
  to the YSX pipeline even though YSX rejects CSR/Zicsr surfaces.
- Copied SiFive CLIC interrupt frame state and stubs remain even though YSX
  lowering rejects interrupt handlers.

## Queued Out Of Scope

- Goal tracker immutable AC-list drift remains documented and unchanged.
- CPU/tune target-attribute warn-and-ignore diagnostics stay queued unless this
  round discovers feature leakage that blocks rv64ima correctness.

## Success Criteria

- The reviewed residual operand/CSR/SiFive CLIC scaffolding is removed or
  collapsed to rv64ima-only code.
- YSX-only and RISCV+YSX static builds succeed.
- Focused YSX LLVM/Clang lit suites, smoke compiles, unsupported-feature
  negative probes, `git diff --check`, and RISCV zero-diff checks pass.
