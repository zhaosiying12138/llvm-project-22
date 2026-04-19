# Round 16 Summary

## Work Completed
- Re-anchored Round 16 to the review blocker and wrote a single-objective
  contract for `YSXISelLowering.*` source-surface pruning.
- Removed the remaining copied FP, vector/VP, scalable-vector, YSXVec,
  generated intrinsic-table, and GlobalISel-only helper declarations/bodies
  from `YSXISelLowering.h` and `YSXISelLowering.cpp`.
- Removed FP/P-SIMD constructor legalization setup and F/vector target-DAG
  combine registration, leaving scalar rv64ima integer/call/address/atomic
  lowering paths.
- Removed vector-call/vscale/scalable-vector handling from YSX formal, call,
  and return lowering, and simplified addressing, misaligned-memory, setcc,
  and calling-convention register-type hooks to scalar rv64ima behavior.
- Current size metric: original RISCV backend is 136,068 lines; current
  YuShuXin backend is 32,083 lines; this is a reduction of 103,985 lines
  (about 76.4%).

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-16-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-16-summary.md`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.h`

## Validation
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen`
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm llvm-mc clang llc`
- PASS: `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
  (132 tests)
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: YSX smoke compile produces an ELF64 RISC-V soft-float relocatable
  object.
- PASS: RISCV `rv64ima` smoke compile still produces an ELF64 RISC-V
  soft-float relocatable object in the combined build.
- PASS: YSX negative probes reject `llvm-mc -triple=ysx64 -mattr=+v`,
  `clang --target=ysx64-unknown-elf -march=rv64imaf`, scalable-vector IR, and
  direct `llvm.riscv.vsetvli`.
- PASS: targeted `YSXISelLowering.*` residue scan has no matches for
  `YSXVIntrinsicsTable`, generated searchable-table includes, VP/vector
  combine markers, VLEN/vscale/scalable-vector hooks, YSXVec markers, vector
  call conventions, FP-immediate hooks, and GlobalISel fallback hooks.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## Remaining Items
- AC-3 remains active for the non-Lowering source surface: BaseInfo/TableGen
  vector/FP/RV32 metadata, generated vector pseudo tables, RegisterInfo
  metadata, Subtarget VLEN/YSXVec helpers, and dead Disassembler FP/vector
  helpers.
- Stale inactive YSX CodeGen check-prefix trimming remains queued until the
  source-surface pruning work is finished.
- CPU/tune target-attribute diagnostic policy remains queued.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: bitlesson-selector returned no applicable lessons for the Round 16
  contract or implementation slice.
