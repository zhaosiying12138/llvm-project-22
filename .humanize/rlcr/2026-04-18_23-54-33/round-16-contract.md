# Round 16 Contract

## Mainline Objective

Finish the AC-3 `YSXISelLowering.*` source-surface prune by deleting the
remaining copied FP, vector/VP, scalable-vector, YSXVec, and GlobalISel-only
helper declarations and bodies that are not needed by scalar rv64ima lowering.

## Target ACs

- AC-3: remove copied support code for features outside rv64ima.
- AC-4: preserve focused regression and negative coverage after the lowering
  cleanup slice.

## Blocking Issues

- `YSXISelLowering.h` and `YSXISelLowering.cpp` still expose copied FP
  immediate/legalization helper declarations, vector preference hooks,
  vector/VP combine registration, extract-subvector/VLEN hooks,
  GlobalISel-only fallback logic, and generated `YSXVIntrinsicsTable` plumbing.
- These surfaces keep removed-feature implementation code alive even though the
  current target parser and subtarget gates reject the corresponding features.

## Queued Out Of Scope

- BaseInfo/TableGen/RegisterInfo/Subtarget/Disassembler generated-vector and
  metadata pruning remains queued unless a lowering deletion requires a
  dependent cleanup to compile.
- Stale inactive YSX CodeGen check-prefix trimming remains queued and must not
  replace this round objective.
- CPU/tune target-attribute diagnostic policy remains queued.

## Success Criteria

- `YSXISelLowering.*` no longer contains `YSXVIntrinsicsTable`, generated
  searchable-table includes, VP/vector DAG-combine registration, VLEN-specific
  extract-subvector lowering hooks, fixed-vector YSXVec helper hooks, or
  GlobalISel-only fallback code.
- Remaining `YSXISelLowering.*` code is limited to scalar rv64ima integer,
  multiply/divide, atomic, branch, address, TLS, stack-probe, ABI/call, and
  target-object lowering.
- No new always-false compatibility wrappers are added to preserve deleted
  feature surfaces.
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
