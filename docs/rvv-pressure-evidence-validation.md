# RISCV RVV Pressure Evidence Validation

`llvm/utils/rvv_pressure_evidence.py` is a repo-root utility for collecting
the softmax evidence required by `docs/rvv-remat-correctness-sched-plan.md`
without committing raw dumps.

## Commands

Compiler evidence for generated softmax and cleaned realcase softmax:

```sh
python3 llvm/utils/rvv_pressure_evidence.py --opt O2 --opt O3 \
  --work-dir /tmp/rvv-pressure-evidence
```

Optional qemu runtime validation:

```sh
python3 llvm/utils/rvv_pressure_evidence.py --runtime \
  --work-dir /tmp/rvv-pressure-runtime
```

The script defaults to `build-riscv`; use `--llvm-build PATH` or `LLC`,
`LLVM_MC`, and `LD_LLD` to point at another build. Runtime checks use
`qemu-riscv64` from `PATH` unless `--qemu PATH` is provided.

## What It Proves

- All generated and realcase softmax compiler invocations include
  `-mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b`, so `exp2` lowers to
  `yushuxin.vfexp`; any `exp2f` fallback is reported as `FAIL`.
- Generated softmax stores are checked to depend on `%sumv`, preserving the
  denominator dependency.
- Stage1+2 pass dumps capture RVV remat, MachineScheduler, and greedy
  register allocation sections. The script checks that reduction instruction
  counts are unchanged across the RVV remat pass while vfexp/load counts may
  change.
- Final assembly spill/reload counts use whole-register vector
  `vs[1248]r.v`/`vl[1248]r.v` matches.
- Runtime checks build tiny freestanding add and softmax ELFs for baseline and
  stage1+2, run them under local qemu, and report `PASS`, `FAIL`, or `SKIP`.
  Add output is checked exactly. Softmax uses nontrivial input data and compares
  output plus the reduction sum against a Python host reference with tolerance.
  The runtime-only softmax ELF deliberately uses the qemu-runnable non-Yushuxin
  scalar `exp2f` lowering plus a freestanding polynomial `exp2f` helper over
  the exercised domain; the compiler evidence path above still uses vector
  `yushuxin.vfexp`.

Missing tools or qemu CPU/ISA limitations are explicit `SKIP` results, not pass
results. Use the script output from the local machine as the runtime evidence;
do not treat a missing emulator or rejected CPU configuration as a successful
runtime validation.

## Current Observation

The current checked command:

```sh
python3 llvm/utils/rvv_pressure_evidence.py --generated-count 512 \
  --opt O2 --opt O3 --work-dir /tmp/rvv-pressure-evidence-r11-strict-round
```

reported `PASS=25 FAIL=0 SKIP=2`. The generated true-softmax rows were:

| Workload | Opt | Mode | Whole-register spills/reloads | vfexp |
| --- | --- | --- | ---: | ---: |
| generated-softmax-512 | O2 | baseline | 4034 | 512 |
| generated-softmax-512 | O2 | stage1 | 2532 | 512 |
| generated-softmax-512 | O2 | stage1+2 | 0 | 1024 |
| generated-softmax-512 | O3 | baseline | 4034 | 512 |
| generated-softmax-512 | O3 | stage1 | 2532 | 512 |
| generated-softmax-512 | O3 | stage1+2 | 0 | 1024 |

For generated-softmax-512, the remat pass-dump check reported unchanged
reduction counts at O2 and O3: `513 -> 513`. It also reported vfexp count
`512 -> 1024` and load count `512 -> 1536`, which is the expected load plus
pure element-wise recompute pattern, not reduction recomputation. The script
also confirmed all 512 generated output stores depend on `%sumv`.

## Key Dump Excerpts

These excerpts are from:

```sh
python3 llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py softmax 4 |
build-riscv/bin/llc -O2 $VLEN_FLAGS \
  -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat \
  -stop-after=riscv-v-reg-pressure-remat -o -
```

The first phase computes the original element-wise exponentials in a streaming
order immediately before the sum reduction:

```llvm
%89:vrm4 = PseudoVLE32_V_M4 ...
%30:vrm4 = PseudoVFSUB_VFPR32_M4_E32 ... %89, %29 ...
%31:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 ... %30 ...
%90:vrm4 = PseudoVLE32_V_M4 ...
%32:vrm4 = PseudoVFSUB_VFPR32_M4_E32 ... %90, %29 ...
%33:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 ... %32 ...
%38:vrm4 = PseudoVFADD_VV_M4_E32 ... %31, %33 ...
...
undef %83.sub_vrm1_0:vrm4 = PseudoVFREDUSUM_VS_M4_E32 ... %40 ...
```

After the sum reduction, stage2 uses the existing denominator value `%48` and
recomputes only load/sub/vfexp chains next to each late normalize/store:

```llvm
%93:vrm4 = PseudoVLE32_V_M4 ...
%94:vrm4 = PseudoVFSUB_VFPR32_M4_E32 ... %93, %29 ...
%95:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 ... %94 ...
%49:vrm4 = PseudoVFMUL_VV_M4_E32 ... %95, %48 ...
PseudoVSE32_V_M4 %49 ...
%96:vrm4 = PseudoVLE32_V_M4 ...
%97:vrm4 = PseudoVFSUB_VFPR32_M4_E32 ... %96, %29 ...
%98:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 ... %97 ...
%50:vrm4 = PseudoVFMUL_VV_M4_E32 ... %98, %48 ...
PseudoVSE32_V_M4 %50 ...
```

The reductions themselves are not duplicated: generated softmax keeps
`513 -> 513` reduction matches across the remat pass at O2 and O3. The cleaned
realcase softmax keeps `2 -> 2` reductions; its tiny pressure window does not
need a second vfexp chain, and the pass leaves the single `PseudoVFREDMAX`,
single `PseudoYUSHUXIN_VFEXP`, and single `PseudoVFREDUSUM` structure intact.
Final assembly for generated-softmax-512 stage1+2 contains zero
`vs[1248]r.v`/`vl[1248]r.v` whole-register vector spill/reload matches.

The current runtime command:

```sh
python3 llvm/utils/rvv_pressure_evidence.py --runtime --generated-count 1 \
  --opt O2 --work-dir /tmp/rvv-pressure-runtime-r11-strict-round
```

reported `PASS=19 FAIL=0 SKIP=0`. Add baseline and stage1+2 both linked, ran
under qemu, and matched the exact expected output bytes. Softmax baseline and
stage1+2 both linked, ran under qemu, and matched the Python host reference:
`max_abs=1.11759e-08`, `sum_abs=1.33514e-05`, and direct `exp2f` probe
`exp2f_abs=5.96046e-08` for both variants on this machine.
