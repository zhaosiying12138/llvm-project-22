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

## Workload Extraction

The checked-in workload test contains all four functions. For measurements, the
following exact splitter was used so each table row counts one function only:

```sh
python3 - <<'PY'
from pathlib import Path
src = Path('llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll').read_text().splitlines()
outdir = Path('/tmp/rvv-pressure-report')
outdir.mkdir(exist_ok=True)
decls = [l for l in src if l.startswith('declare ')]
for name in ['vector_add_32', 'safe_softmax_32', 'top2_32', 'rmsnorm_32']:
    body = []
    capture = False
    depth = 0
    for line in src:
        if line.startswith(f'define void @{name}'):
            capture = True
        if capture:
            body.append(line)
            depth += line.count('{') - line.count('}')
            if depth == 0 and line.strip() == '}':
                break
    (outdir / f'{name}.ll').write_text('\n'.join(decls) + '\n\n' + '\n'.join(body) + '\n')
PY
```

## Commands

Baseline and optimized assembly were produced with these exact command forms:

```sh
build-riscv/bin/llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 \
  /tmp/rvv-pressure-report/vector_add_32.ll \
  -o /tmp/rvv-pressure-report/vector_add_32.O2.base.s
build-riscv/bin/llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b \
  -riscv-v-vector-bits-min=1024 -riscv-v-reg-pressure-aware-sched \
  /tmp/rvv-pressure-report/vector_add_32.ll \
  -o /tmp/rvv-pressure-report/vector_add_32.O2.opt.s
```

The same command shape was used for `-O3` and for `top2_32.ll` and
`rmsnorm_32.ll`. For `safe_softmax_32.ll`, `-mattr` was
`+v,+experimental-yushuxin-vfexp,+zvl1024b` so `llvm.exp.v128f32` used the
experimental instruction.

The exact loop used to produce every row was:

```sh
for w in vector_add_32 safe_softmax_32 top2_32 rmsnorm_32; do
  for optlevel in O2 O3; do
    for mode in base opt; do
      flags="-${optlevel} -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024"
      if [ "$w" = safe_softmax_32 ]; then
        flags="-${optlevel} -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024"
      fi
      if [ "$mode" = opt ]; then
        flags="$flags -riscv-v-reg-pressure-aware-sched"
      fi
      build-riscv/bin/llc $flags /tmp/rvv-pressure-report/${w}.ll \
        -o /tmp/rvv-pressure-report/${w}.${optlevel}.${mode}.s
      rg -o '\b[vu][sl][1248]r\.v\b' \
        /tmp/rvv-pressure-report/${w}.${optlevel}.${mode}.s | wc -l
      build-riscv/bin/llc $flags -riscv-v-reg-pressure-report \
        /tmp/rvv-pressure-report/${w}.ll -o /dev/null 2>&1
    done
  done
done
```

Whole-register spill/reload counts were collected with:

```sh
rg -o '\b[vu][sl][1248]r\.v\b' output.s | wc -l
```

Diagnostics were collected by adding `-riscv-v-reg-pressure-report`.

The alloca sanity check for the diagnostic counter was:

```sh
build-riscv/bin/llc -mtriple=riscv64 -mattr=+v,+zvl128b \
  -riscv-v-reg-pressure-report -o /dev/null - <<'EOF'
define void @sv_alloca() {
entry:
  %x = alloca <vscale x 4 x i32>, align 16
  ret void
}
EOF
```

It produced:

```text
riscv-v-reg-pressure-report: function=sv_alloca rvv-scalable-stack-bytes=16 rvv-spill-slots=0 fixed-stack-estimate=0
```

## Results

| Workload | Opt | Baseline whole-reg spills/reloads | Optimized whole-reg spills/reloads | Baseline RVV stack / slots | Optimized RVV stack / slots |
|---|---:|---:|---:|---:|---:|
| 32 x `<128 x float>` add | O2 | 50 | 0 | 768 / 24 | 0 / 0 |
| 32 x `<128 x float>` add | O3 | 50 | 0 | 768 / 24 | 0 / 0 |
| safe-softmax reduce + exp | O2 | 86 | 17 | 816 / 33 | 96 / 3 |
| safe-softmax reduce + exp | O3 | 86 | 17 | 816 / 33 | 96 / 3 |
| top-2 compare/select/reduce | O2 | 93 | 8 | 872 / 34 | 64 / 2 |
| top-2 compare/select/reduce | O3 | 93 | 8 | 872 / 34 | 64 / 2 |
| RMSNorm square/sum/sqrt/normalize | O2 | 268 | 37 | 2208 / 69 | 128 / 4 |
| RMSNorm square/sum/sqrt/normalize | O3 | 268 | 37 | 2208 / 69 | 128 / 4 |

Representative diagnostics from the measured O2 runs:

```text
riscv-v-reg-pressure-report: function=vector_add_32 rvv-scalable-stack-bytes=768 rvv-spill-slots=24 fixed-stack-estimate=160
riscv-v-reg-pressure-report: function=vector_add_32 rvv-scalable-stack-bytes=0 rvv-spill-slots=0 fixed-stack-estimate=672
riscv-v-reg-pressure-report: function=safe_softmax_32 rvv-scalable-stack-bytes=816 rvv-spill-slots=33 fixed-stack-estimate=32
riscv-v-reg-pressure-report: function=safe_softmax_32 rvv-scalable-stack-bytes=96 rvv-spill-slots=3 fixed-stack-estimate=608
riscv-v-reg-pressure-report: function=top2_32 rvv-scalable-stack-bytes=872 rvv-spill-slots=34 fixed-stack-estimate=0
riscv-v-reg-pressure-report: function=top2_32 rvv-scalable-stack-bytes=64 rvv-spill-slots=2 fixed-stack-estimate=608
riscv-v-reg-pressure-report: function=rmsnorm_32 rvv-scalable-stack-bytes=2208 rvv-spill-slots=69 fixed-stack-estimate=176
riscv-v-reg-pressure-report: function=rmsnorm_32 rvv-scalable-stack-bytes=128 rvv-spill-slots=4 fixed-stack-estimate=608
```

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
