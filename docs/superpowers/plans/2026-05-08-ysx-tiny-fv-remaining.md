# YSX Tiny-F/Tiny-V Remaining Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the remaining YSX tiny-F/tiny-V plan after the initial auto-td builtin proof.

**Architecture:** Keep per-instruction facts in structured YAML and opcode-source files, extend the generator only for repeated mechanical surfaces, and keep handwritten C++ limited to reusable lowering behavior. After the 2026-06-14 priority reset, direct vector ABI and broad vectorizer work are out of scope for this auto-td completion pass.

**Tech Stack:** LLVM TableGen, YSX backend C++, Clang target builtins/resource headers, Python auto-td generator, lit/FileCheck, pinned `riscv-opcodes`.

---

## File Map

- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/*.py`: generator model, loading, validation, and TD emission.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/*.yaml`: semantic operand/effect categories.
- `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/*.yaml`: scalar f32 instruction source records.
- `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/*.yaml`: vector instruction source records.
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`: shared operand classes and generated include anchors.
- `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td`: FPR/VR register classes and ABI support boundaries.
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`: fixed proof vector legal types, operation actions, and explicit builtin lowering only.
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`: custom selection for proof DAG nodes and generated instruction records.
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`: copy/spill/load/store helper emission.
- `clang/include/clang/Basic/BuiltinsYSX.td`, `clang/lib/Headers/ysx_vector.h`, `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp`: explicit C builtin surface.
- `llvm/test/MC/YSX/*.s`, `llvm/test/CodeGen/YSX/*.ll`, `clang/test/CodeGen/YSX/*.c`: regression and proof tests.
- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`, `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`, `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`: design and release documentation.

## Task 1: Auto-TD Tiny-F MC Surface

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/emit_td.py`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/opcodes.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/scalar-float.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/flw.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fsw.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fadd_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fsub_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fmul_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/feq_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/flt_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fle_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fcvt_w_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fcvt_wu_s.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fcvt_s_w.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/fcvt_s_wu.yaml`
- Modify: `llvm/lib/Target/YuShuXin/Disassembler/YSXDisassembler.cpp`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tests/test_tinyf_mc_support.py`
- Create: `llvm/test/MC/YSX/tinyf-auto-td.s`

- [x] **Step 1: Write generator red tests**

Add assertions to `test_generator.py` that expect `YSXGenAutoTinyFInstrInfo.inc` to contain `YSX_AUTO_FLW`, `YSX_AUTO_FSW`, `YSX_AUTO_FADD_S`, `YSX_AUTO_FSUB_S`, `YSX_AUTO_FMUL_S`, `YSX_AUTO_FEQ_S`, `YSX_AUTO_FLT_S`, `YSX_AUTO_FLE_S`, `YSX_AUTO_FCVT_W_S`, `YSX_AUTO_FCVT_WU_S`, `YSX_AUTO_FCVT_S_W`, and `YSX_AUTO_FCVT_S_WU`.

- [x] **Step 2: Verify red**

Run:

```bash
python3 -m unittest llvm.lib.Target.YuShuXin.auto-td.tests.test_generator.GeneratorTest.test_generator_writes_real_tinyf_instrinfo_from_opcode_sources -v
python3 -m unittest llvm.lib.Target.YuShuXin.auto-td.tests.test_tinyf_mc_support -v
```

Expected before implementation: failure mentioning missing tiny-F records.

- [x] **Step 3: Implement tiny-F emission**

Extend the generator so `write_td_outputs()` routes `instructions/tiny-f/*.yaml` into `YSXGenAutoTinyFInstrInfo.inc`, with `Predicates = [HasStdExtXTinyF]`, FPR32/GPR operand mapping, numeric `rm` via `uimm3`, scalar memory operands via `GPRMem` and `simm12_lo`, S-format `imm12hi/imm12lo` slicing, and instruction formats derived from opcode field shape. Ignore hidden editor temp files when scanning opcode extension sources.

- [x] **Step 4: Add tiny-F YAML and taxonomy**

Use `riscv-opcodes/rv_f` keys:

```text
flw, fsw, fadd_s, fsub_s, fmul_s, feq_s, flt_s, fle_s,
fcvt_w_s, fcvt_wu_s, fcvt_s_w, fcvt_s_wu
```

- [x] **Step 5: Add MC lit proof**

Create `llvm/test/MC/YSX/tinyf-auto-td.s` with `llvm-mc -triple=ysx64 -mattr=+xtinyf -show-encoding`, feature-gating checks, and object disassembly checks for the twelve generated instructions using numeric rounding mode operands.

- [ ] **Step 6: Verify and commit**

Run:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyf-auto-td.s
git diff --check
git add llvm/lib/Target/YuShuXin/auto-td llvm/test/MC/YSX/tinyf-auto-td.s llvm/lib/Target/YuShuXin/YSXInstrInfo.td
git commit -m "feat: generate YSX tiny-f MC records"
```

Expected: unit tests pass, MC lit passes, whitespace check has no output.

## Task 2: Auto-TD Tiny-V Bulk MC Surface

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-memory.yaml`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-alu.yaml`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-reduce.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-shuffle.yaml`
- Create: additional `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/*.yaml`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py`
- Modify: `llvm/test/MC/YSX/tinyv-auto-td.s`

- [ ] **Step 1: Write red tests for next vector batches**

Extend generator tests and `tinyv-auto-td.s` to require these next records:

```text
vlse32.v, vsse32.v, vluxei32.v, vsuxei32.v,
vsub.vv, vmul.vv, vmin.vv, vmax.vv, vand.vv, vor.vv, vxor.vv,
vmseq.vv, vmslt.vv, vmerge.vvm,
vredsum.vs, vredmin.vs, vredmax.vs, vredand.vs, vredor.vs, vredxor.vs,
vfredmin.vs, vfredmax.vs,
vslideup.vx, vslidedown.vx, vrgather.vv, vmv.v.x, vmv.v.v
```

- [ ] **Step 2: Verify red**

Run:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyv-auto-td.s
```

Expected before implementation: failures for missing generated records or unsupported operand shapes.

- [ ] **Step 3: Implement taxonomy categories**

Add structured categories for strided memory, indexed memory, integer binary ALU, integer compares producing mask/vector register class, merge, integer reductions, float min/max reductions, slide, gather, and splat/move.

- [ ] **Step 4: Implement generator gaps only if tests require them**

Supported new repeated shapes should include GPR stride/index operands, `simm5` or `uimm` operands if selected later, and field-specific register class mapping where `vd` may be a mask/vector destination.

- [ ] **Step 5: Verify and commit**

Run:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyv-auto-td.s
git diff --check
git add llvm/lib/Target/YuShuXin/auto-td llvm/test/MC/YSX/tinyv-auto-td.s
git commit -m "feat: bulk generate YSX tiny-v MC records"
```

Expected: all selected instructions assemble/disassemble from generated records with no copied encoding bits in YAML.

## Task 3: Tiny-F CodeGen Proof

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- Modify: `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- Modify: `llvm/lib/Target/YuShuXin/Disassembler/YSXDisassembler.cpp`
- Create: `llvm/test/CodeGen/YSX/tinyf-isel.ll`
- Create: `clang/test/CodeGen/YSX/tinyf-builtins-asm.c`

- [ ] **Step 1: Write red CodeGen tests**

Add IR tests for f32 add/sub/mul/cmp/cvt and a C builtin or C expression smoke test that keeps ABI soft-float while selecting f32 instructions in function bodies.

- [ ] **Step 2: Verify red**

Run:

```bash
python3 build/bin/llvm-lit -sv llvm/test/CodeGen/YSX/tinyf-isel.ll clang/test/CodeGen/YSX/tinyf-builtins-asm.c
```

Expected before implementation: selection failure or missing f32 instruction output.

- [ ] **Step 3: Implement minimal scalar-F lowering**

Add FPR32 copy/spill support, FPR32 decode support if TableGen requires `DecodeFPR32RegisterClass`, legalize `f32` only under `xtinyf`, and custom select arithmetic/compare/convert nodes to generated tiny-F records.

- [ ] **Step 4: Verify and commit**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
python3 build/bin/llvm-lit -sv llvm/test/CodeGen/YSX/tinyf-isel.ll clang/test/CodeGen/YSX/tinyf-builtins-asm.c
git diff --check
git add llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX/tinyf-isel.ll clang/test/CodeGen/YSX/tinyf-builtins-asm.c
git commit -m "feat: add YSX tiny-f codegen proof"
```

Expected: f32 operations select to generated tiny-F records without enabling `lp64f`.

## Task 4: Extended Tiny-V Builtin Proofs

**Files:**
- Modify: `clang/include/clang/Basic/BuiltinsYSX.td`
- Modify: `clang/lib/Headers/ysx_vector.h`
- Modify: `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp`
- Modify: `llvm/include/llvm/IR/IntrinsicsYSX.td`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- Modify: `llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll`
- Modify: `clang/test/CodeGen/YSX/tinyv-builtins-asm.c`

- [ ] **Step 1: Write red builtin tests**

Add explicit `ysx_vector.h` APIs for one representative per class:

```text
ysx_vsub_vv_i32m1, ysx_vmul_vv_i32m1, ysx_vredsum_vs_i32m1,
ysx_vfredsum_vs_f32m1, ysx_vrgather_vv_i32m1, ysx_vslideup_vx_i32m1
```

- [ ] **Step 2: Verify red**

Run:

```bash
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-builtins-asm.c llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll
```

Expected before implementation: missing builtin declarations or missing ISel.

- [ ] **Step 3: Implement minimal lowering**

Add LLVM YSX intrinsics and Clang builtin lowering only for the representative builtins. Extend `YSXISelDAGToDAG.cpp` using the existing `vsetvli` glue helper and generated records.

- [ ] **Step 4: Verify and commit**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-builtins-asm.c llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll
git diff --check
git add clang llvm
git commit -m "feat: extend YSX tiny-v builtin proof"
```

Expected: representative ALU, reduction, and shuffle builtins compile to asm/object through generated records.

## Task 5: Direct Vector ABI Boundary

**Status:** Superseded by the 2026-06-14 priority reset.

Do not implement direct vector ABI passing or returning as part of this auto-td
completion pass. The accepted proof shape is explicit fixed 128-bit
`ysx_vector.h` / `__builtin_ysx_*` use that stores results to memory and maps
one-to-one to generated instruction records. Direct vector ABI design remains
future work and must not be mixed into the auto-td generator completion.

## Task 6: Automatic Vectorization Smoke

**Status:** Superseded by the 2026-06-14 priority reset.

Do not implement this task as originally written. The current accepted scope is
explicit fixed 128-bit `ysx_vector.h` / `__builtin_ysx_*` proof APIs and
one-to-one fixed-width IR smoke only. Do not add generic loop-vectorizer hooks,
`YSXTargetTransformInfo` policy, direct vector ABI tests, vscale frontend
plumbing, or broad autovec coverage while completing the auto-td goal.

The replacement work is to generate and consume the selected
`builtin.codegen: true` YAML facts in Clang and LLVM TableGen, then prove the
explicit C API maps to generated instruction records.

## Task 7: Final Validation, Review, and Docs

**Files:**
- Modify: `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- Modify: `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`

- [ ] **Step 1: Update documentation**

Record implemented features, generated surfaces, retained gaps, verification evidence, and blog-ready feature summary.

- [ ] **Step 2: Run full validation**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX
git diff --check
```

Expected: build exits 0, unit tests pass, directory lit has zero failures, whitespace check has no output.

- [ ] **Step 3: Request final review**

Dispatch a read-only review agent against the full diff since `239330a62777`, requiring `Critical/Important/Minor` findings with file/line references.

- [ ] **Step 4: Commit docs and close goal**

Run:

```bash
git add docs/superpowers
git commit -m "docs: summarize completed YSX tiny FV expansion"
```

Expected: clean worktree, final commit includes updated spec/checklist/blog.
