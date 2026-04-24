# RISCV RVV Register Pressure Scheduling Report

## Build

Configured in `build-riscv`:

```sh
cmake -S llvm -B build-riscv -G Ninja \
  -DLLVM_TARGETS_TO_BUILD=RISCV \
  -DLLVM_ENABLE_PROJECTS='clang;lld' \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_CCACHE_BUILD=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DLLVM_USE_LINKER=lld
ninja -C build-riscv llc clang lld FileCheck llvm-mc llvm-objdump count not llvm-config llvm-readobj
```

The benchmark IR was generated as fully unrolled noalias inputs and checked in
as `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll`.

All runs used fixed `VLEN=1024`. The softmax workload also enabled
`+experimental-yushuxin-vfexp`.

## Commands

Baseline:

```sh
build-riscv/bin/llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 INPUT.ll -o base.s
build-riscv/bin/llc -O3 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 INPUT.ll -o base.s
```

Optimized:

```sh
build-riscv/bin/llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 -riscv-v-reg-pressure-aware-sched INPUT.ll -o opt.s
build-riscv/bin/llc -O3 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 -riscv-v-reg-pressure-aware-sched INPUT.ll -o opt.s
```

For softmax, `-mattr` was
`+v,+experimental-yushuxin-vfexp,+zvl1024b`.

Whole-register spill/reload counts were collected with:

```sh
rg -o '\b[vu][sl][1248]r\.v\b' output.s | wc -l
```

Diagnostics were collected by adding `-riscv-v-reg-pressure-report`.

## Results

| Workload | Opt | Baseline whole-reg spills/reloads | Optimized whole-reg spills/reloads | Baseline RVV stack / slots | Optimized RVV stack / slots |
|---|---:|---:|---:|---:|---:|
| 32 x `<128 x float>` add | O2 | 50 | 0 | 768 / 24 | 0 / 0 |
| 32 x `<128 x float>` add | O3 | 50 | 0 | 768 / 24 | 0 / 0 |
| safe-softmax reduce + exp | O2 | 86 | 17 | 816 / 33 | 96 / 3 |
| safe-softmax reduce + exp | O3 | 86 | 17 | 816 / 33 | 96 / 3 |
| top-2 compare/select/reduce | O2 | 93 | 8 | 872 / 34 | 64 / 2 |
| top-2 compare/select/reduce | O3 | 93 | 8 | 872 / 34 | 64 / 2 |
| RMSNorm square/sum/sqrt/normalize | O2 | 268 | 40 | 2208 / 69 | 128 / 4 |
| RMSNorm square/sum/sqrt/normalize | O3 | 268 | 40 | 2208 / 69 | 128 / 4 |

## Snippets

Baseline vector-add shows RVV whole-register spills:

```asm
vfadd.vv v4, v4, v0
csrr a7, vlenb
slli a7, a7, 4
mv t0, a7
slli a7, a7, 2
add a7, a7, t0
add a7, sp, a7
addi a7, a7, 80
vs4r.v v4, (a7)                        # vscale x 32-byte Folded Spill
```

With `-riscv-v-reg-pressure-aware-sched`, vector-add has no
`vs{1,2,4,8}r.v` or `vl{1,2,4,8}r.v` and schedules consumers near loads:

```asm
vle32.v v8, (a0)
vle32.v v12, (t4)
vle32.v v16, (a3)
vle32.v v20, (a6)
vfadd.vv v8, v8, v12
vse32.v v8, (a2)
```

## Notes

The results are trend-based. The reduce-style kernels still retain a small
number of RVV slots because reductions and multi-step normalizations keep some
wide values live across unavoidable dependent work. Unsafe alias cases remain
out of scope and are skipped by the reload pass.
