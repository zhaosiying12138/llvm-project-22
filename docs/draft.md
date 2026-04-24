# RISCV RVV Register-Pressure-Aware Scheduling Draft

## Intent

Implement a default-off RISCV RVV scheduling experiment for LLVM 22.1.3. The branch starts from `llvmorg-22.1.3`, keeps all build artifacts inside this worktree, and exposes the optimization through hidden `-mllvm` flags only.

## Interfaces

- Add `+experimental-yushuxin-vfexp`.
- Lower vector `llvm.exp.*` to `yushuxin.vfexp` only when the Yushuxin feature is enabled.
- Use a reserved OP-V unary slot modeled after `vfsqrt.v`; this encoding is experimental and non-final.
- Add hidden `-riscv-v-reg-pressure-aware-sched`.
- Add hidden `-riscv-v-reg-pressure-report`.
- Preserve current default behavior when the new flags are not passed.

## Implementation Notes

- Add TableGen instruction and pseudos using the existing RVV unary FP vector structure.
- Treat strict/libcall/globalisel exp paths as out of scope; target SelectionDAG vector `ISD::FEXP` for supported RVV float vectors.
- Add a RISCV-specific pre-RA `GenericScheduler` strategy that detects high vector pressure, biases top-down scheduling, and prioritizes vector consumers/stores over more vector loads when legal.
- Under the new scheduler flag, skip generic load/store clustering that creates long vector-load runs, while preserving existing vector mask mutation behavior.
- Add a conservative pre-scheduler machine pass after the VL optimizer and before generic MachineScheduler. It may clone simple same-block RVV loads before later elementwise users when earlier reduction users create excessive live ranges. It must skip unsafe memory cases, volatile/atomic/ordered memory, unknown side effects, and any intervening may-alias store or call.
- Add frame lowering diagnostics under the report flag: function name, scalable RVV stack size, vector spill-slot count, and fixed stack estimate.

## Tests And Reporting

- Add CodeGen tests for `yushuxin.vfexp` lowering from `llvm.exp.v128f32`.
- Add fully unrolled noalias IR workloads under `llvm/test/CodeGen/RISCV/rvv/`:
  - 32 x `<128 x float>` vector add.
  - `[32,4096]` safe-softmax with reductions and `llvm.exp.v128f32`.
  - Top-2 values with compare/select/reduce.
  - RMSNorm with square/sum/sqrt/normalize.
- Run baseline and optimized at `-O2` and `-O3`, fixed `VLEN=1024`, using `+experimental-yushuxin-vfexp` where exp appears.
- Report commands, whole-register RVV spill/reload counts, stack diagnostics, and before/after snippets in `docs/report.md`.

## Acceptance

- Default code generation is unchanged without the new flags/features.
- The Yushuxin exp instruction is only emitted with `+experimental-yushuxin-vfexp`.
- Scheduler/reload behavior is only enabled by `-riscv-v-reg-pressure-aware-sched`.
- Unsafe reload-rematerialization cases are skipped.
- Each workload should show a substantial trend reduction in RVV spills/reloads; vector add should aim for zero RVV whole-register spills.
