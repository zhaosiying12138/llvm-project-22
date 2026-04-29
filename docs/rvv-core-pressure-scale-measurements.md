# RISCV RVV Core Pressure Scale Measurements

Fixed configuration:

```sh
VLEN_FLAGS="-mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024"
COUNT_RE='\b[vu][sl][1248]r\.v\b'
```

Counts are whole-register vector spill/reload instructions matched by
`$COUNT_RE` in `llc -o -` output. The Yushuxin feature is present on baseline,
stage1, and stage1+2 runs so softmax always lowers `exp2(log2e * x)` to vector
`yushuxin.vfexp` instead of a scalar `exp2f` libcall.

## Generated Stress Cases

The checked-in lit test
`llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-scale.ll` uses
`llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py` to generate:

- `add 896`: baseline count is about 4096.
- `softmax 512`: baseline count is about 4096 and the final stores compute
  `exp(x - max(x)) / sum(exp(x - max(x)))`.

The lit test also checks a small generated-IR sample so the `%norm*` stores are
fed by `%sumv`, not by denominator-independent exponentials.

## Results

| Workload | Opt | Baseline | Stage 1 | Stage 1+2 |
| --- | --- | ---: | ---: | ---: |
| add-896 | O2 | 4056 | 0 | 0 |
| softmax-512 | O2 | 4034 | 2532 | 0 |
| add-896 | O3 | 4056 | 0 | 0 |
| softmax-512 | O3 | 4034 | 2532 | 0 |

The add result still shows the intended bounded behavior. For true softmax,
stage1 alone remains proportional because the reduce-then-normalize dependency
keeps wide exponentials live. Stage1+2 removes the whole-register vector
spill/reload matches by cloning safe loads and recomputing only pure
element-wise exp chains near the late stores. This table replaces the older
denominator-independent softmax-like numbers.

## Reproduction

Use the same commands with `-O3` for the O3 rows; all other flags stay the same.

```sh
python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py add 896 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py add 896 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -riscv-rvv-pressure-dag-sched -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS -riscv-rvv-pressure-dag-sched -o - |
  grep -Eo "$COUNT_RE" | wc -l

python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
build-riscv/bin/llc -O2 $VLEN_FLAGS \
  -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -o - |
  grep -Eo "$COUNT_RE" | wc -l
```

Lowering sanity check for all softmax modes. Expected result: non-zero
`yushuxin.vfexp` and `exp2f=0` for every line.

```sh
for mode in baseline stage1 stage1_2; do
  case "$mode" in
    baseline) MODE_FLAGS="" ;;
    stage1) MODE_FLAGS="-riscv-rvv-pressure-dag-sched" ;;
    stage1_2) MODE_FLAGS="-riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat" ;;
  esac
  asm="$(
    python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 512 |
    build-riscv/bin/llc -O2 $VLEN_FLAGS $MODE_FLAGS -o -
  )"
  vfexp_count="$(printf '%s\n' "$asm" | grep -c 'yushuxin\.vfexp' || true)"
  exp2f_count="$(printf '%s\n' "$asm" | grep -c 'exp2f' || true)"
  printf '%s yushuxin.vfexp=%s exp2f=%s\n' \
    "$mode" "$vfexp_count" "$exp2f_count"
done
```

The lit form avoids relying on `/tmp` inputs:

```sh
build-riscv/bin/llvm-lit -q \
  llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-scale.ll
```
