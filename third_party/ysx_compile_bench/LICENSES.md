# Source and License Notes

The benchmark corpus is a small clean-room C corpus organized around two
well-known open benchmark families:

- Embench IoT: https://github.com/embench/embench-iot
  - Upstream project license: GPL-3.0-or-later, with some files carrying their
    own notices.
  - This directory does not copy Embench source files.  The `embench_style`
    kernels are independently written freestanding C programs that cover
    similar embedded integer categories: checksum, matrix multiply, bit mixing,
    modular arithmetic, sorting, and state-machine logic.
- PolyBench/C: https://github.com/ferrandi/PolyBenchC
  - Upstream project license: Ohio State University software distribution
    license, included in summary form in the upstream `LICENSE.txt`.
  - The `polybench_style` kernels are independently written integer variants of
    common PolyBench-style static-control loop kernels.  They intentionally do
    not copy PolyBench source text and avoid floating-point operations so they
    compile to rv64ima-only code.

All original files added in this directory are provided under the same license
as the surrounding LLVM project unless a file states otherwise.

This benchmark is for compile-time comparison only.  It is not a SPEC CPU run,
does not include proprietary SPEC sources, and does not measure generated-code
runtime performance.
