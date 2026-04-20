# YSX Compile-Time Benchmark Blog Draft

## Goal

Add a reproducible C-level compile-time benchmark for the minimized YSX backend
and compare it with a same-version RISCV-only LLVM 22.1.3 clang. The benchmark
must compile ordinary C programs to object files using only the rv64ima ISA
surface, collect timing and size data, and produce a Chinese `blog.md` that can
be used as the basis for a technical blog post.

## Required Outputs

- Create a new tracked benchmark area under `third_party/ysx_compile_bench`.
- Include open-source, redistributable C benchmark programs derived from
  Embench IoT and PolyBench/C style kernels.
- Provide a one-command script that builds or locates the required clang
  binaries, runs the benchmark, checks that generated assembly stays within
  rv64ima, records raw measurements, generates summaries, and refreshes
  `blog.md`.
- Record current benchmark data in tracked result files.
- Explain the test corpus, methodology, measurements, and interpretation in
  Chinese in `third_party/ysx_compile_bench/blog.md`.
- Use Humanize for this second task: generate `docs/plan2.md` from this draft
  and execute it with RLCR.
- Commit the completed work.

## Measurement Requirements

- Compare:
  - YSX-only clang from
    `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang`.
  - A new same-source LLVM 22.1.3 RISCV-only clang built in
    `/home/zhaosiying/codebase/compiler/build_riscv_only_22_1_3_host_llvm`.
- Use Ninja, Clang, Release configuration, `clang;lld`, and
  `LLVM_TARGETS_TO_BUILD=RISCV` for the RISCV-only build.
- Compile each benchmark source as a separate clang process from startup to
  object-file output.
- Use warm-cache timing, one warmup, and 30 measured iterations by default.
- Main compile flags:
  - YSX: `--target=ysx64-unknown-elf -march=rv64ima -mabi=lp64 -O2
    -ffreestanding -fno-builtin -c`
  - RISCV: `--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64 -O2
    -ffreestanding -fno-builtin -c`
- Collect wall time, user time, system time, maximum RSS, output object size,
  clang executable size, and linked shared-library footprint when available.
- Generate CSV/JSON raw data plus Markdown summaries.

## Benchmark Corpus Requirements

- Use open-source C programs that can be committed to this repository.
- Prefer integer/control/array kernels that do not require libc, floating point,
  vector intrinsics, threads, files, or OS services.
- Ensure each source compiles to rv64ima-only assembly for both compilers.
- Exclude any benchmark that introduces floating-point operations, vector
  code, compressed instructions, atomics outside A, system instructions, or
  unsupported target features.
- Include license/source notes in the benchmark directory.

## Report Requirements

`blog.md` must include:

- What was measured and why YSX should plausibly reduce startup/compile time.
- Dataset description and source/license notes.
- Exact compiler versions and build directories.
- Exact flags, repeat count, cache policy, and machine environment.
- Tables for per-test and aggregate results.
- Interpretation of speedup sources:
  - smaller target backend binary/code footprint,
  - less target registration and feature surface,
  - simpler target parsing and driver surface,
  - reduced instruction-selection/lowering/scheduling code,
  - fewer unsupported-extension branches and data tables.
- Limitations:
  - results are machine- and build-specific,
  - warm-cache timing does not perfectly isolate cold process load,
  - elapsed time includes shared frontend/middle-end work,
  - benchmark programs are C compile workloads, not runtime performance tests.

## Validation

- `run_compare.sh --quick` completes.
- Full `run_compare.sh` completes and regenerates results plus `blog.md`.
- Generated assembly/object checks prove rv64ima-only output.
- Existing focused YSX tests still pass.
- RISCV source/test directories remain unmodified.
- `git diff --check` passes.
