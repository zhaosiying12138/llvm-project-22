# YSX LLVM Backend

This checkout contains the YuShuXin LLVM target, registered as `YSX` and exposed through the `ysx64` triple.  The backend is a standalone `rv64ima/lp64` target derived from LLVM 22.1.3 RISCV, with unsupported RV32, floating-point, compressed, vector, bitmanip, crypto, vendor, and experimental extension paths removed.

The source directory is `llvm/lib/Target/YuShuXin`; the CMake target name is `YSX`.

## Build

Configure a YSX-only host build with Ninja, Clang, and CCache:

```bash
cmake -S llvm -B ../build_ysx_only_host_llvm -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD=YSX \
  -DLLVM_CCACHE_BUILD=ON \
  -DLLVM_ENABLE_ASSERTIONS=OFF
```

Build the normal tools:

```bash
ninja -C ../build_ysx_only_host_llvm \
  clang clang-22 llc llvm-mc llvm-objdump lld llvm-lit FileCheck
```

If `ccache` is not installed, either install it or set `-DLLVM_CCACHE_BUILD=OFF`.

## Lit Tests

Run the focused YSX regression suite:

```bash
../build_ysx_only_host_llvm/bin/llvm-lit -q \
  llvm/test/CodeGen/YSX \
  llvm/test/MC/YSX \
  clang/test/CodeGen/YSX \
  clang/test/Driver/YSX
```

The suite covers YSX CodeGen, MC assembly/disassembly, object attributes, Clang target options, negative unsupported-feature diagnostics, and assembly/object round trips.

## Compile-Time Benchmark

The benchmark corpus and runner are in `third_party/ysx_compile_bench`.

Run the negative validation fixtures only:

```bash
third_party/ysx_compile_bench/run_compare.sh --self-test
```

Run a quick smoke benchmark:

```bash
third_party/ysx_compile_bench/run_compare.sh --quick
```

Run the full benchmark:

```bash
third_party/ysx_compile_bench/run_compare.sh
```

By default the script looks for sibling build directories named `build_ysx_only_host_llvm` and `build_riscv_only_22_1_3_host_llvm` next to this checkout.  Override paths when needed:

```bash
YSX_CLANG=/path/to/ysx/bin/clang \
RISCV_CLANG=/path/to/riscv/bin/clang \
YSX_LLVM_OBJDUMP=/path/to/ysx/bin/llvm-objdump \
RISCV_LLVM_OBJDUMP=/path/to/riscv/bin/llvm-objdump \
third_party/ysx_compile_bench/run_compare.sh --quick
```

You can also override build directories and host compilers:

```bash
YSX_BUILD_DIR=/path/to/build_ysx \
RISCV_BUILD_DIR=/path/to/build_riscv \
CMAKE_C_COMPILER=/path/to/clang \
CMAKE_CXX_COMPILER=/path/to/clang++ \
third_party/ysx_compile_bench/run_compare.sh
```

The runner validates that generated assembly and object disassembly stay within the `rv64ima` instruction surface before accepting timing data.

## Tool Usage

Compile C to an object with Clang:

```bash
../build_ysx_only_host_llvm/bin/clang \
  --target=ysx64-unknown-elf \
  -march=rv64ima \
  -mabi=lp64 \
  -O2 \
  -ffreestanding \
  -fno-builtin \
  -c input.c \
  -o input.o
```

Compile LLVM IR to assembly with `llc`:

```bash
../build_ysx_only_host_llvm/bin/llc \
  -mtriple=ysx64-unknown-elf \
  -mattr=+m,+a \
  input.ll \
  -o input.s
```

Assemble with `llvm-mc`:

```bash
../build_ysx_only_host_llvm/bin/llvm-mc \
  -triple=ysx64-unknown-elf \
  -mattr=+m,+a \
  -filetype=obj \
  input.s \
  -o input.o
```

Disassemble with `llvm-objdump`:

```bash
../build_ysx_only_host_llvm/bin/llvm-objdump \
  -d \
  --triple=ysx64 \
  input.o
```

## Supported Target Surface

YSX intentionally supports only:

```text
triple: ysx64-unknown-elf
arch:   rv64ima
abi:    lp64
cpu:    generic-rv64
```

The backend rejects RV32, floating-point ABIs, compressed instructions, vector IR/intrinsics, and unsupported RISC-V extension feature strings.  The object attribute emitted for the retained ISA is the canonical versioned form:

```text
rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0
```

## Report

See `docs/blog-llvm.md` for the line-count reduction, correctness validation record, compile-time benchmark table, and binary-size comparison.
