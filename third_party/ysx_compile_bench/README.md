# YSX Compile Benchmark

This directory contains a small, reproducible compile-time benchmark for
comparing an LLVM 22.1.3 YSX-only clang with an LLVM 22.1.3 RISCV-only clang.
The tracked corpus currently contains 20 freestanding integer C kernels.

Run:

```bash
third_party/ysx_compile_bench/run_compare.sh
```

For a quick smoke run:

```bash
third_party/ysx_compile_bench/run_compare.sh --quick
```

To run only the negative validation fixtures:

```bash
third_party/ysx_compile_bench/run_compare.sh --self-test
```

The benchmark compiles each C file as a separate clang process to include
process startup and target backend load time.  Generated assembly is checked to
stay within the rv64ima target surface before timing data is accepted.  The
runner also rejects includes, inline assembly, host/runtime dependencies, and
unexpected undefined symbols.

By default the runner looks for sibling build directories named
`build_ysx_only_host_llvm` and `build_riscv_only_22_1_3_host_llvm` next to the
checkout. If either clang is missing, it configures that build with CMake/Ninja,
`LLVM_ENABLE_PROJECTS=clang;lld`, and the matching single target. Override with
`YSX_BUILD_DIR`, `YSX_CLANG`, `RISCV_BUILD_DIR`, `RISCV_CLANG`,
`LLVM_SOURCE_DIR`, `CMAKE_C_COMPILER`, or `CMAKE_CXX_COMPILER` when needed.
