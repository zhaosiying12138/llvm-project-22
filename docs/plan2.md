# YSX Compile-Time Benchmark and Blog Report

## Goal Description

Create a reproducible C compile-time benchmark suite that compares the minimized
YSX-only LLVM 22.1.3 clang against a same-source RISCV-only LLVM 22.1.3 clang.
The benchmark must use ordinary C programs that compile to rv64ima-only object
files, collect timing and size metrics, and generate a Chinese blog report under
`third_party/ysx_compile_bench/blog.md`.

The purpose is to produce evidence for the blog claim that a YSX-only clang can
complete target compilation work faster than a RISCV-only clang because the YSX
build loads less target code and executes a smaller target backend surface.

## Acceptance Criteria

- AC-1: The benchmark corpus is tracked, redistributable, and rv64ima-only.
  - Positive Tests (expected to PASS):
    - Every selected C benchmark compiles to `.o` with both YSX and RISCV
      target flags.
    - The validation script confirms generated assembly does not contain
      floating-point, vector, compressed, privileged, or other non-rv64ima
      instructions.
    - License/source notes identify the upstream benchmark families.
  - Negative Tests (expected to FAIL):
    - A benchmark that emits FP/vector/compressed/system instructions is
      rejected and excluded from the official result set.
    - A benchmark that depends on libc, OS services, threads, files, or runtime
      input is rejected.
- AC-2: The comparison uses same-version target-only clang builds.
  - Positive Tests (expected to PASS):
    - The script locates the existing YSX-only clang and verifies it registers
      YSX but not RISCV.
    - The script configures or reuses a RISCV-only LLVM 22.1.3 build directory
      with `LLVM_TARGETS_TO_BUILD=RISCV` and verifies it registers RISCV but
      not YSX.
  - Negative Tests (expected to FAIL):
    - The script refuses to run if either compiler is missing or advertises the
      wrong target set.
    - The script refuses to compare compilers with different major LLVM version
      strings.
- AC-3: Measurement is reproducible and records raw plus summarized metrics.
  - Positive Tests (expected to PASS):
    - `third_party/ysx_compile_bench/run_compare.sh --quick` completes quickly.
    - The full script performs warm-cache timing with 30 measured iterations per
      source and records wall/user/sys/RSS/object-size metrics.
    - Results are written as CSV, JSON, and Markdown summary files under
      `third_party/ysx_compile_bench/results/latest`.
  - Negative Tests (expected to FAIL):
    - Invalid iteration counts, missing tools, failed compiles, or failed
      rv64ima checks abort with non-zero status.
- AC-4: The generated Chinese blog report is data-backed and honest about
  limitations.
  - Positive Tests (expected to PASS):
    - `blog.md` includes test corpus description, build configuration, exact
      flags, machine environment, aggregate and per-test result tables, and an
      interpretation of observed speedups.
    - The report explains plausible speedup causes from reduced binary size,
      target registration surface, feature parsing, lowering/selection code,
      and scheduling/instruction tables.
    - The report states that measurements are machine/build-specific and
      warm-cache compile-process timings, not target runtime performance.
  - Negative Tests (expected to FAIL):
    - The report must not claim SPEC CPU usage, runtime speedup, or universal
      performance improvement.
    - The report must not hide failed or excluded benchmarks.
- AC-5: Existing backend work remains intact.
  - Positive Tests (expected to PASS):
    - Focused YSX LLVM/Clang tests still pass.
    - RISCV source and RISCV test directories have zero diff.
    - `git diff --check` passes.
  - Negative Tests (expected to FAIL):
    - Any modification under `llvm/lib/Target/RISCV` or existing RISCV test
      directories blocks completion.

## Path Boundaries

### Upper Bound (Maximum Acceptable Scope)

The implementation includes a tracked open-source benchmark subset, scripts to
build or validate both compilers, warm-cache timing and size collection, raw and
summarized result artifacts, rv64ima-only validation, a Chinese blog report, and
successful RLCR/code-review completion.

### Lower Bound (Minimum Acceptable Scope)

The implementation includes enough tracked C benchmarks to cover scalar integer,
control-flow, and array-loop workloads; a one-command script that compares
YSX-only and RISCV-only clang compile-to-object timing; recorded current results;
and a Chinese `blog.md` grounded in the collected data.

### Allowed Choices

- Can use: open-source benchmark snippets, generated harness-free C kernels,
  Python helper scripts, `/usr/bin/time`, `llvm-objdump`, `llvm-size`,
  same-source out-of-tree CMake builds, and environment-variable overrides for
  compiler/build paths and iteration counts.
- Cannot use: proprietary SPEC CPU source code, runtime benchmark claims,
  benchmarks requiring libc/OS execution, changes to the RISCV backend, or
  unsupported ISA features in the official result corpus.

## Feasibility Hints and Suggestions

- Put all new benchmark artifacts under `third_party/ysx_compile_bench`.
- Keep benchmark programs self-contained and freestanding: no `main` required,
  no includes beyond simple local typedefs/macros, no floating-point types, and
  no external calls.
- Use `clang -S` for instruction validation and `clang -c` for measured object
  generation.
- Use `--target=ysx64-unknown-elf -march=rv64ima -mabi=lp64` and
  `--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64` for target parity.
- Measure one clang process per source per iteration so startup/load cost is
  included.
- Prefer medians for headline speedups and keep raw samples available.

## Dependencies and Sequence

### Milestones

1. Planning and RLCR setup.
   - Phase A: write `docs/draft2.md`.
   - Phase B: generate `docs/plan2.md` and initialize RLCR.
2. Benchmark infrastructure.
   - Phase A: add benchmark sources and license/source notes.
   - Phase B: add scripts for compiler discovery/build, timing, validation,
     summarization, and report generation.
3. Measurement execution.
   - Phase A: create or validate the RISCV-only build.
   - Phase B: run quick and full benchmark modes.
4. Reporting and validation.
   - Phase A: generate CSV/JSON/Markdown summaries and Chinese `blog.md`.
   - Phase B: run focused YSX tests, diff checks, and commit.

## Task Breakdown

Each task must include exactly one routing tag:
- `coding`: implemented directly
- `analyze`: use Codex review/analysis through the RLCR loop

| Task ID | Description | Target AC | Tag (`coding`/`analyze`) | Depends On |
|---------|-------------|-----------|----------------------------|------------|
| task1 | Create draft2/plan2 and start RLCR for the benchmark/report task | AC-1 | coding | - |
| task2 | Add tracked C benchmark corpus and license/source notes | AC-1 | coding | task1 |
| task3 | Implement one-command compare script and Python helpers | AC-2, AC-3 | coding | task2 |
| task4 | Configure or validate the RISCV-only 22.1.3 clang build | AC-2 | coding | task3 |
| task5 | Run quick/full measurements, validate rv64ima-only outputs, and store results | AC-1, AC-3 | coding | task4 |
| task6 | Generate Chinese blog report, run final tests/diff checks, and commit | AC-4, AC-5 | coding | task5 |

## Implementation Notes

- Implementation code and comments must not contain plan-specific terminology
  such as `AC-`, `Milestone`, `Step`, or `Phase`.
- `third_party/ysx_compile_bench/run_compare.sh` is the public one-command
  entry point.
- Default full iterations are 30; `--quick` uses fewer sources and iterations.
- Keep generated build artifacts and temporary objects out of git.
- Record actual CMake cache/tool versions in the generated results.
