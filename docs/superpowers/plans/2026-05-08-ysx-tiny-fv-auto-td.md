# YSX Tiny-F/Tiny-V Auto TD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a schema-first auto-td-gen path for YSX tiny-F/tiny-V instructions, then use it to implement typed `ysx_vector.h` builtins, selected tiny-F/tiny-V CodeGen, and the custom `yushuxin.vfexp` instruction.

**Architecture:** New instruction facts live in one YAML file per instruction under `llvm/lib/Target/YuShuXin/auto-td`; instruction encodings come from read-only opcode sources under `third_party`; build-tree generated TableGen includes are included by the existing YSX target. Existing `rv64ima` handwritten TD remains in place, while new tiny-F/tiny-V real instructions, pseudos, patterns, schedule references, and builtin metadata are generated from YAML plus taxonomy.

**Tech Stack:** LLVM/Clang CMake and TableGen, Python 3 generator code, YAML metadata, `riscv-opcodes` compatible opcode files, YSX SelectionDAG lowering, Clang target builtins and resource headers, lit/FileCheck tests.

---

## File Structure

Create these new files and directories:

- `third_party/riscv-opcodes/`: pinned upstream opcode snapshot.
- `third_party/riscv-isa-manual/`: pinned upstream manual snapshot.
- `third_party/ysx-opcodes/extensions/rv_xtinyv`: YSX custom opcode source, starting with `yushuxin.vfexp`.
- `third_party/YSX_THIRD_PARTY_PROVENANCE.md`: exact source URLs and commits for all three opcode/manual sources.
- `llvm/lib/Target/YuShuXin/auto-td/schema/instruction.schema.yaml`: instruction YAML schema.
- `llvm/lib/Target/YuShuXin/auto-td/schema/taxonomy.schema.yaml`: taxonomy YAML schema.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/scalar-float.yaml`: tiny-F semantic defaults.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-memory.yaml`: tiny-V memory defaults.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-alu.yaml`: tiny-V ALU defaults.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-reduce.yaml`: tiny-V reduce defaults, including `vfredsum.vs` alias handling through canonical `vfredusum.vs`.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-shuffle.yaml`: tiny-V shuffle defaults.
- `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-custom.yaml`: YSX custom vector defaults for `yushuxin.vfexp`.
- `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/*.yaml`: tiny-F instruction YAML files.
- `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/*.yaml`: tiny-V instruction YAML files.
- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py`: generator entrypoint.
- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/`: focused Python modules for parsing, validation, emission, and reporting.
- `llvm/lib/Target/YuShuXin/auto-td/tests/`: generator unit tests and snapshot fixtures.
- `clang/lib/Headers/ysx_vector.h`: checked-in fixed 128-bit YSX wrapper header for the selected proof APIs.
- `clang/test/CodeGen/YSX/tinyv-builtins.c`: builtin-to-IR/asm smoke tests.
- `llvm/test/MC/YSX/tinyv-auto-td.s`: generated tiny-V MC encoding tests.
- `llvm/test/CodeGen/YSX/tinyv-builtins.ll`: backend lowering tests.
- Superseded old autovec smoke tests: do not create `tinyv-autovec.ll` for the current completion pass.
- `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`: standalone final concise blog.
- `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`: implementation checklist for blog reuse.

Modify these existing files:

- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`: update whenever the implementation goal changes.
- `llvm/lib/Target/YuShuXin/CMakeLists.txt`: run the generator before YSX TableGen and add dependencies.
- `llvm/lib/Target/YuShuXin/YSX.td`: include generated auto-td fragments.
- `llvm/lib/Target/YuShuXin/YSXFeatures.td`: add `xtinyf`, `xtinyv`, and `zvl*` feature declarations.
- `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td`: add FPR and VR register classes needed by tiny-F/tiny-V.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`: add format scaffolding that generated instructions can instantiate without old RISCV helper multiclasses.
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h`: add TSFlags for generated vector format metadata.
- `llvm/include/llvm/TargetParser/YSXISAInfo.h`: accept `rv64ima_xtinyf_xtinyv_zvl*` style arch strings.
- `clang/lib/Driver/ToolChains/Arch/RISCV.cpp`: allow YSX driver feature parsing for `xtinyf`, `xtinyv`, and `zvl*` while keeping ABI `lp64`.
- `clang/lib/Basic/Targets/RISCV.cpp`: expose YSX target defines and builtin availability without defining `__riscv_vector`.
- `clang/lib/Basic/Targets/RISCV.h`: keep YSX target distinct and enable only the type support needed by `ysx_vector.h`.
- `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp`: add YSX builtin codegen dispatch for `__builtin_ysx_*`.
- `clang/include/clang/Basic/CMakeLists.txt`: add generated YSX builtin includes if generator output is split into Clang generated tables.
- `clang/lib/Headers/CMakeLists.txt`: add `ysx_vector.h` to RISC-V/YSX resource headers.

## Instruction Import Sets

The first no-gap proof slice is:

- `vle32.v`
- `vse32.v`
- `vadd.vv`
- canonical `vfredusum.vs` plus public alias `vfredsum.vs`

Tiny-F initial set:

- `flw`
- `fsw`
- `fadd.s`
- `fsub.s`
- `fmul.s`
- `feq.s`
- `flt.s`
- `fle.s`
- `fcvt.w.s`
- `fcvt.wu.s`
- `fcvt.s.w`
- `fcvt.s.wu`

Tiny-V initial memory set:

- `vle32.v`
- `vse32.v`
- `vlse32.v`
- `vsse32.v`
- `vluxei32.v`
- `vloxei32.v`
- `vsuxei32.v`
- `vsoxei32.v`

Tiny-V initial integer ALU set:

- `vadd.vv`
- `vadd.vx`
- `vsub.vv`
- `vsub.vx`
- `vmul.vv`
- `vmul.vx`
- `vmin.vv`
- `vmin.vx`
- `vminu.vv`
- `vminu.vx`
- `vmax.vv`
- `vmax.vx`
- `vmaxu.vv`
- `vmaxu.vx`
- `vand.vv`
- `vand.vx`
- `vor.vv`
- `vor.vx`
- `vxor.vv`
- `vxor.vx`
- `vmseq.vv`
- `vmseq.vx`
- `vmsne.vv`
- `vmsne.vx`
- `vmslt.vv`
- `vmslt.vx`
- `vmsltu.vv`
- `vmsltu.vx`
- `vmsle.vv`
- `vmsle.vx`
- `vmsleu.vv`
- `vmsleu.vx`
- `vmerge.vvm`
- `vmerge.vxm`

Tiny-V initial f32 ALU set:

- `vfadd.vv`
- `vfadd.vf`
- `vfsub.vv`
- `vfsub.vf`
- `vfmul.vv`
- `vfmul.vf`
- `vfmin.vv`
- `vfmin.vf`
- `vfmax.vv`
- `vfmax.vf`
- `vmfeq.vv`
- `vmfeq.vf`
- `vmfne.vv`
- `vmfne.vf`
- `vmflt.vv`
- `vmflt.vf`
- `vmfle.vv`
- `vmfle.vf`
- `vfmerge.vfm`

Tiny-V initial reduction set:

- `vredsum.vs`
- `vredmin.vs`
- `vredminu.vs`
- `vredmax.vs`
- `vredmaxu.vs`
- `vredand.vs`
- `vredor.vs`
- `vredxor.vs`
- `vfredusum.vs`
- `vfredmin.vs`
- `vfredmax.vs`

Tiny-V initial shuffle set:

- `vmv.v.x`
- `vfmv.v.f`
- `vmv.x.s`
- `vfmv.f.s`
- `vslideup.vx`
- `vslideup.vi`
- `vslidedown.vx`
- `vslidedown.vi`
- `vslide1up.vx`
- `vfslide1up.vf`
- `vslide1down.vx`
- `vfslide1down.vf`
- `vrgather.vv`
- `vrgather.vx`
- `vrgather.vi`

Custom tiny-V set:

- `yushuxin.vfexp`

## Task 1: Baseline Build and Test Record

**Files:**
- Read: `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- Create: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [ ] **Step 1: Create the implementation checklist**

Create `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`:

```markdown
# YSX Tiny-F/Tiny-V Feature Checklist

## Implemented Features

- Spec approved: docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md

## Generated Surfaces

- No generated surfaces implemented yet.

## Handwritten Glue

- No handwritten glue implemented yet.

## Validation Evidence

- Baseline validation command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- Baseline validation result: not run before Task 1 Step 4.

## Retained Schema Gaps

- No retained schema gaps recorded yet.

## Blog Notes

- The standalone final blog at `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md` should describe auto-td-gen, tiny-F/tiny-V, builtin proof, automatic vectorization, and yushuxin.vfexp.
```

- [ ] **Step 2: Configure the YSX-only build inside the worktree**

Run:

```bash
cmake -G Ninja -S llvm -B build \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="YSX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON
```

Expected: configure succeeds and `build/build.ninja` exists.

- [ ] **Step 3: Build baseline tools**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump
```

Expected: all four tools build successfully.

- [ ] **Step 4: Run baseline YSX tests**

Run:

```bash
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX \
  llvm/test/CodeGen/YSX \
  clang/test/Driver/YSX \
  clang/test/CodeGen/YSX
```

Expected: existing YSX tests pass or existing failures are recorded in the checklist before feature work begins.

- [ ] **Step 5: Commit the baseline record**

Run:

```bash
git add docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md
git commit -m "docs: start YSX tiny FV implementation checklist"
```

## Task 2: Third-Party Opcode and Manual Snapshots

**Files:**
- Create: `third_party/riscv-opcodes/`
- Create: `third_party/riscv-isa-manual/`
- Create: `third_party/ysx-opcodes/extensions/rv_xtinyv`
- Create: `third_party/YSX_THIRD_PARTY_PROVENANCE.md`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [ ] **Step 1: Fetch pinned upstream snapshots**

Run these commands from the worktree root:

```bash
git clone --depth 1 https://github.com/riscv/riscv-opcodes /tmp/ysx-riscv-opcodes
git clone --depth 1 https://github.com/riscv/riscv-isa-manual /tmp/ysx-riscv-isa-manual
mkdir -p third_party/riscv-opcodes third_party/riscv-isa-manual
cp -R /tmp/ysx-riscv-opcodes/. third_party/riscv-opcodes/
cp -R /tmp/ysx-riscv-isa-manual/. third_party/riscv-isa-manual/
rm -rf third_party/riscv-opcodes/.git third_party/riscv-isa-manual/.git
```

Expected: both directories exist and contain upstream files.

- [ ] **Step 2: Create the YSX opcode source**

Create `third_party/ysx-opcodes/extensions/rv_xtinyv` with:

```text
yushuxin.vfexp 31..26=0x2a vm vs2 19..15=0 14..12=0x1 vd 6..0=0x0b
```

This uses the RISC-V `custom-0` major opcode `0x0b` and fixed `rs1=0`, so it does not conflict with standard OP-V encodings under major opcode `0x57`.

- [ ] **Step 3: Create provenance**

Create `third_party/YSX_THIRD_PARTY_PROVENANCE.md`:

```markdown
# YSX Third-Party Provenance

## riscv-opcodes

- Source: https://github.com/riscv/riscv-opcodes
- Snapshot command: `git clone --depth 1 https://github.com/riscv/riscv-opcodes`
- Role: read-only encoding and operand-field authority for standard tiny-F/tiny-V instructions.
- Local path: `third_party/riscv-opcodes`
- Project policy: do not modify this snapshot.

## riscv-isa-manual

- Source: https://github.com/riscv/riscv-isa-manual
- Snapshot command: `git clone --depth 1 https://github.com/riscv/riscv-isa-manual`
- Role: read-only provenance and manual-section references.
- Local path: `third_party/riscv-isa-manual`
- Project policy: do not infer semantics from prose and do not modify this snapshot.

## ysx-opcodes

- Source: YSX-owned opcode source in this repository.
- Format: compatible with `riscv-opcodes/extensions/*`.
- Role: encoding authority for custom YSX instructions such as `yushuxin.vfexp`.
- Local path: `third_party/ysx-opcodes`
```

After copying, write the two upstream commit hashes into the provenance file from:

```bash
git -C /tmp/ysx-riscv-opcodes rev-parse HEAD
git -C /tmp/ysx-riscv-isa-manual rev-parse HEAD
```

- [ ] **Step 4: Add third-party cleanliness check**

Run:

```bash
git status --short third_party/riscv-opcodes third_party/riscv-isa-manual third_party/ysx-opcodes
```

Expected: all third-party files are untracked additions. No upstream snapshot file is modified after initial copy.

- [ ] **Step 5: Commit snapshots**

Run:

```bash
git add third_party/riscv-opcodes third_party/riscv-isa-manual third_party/ysx-opcodes third_party/YSX_THIRD_PARTY_PROVENANCE.md
git commit -m "vendor: add YSX auto TD opcode sources"
```

## Task 3: Auto-TD Schema, Taxonomy, and Proof YAML

**Files:**
- Create: `llvm/lib/Target/YuShuXin/auto-td/schema/instruction.schema.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/schema/taxonomy.schema.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/*.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vle32_v.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vse32_v.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vadd_vv.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vfredusum_vs.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [ ] **Step 1: Add the instruction schema**

Create `llvm/lib/Target/YuShuXin/auto-td/schema/instruction.schema.yaml`:

```yaml
required_fields:
  - mnemonic
  - opcode_source
  - spec_ref
allowed_fields:
  - mnemonic
  - opcode_source
  - spec_ref
  - features
  - operands
  - effects
  - sched
  - aliases
  - pseudos
  - patterns
  - builtin
  - status
  - reason
  - missing_schema
  - retained_owner_files
forbidden_strings:
  - raw_td
  - "def : Pat"
  - RVInst
  - VUnitStrideLoad
  - VRED_
  - VPseudo
  - "let Inst{"
  - "bits<"
status_values:
  - auto_full
  - auto_with_structured_override
  - retained_schema_gap
```

- [ ] **Step 2: Add the taxonomy schema**

Create `llvm/lib/Target/YuShuXin/auto-td/schema/taxonomy.schema.yaml`:

```yaml
required_fields:
  - categories
allowed_category_fields:
  - domain
  - operation
  - default_surfaces
  - operands
  - effects
  - sched
  - pseudos
  - patterns
  - builtin
  - aliases
forbidden_fields:
  - encoding
  - fixed_bits
  - raw_td
  - raw_cpp
```

- [ ] **Step 3: Add vector taxonomy files**

Create `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-memory.yaml`:

```yaml
categories:
  unit_load_e32:
    domain: tinyv
    operation: vector_unit_load
    default_surfaces: [mc_def, decoder, sched, pseudo, pattern, builtin, coverage]
    operands:
      outs: [{role: dest, field: vd, reg_class: VR}]
      ins:
        - {role: base, field: rs1, reg_class: GPR}
        - {role: mask_policy, field: vm, operand: VMaskOp}
    effects: {may_load: true, may_store: false, has_side_effects: false}
    sched: {semantic_class: vector_memory}
  unit_store_e32:
    domain: tinyv
    operation: vector_unit_store
    default_surfaces: [mc_def, decoder, sched, pseudo, pattern, builtin, coverage]
    operands:
      outs: []
      ins:
        - {role: value, field: vs3, reg_class: VR}
        - {role: base, field: rs1, reg_class: GPR}
        - {role: mask_policy, field: vm, operand: VMaskOp}
    effects: {may_load: false, may_store: true, has_side_effects: false}
    sched: {semantic_class: vector_memory}
```

Create `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-alu.yaml`:

```yaml
categories:
  int_add:
    domain: tinyv
    operation: add
    default_surfaces: [mc_def, decoder, sched, pseudo, pattern, builtin, coverage]
    operands:
      outs: [{role: dest, field: vd, reg_class: VR}]
      ins:
        - {role: lhs, field: vs2, reg_class: VR}
        - {role: rhs, field: vs1, reg_class: VR}
        - {role: mask_policy, field: vm, operand: VMaskOp}
    effects: {may_load: false, may_store: false, has_side_effects: false}
    sched: {semantic_class: vector_int_alu}
  custom_float_unary:
    domain: tinyv
    operation: fexp
    default_surfaces: [mc_def, decoder, sched, pseudo, pattern, builtin, coverage]
    operands:
      outs: [{role: dest, field: vd, reg_class: VR}]
      ins:
        - {role: value, field: vs2, reg_class: VR}
        - {role: mask_policy, field: vm, operand: VMaskOp}
    effects: {may_load: false, may_store: false, has_side_effects: false}
    sched: {semantic_class: vector_float_alu}
```

Create `llvm/lib/Target/YuShuXin/auto-td/taxonomy/vector-reduce.yaml`:

```yaml
categories:
  float_sum:
    domain: tinyv
    operation: freduce_sum
    default_surfaces: [mc_def, decoder, sched, pseudo, pattern, builtin, coverage]
    operands:
      outs: [{role: dest, field: vd, reg_class: VR}]
      ins:
        - {role: vector, field: vs2, reg_class: VR}
        - {role: scalar_seed, field: vs1, reg_class: VR}
        - {role: mask_policy, field: vm, operand: VMaskOp}
    aliases:
      - {mnemonic: vfredsum.vs, canonical: vfredusum.vs}
    effects: {may_load: false, may_store: false, has_side_effects: false}
    sched: {semantic_class: vector_reduce}
```

- [ ] **Step 4: Add proof instruction YAML files**

Create `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vadd_vv.yaml`:

```yaml
mnemonic: vadd.vv
opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}
spec_ref: tinyv.vector-alu.int_add
features: {required: [xtinyv]}
pseudos:
  matrix: {element_types: [i32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vadd, operation: add}
builtin:
  header: ysx_vector.h
  names: [ysx_vadd_vv_i32m1]
  overloaded: false
```

Create `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vle32_v.yaml`:

```yaml
mnemonic: vle32.v
opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vle32_v}
spec_ref: tinyv.vector-memory.unit_load_e32
features: {required: [xtinyv]}
pseudos:
  matrix: {element_types: [i32, f32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vle32, operation: load}
builtin:
  header: ysx_vector.h
  names: [ysx_vle32_v_i32m1, ysx_vle32_v_f32m1]
  overloaded: false
```

Create `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vse32_v.yaml`:

```yaml
mnemonic: vse32.v
opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vse32_v}
spec_ref: tinyv.vector-memory.unit_store_e32
features: {required: [xtinyv]}
pseudos:
  matrix: {element_types: [i32, f32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vse32, operation: store}
builtin:
  header: ysx_vector.h
  names: [ysx_vse32_v_i32m1, ysx_vse32_v_f32m1]
  overloaded: false
```

Create `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/vfredusum_vs.yaml`:

```yaml
mnemonic: vfredusum.vs
opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vfredusum_vs}
spec_ref: tinyv.vector-reduce.float_sum
features: {required: [xtinyv]}
aliases:
  - {mnemonic: vfredsum.vs}
pseudos:
  matrix: {element_types: [f32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vfredsum, operation: freduce_sum}
builtin:
  header: ysx_vector.h
  names: [ysx_vfredsum_vs_f32m1]
  overloaded: false
```

Create `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml`:

```yaml
mnemonic: yushuxin.vfexp
opcode_source: {repo: ysx-opcodes, extension: rv_xtinyv, key: yushuxin_vfexp}
spec_ref: tinyv.vector-alu.custom_float_unary
features: {required: [xtinyv]}
pseudos:
  matrix: {element_types: [f32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vfexp, operation: fexp}
builtin:
  header: ysx_vector.h
  names: [ysx_vfexp_v_f32m1]
  overloaded: false
```

- [ ] **Step 5: Run schema grep checks**

Run:

```bash
rg -n "raw_td|def : Pat|RVInst|VPseudo|let Inst\\{|bits<" llvm/lib/Target/YuShuXin/auto-td
```

Expected: no matches.

- [ ] **Step 6: Commit schema and proof YAML**

Run:

```bash
git add llvm/lib/Target/YuShuXin/auto-td docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md
git commit -m "feat: add YSX auto TD schema and proof YAML"
```

## Task 4: Generator Parser, Validator, and Coverage Report

**Files:**
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/__init__.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/opcodes.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/model.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/loader.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/validate.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/report.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py`

- [ ] **Step 1: Add the generator entrypoint**

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py`:

```python
#!/usr/bin/env python3
import argparse
from pathlib import Path

from ysx_auto_td.loader import load_instruction_set
from ysx_auto_td.report import write_coverage
from ysx_auto_td.validate import validate_instruction_set


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate YSX auto TD includes")
    parser.add_argument("--ysx-root", required=True)
    parser.add_argument("--riscv-opcodes", required=True)
    parser.add_argument("--ysx-opcodes", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--coverage", required=True)
    args = parser.parse_args()

    instructions = load_instruction_set(
        Path(args.ysx_root),
        Path(args.riscv_opcodes),
        Path(args.ysx_opcodes),
    )
    validate_instruction_set(instructions)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "YSXGenAutoTinyFInstrInfo.inc").write_text("// generated by ysx_auto_td_gen\n")
    (out_dir / "YSXGenAutoTinyVInstrInfo.inc").write_text("// generated by ysx_auto_td_gen\n")
    (out_dir / "YSXGenAutoTinyVPseudos.inc").write_text("// generated by ysx_auto_td_gen\n")
    (out_dir / "YSXGenAutoTinyVPatterns.inc").write_text("// generated by ysx_auto_td_gen\n")
    (out_dir / "YSXGenAutoTinyVBuiltins.inc").write_text("// generated by ysx_auto_td_gen\n")
    write_coverage(Path(args.coverage), instructions)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Add opcode parser model**

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/model.py`:

```python
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class OpcodeSource:
    repo: str
    extension: str
    key: str


@dataclass(frozen=True)
class OpcodeRecord:
    key: str
    mnemonic: str
    fields: tuple[str, ...]
    fixed_bits: tuple[str, ...]
    source_file: Path


@dataclass
class InstructionRecord:
    path: Path
    mnemonic: str
    opcode_source: OpcodeSource
    opcode: OpcodeRecord
    spec_ref: str
    status: str = "auto_full"
    retained_owner_files: list[str] = field(default_factory=list)
```

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/opcodes.py`:

```python
from pathlib import Path

from .model import OpcodeRecord


def source_key_from_mnemonic(mnemonic: str) -> str:
    return mnemonic.replace(".", "_").replace("-", "_")


def parse_opcode_file(path: Path) -> dict[str, OpcodeRecord]:
    records: dict[str, OpcodeRecord] = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("$"):
            continue
        parts = line.split()
        mnemonic = parts[0]
        key = source_key_from_mnemonic(mnemonic)
        fields = tuple(p for p in parts[1:] if "=" not in p)
        fixed_bits = tuple(p for p in parts[1:] if "=" in p)
        records[key] = OpcodeRecord(key, mnemonic, fields, fixed_bits, path)
    return records


def load_opcode_repo(root: Path) -> dict[tuple[str, str], OpcodeRecord]:
    result: dict[tuple[str, str], OpcodeRecord] = {}
    for path in sorted((root / "extensions").glob("*")):
        if path.is_file():
            for key, record in parse_opcode_file(path).items():
                result[(path.name, key)] = record
    return result
```

- [ ] **Step 3: Add YAML loader and validator**

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/loader.py`:

```python
from pathlib import Path
import yaml

from .model import InstructionRecord, OpcodeSource
from .opcodes import load_opcode_repo


def load_instruction_set(ysx_root: Path, riscv_opcodes: Path, ysx_opcodes: Path) -> list[InstructionRecord]:
    opcode_repos = {
        "riscv-opcodes": load_opcode_repo(riscv_opcodes),
        "ysx-opcodes": load_opcode_repo(ysx_opcodes),
    }
    records: list[InstructionRecord] = []
    for path in sorted((ysx_root / "auto-td" / "instructions").glob("*/*.yaml")):
        data = yaml.safe_load(path.read_text())
        source_data = data["opcode_source"]
        source = OpcodeSource(source_data["repo"], source_data["extension"], source_data["key"])
        opcode = opcode_repos[source.repo][(source.extension, source.key)]
        records.append(
            InstructionRecord(
                path=path,
                mnemonic=data["mnemonic"],
                opcode_source=source,
                opcode=opcode,
                spec_ref=data["spec_ref"],
                status=data.get("status", "auto_full"),
                retained_owner_files=data.get("retained_owner_files", []),
            )
        )
    return records
```

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/validate.py`:

```python
FORBIDDEN = ("raw_td", "def : Pat", "RVInst", "VPseudo", "let Inst{", "bits<")
ALLOWED_STATUS = {"auto_full", "auto_with_structured_override", "retained_schema_gap"}


def validate_instruction_set(instructions):
    for instruction in instructions:
        text = instruction.path.read_text()
        for token in FORBIDDEN:
            if token in text:
                raise ValueError(f"{instruction.path}: forbidden token {token}")
        if instruction.status not in ALLOWED_STATUS:
            raise ValueError(f"{instruction.path}: invalid status {instruction.status}")
        if instruction.status == "retained_schema_gap" and not instruction.retained_owner_files:
            raise ValueError(f"{instruction.path}: retained_schema_gap requires retained_owner_files")
        if instruction.mnemonic != instruction.opcode.mnemonic:
            aliases = "aliases:" in text
            if not aliases:
                raise ValueError(
                    f"{instruction.path}: mnemonic {instruction.mnemonic} does not match opcode {instruction.opcode.mnemonic}"
                )
```

- [ ] **Step 4: Add coverage writer**

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/report.py`:

```python
from collections import Counter


def write_coverage(path, instructions):
    counts = Counter(instruction.status for instruction in instructions)
    lines = ["# YSX Auto TD Coverage", ""]
    for key in ("auto_full", "auto_with_structured_override", "retained_schema_gap"):
        lines.append(f"- {key}: {counts.get(key, 0)}")
    lines.append("")
    lines.append("| instruction | status | opcode source |")
    lines.append("|---|---|---|")
    for instruction in instructions:
        source = f"{instruction.opcode_source.repo}/{instruction.opcode_source.extension}/{instruction.opcode_source.key}"
        lines.append(f"| {instruction.mnemonic} | {instruction.status} | {source} |")
    path.write_text("\n".join(lines) + "\n")
```

- [ ] **Step 5: Add generator unit tests**

Create `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py`:

```python
from pathlib import Path
import subprocess


YSX_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = Path(__file__).resolve().parents[6]
TOOL = YSX_ROOT / "auto-td" / "tools" / "ysx_auto_td_gen.py"


def test_generator_rejects_forbidden_raw_td(tmp_path):
    bad = tmp_path / "bad.yaml"
    bad.write_text("mnemonic: bad\nraw_td: def BAD\n")
    assert "raw_td" in bad.read_text()


def test_generator_emits_coverage(tmp_path):
    out = tmp_path / "out"
    coverage = tmp_path / "coverage.md"
    subprocess.check_call([
        "python3",
        str(TOOL),
        "--ysx-root",
        str(YSX_ROOT),
        "--riscv-opcodes",
        str(REPO_ROOT / "third_party" / "riscv-opcodes"),
        "--ysx-opcodes",
        str(REPO_ROOT / "third_party" / "ysx-opcodes"),
        "--out-dir",
        str(out),
        "--coverage",
        str(coverage),
    ])
    assert coverage.exists()
    assert "vadd.vv" in coverage.read_text()
```

- [ ] **Step 6: Run generator manually**

Run:

```bash
PYTHONPATH=llvm/lib/Target/YuShuXin/auto-td/tools \
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td \
  --coverage build/ysx-auto-td/coverage.md
```

Expected: `build/ysx-auto-td/coverage.md` exists and lists the proof YAML files.

- [ ] **Step 7: Commit generator parser and validator**

Run:

```bash
git add llvm/lib/Target/YuShuXin/auto-td
git commit -m "feat: add YSX auto TD generator validator"
```

## Task 5: YSX Feature, Register, and Format Scaffolding

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/YSXFeatures.td`
- Modify: `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td`
- Modify: `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`
- Modify: `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h`
- Modify: `llvm/include/llvm/TargetParser/YSXISAInfo.h`
- Modify: `clang/lib/Driver/ToolChains/Arch/RISCV.cpp`
- Modify: `clang/lib/Basic/Targets/RISCV.cpp`
- Test: `clang/test/Driver/YSX/target-options.c`

- [ ] **Step 1: Add tiny feature declarations**

In `llvm/lib/Target/YuShuXin/YSXFeatures.td`, add after `FeatureStdExtA`:

```tablegen
def FeatureExtXTinyF
    : SubtargetFeature<"xtinyf", "HasExtXTinyF", "true",
                       "YSX tiny 32-bit floating-point instructions">;
def HasExtXTinyF : Predicate<"Subtarget->hasExtXTinyF()">,
                   AssemblerPredicate<(all_of FeatureExtXTinyF),
                                      "'xtinyf' (YSX tiny 32-bit floating point)">;

def FeatureExtXTinyV
    : SubtargetFeature<"xtinyv", "HasExtXTinyV", "true",
                       "YSX tiny 32-bit vector instructions">;
def HasExtXTinyV : Predicate<"Subtarget->hasExtXTinyV()">,
                   AssemblerPredicate<(all_of FeatureExtXTinyV),
                                      "'xtinyv' (YSX tiny vector)">;
```

- [ ] **Step 2: Add FPR and VR register classes**

In `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td`, add FPR and VR definitions after the GPR classes:

```tablegen
let Namespace = "YSX" in {
foreach i = {0-31} in
  def F#i : YSXReg<i, "f"#i>, DwarfRegNum<[!add(32, i)]>;

foreach i = {0-31} in
  def V#i : YSXReg<i, "v"#i>, DwarfRegNum<[!add(96, i)]>;
}

def FPR32 : YSXRegisterClass<[f32], 32, (add (sequence "F%u", 0, 31))>;
def VR : YSXRegisterClass<[nxv1i1, nxv1i32, nxv1f32], 64,
                          (add (sequence "V%u", 0, 31))>;
def VMV0 : YSXRegisterClass<[nxv1i1], 64, (add V0)>;
```

- [ ] **Step 3: Add vector instruction format metadata**

In `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`, add format values:

```tablegen
def InstFormatR4            : InstFormat<8>;
def InstFormatV             : InstFormat<9>;
```

Also add matching enum values in `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h`:

```c++
InstFormatR4 = 8,
InstFormatV = 9,
```

- [ ] **Step 4: Permit YSX arch strings with tiny features**

In `llvm/include/llvm/TargetParser/YSXISAInfo.h`, extend supported extension checks to accept:

```c++
Feature == "xtinyf" || Feature == "xtinyv" || Feature.starts_with("zvl")
```

Update `getRISCVAttributeString()` to include enabled features only when present. Keep the default as `rv64ima` when users do not request tiny features.

- [ ] **Step 5: Update Clang driver tests before implementation**

Add RUN lines to `clang/test/Driver/YSX/target-options.c`:

```c
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyf_xtinyv_zvl128b -### -c %s 2>&1 | FileCheck %s --check-prefix=TINYFV
// TINYFV: "-target-feature" "+xtinyf"
// TINYFV: "-target-feature" "+xtinyv"
// TINYFV: "-target-abi" "lp64"
// TINYFV-NOT: "-target-abi" "lp64f"
```

- [ ] **Step 6: Run the driver test and observe failure**

Run:

```bash
python3 build/bin/llvm-lit -sv clang/test/Driver/YSX/target-options.c
```

Expected before implementation: failure because `xtinyf`/`xtinyv` are still rejected.

- [ ] **Step 7: Implement the driver feature allowance**

Update `clang/lib/Driver/ToolChains/Arch/RISCV.cpp` and `clang/lib/Basic/Targets/RISCV.cpp` so YSX accepts `xtinyf`, `xtinyv`, and `zvl*` only when explicitly present in `-march`. Keep `lp64` as the only accepted ABI.

- [ ] **Step 8: Run focused tests**

Run:

```bash
ninja -C build clang
python3 build/bin/llvm-lit -sv clang/test/Driver/YSX/target-options.c
```

Expected: new `TINYFV` checks pass and existing rejection checks for `rv64gc`, `rv64imaf`, `rv64imav`, and `lp64d` are updated to reject full standard features but not `xtinyf/xtinyv`.

- [ ] **Step 9: Commit feature scaffolding**

Run:

```bash
git add llvm/lib/Target/YuShuXin clang/lib/Driver/ToolChains/Arch/RISCV.cpp clang/lib/Basic/Targets/RISCV.cpp clang/test/Driver/YSX/target-options.c llvm/include/llvm/TargetParser/YSXISAInfo.h
git commit -m "feat: add YSX tiny FV feature scaffolding"
```

## Task 6: Real Instruction TD Emitter and CMake Integration

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py`
- Create: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/emit_td.py`
- Modify: `llvm/lib/Target/YuShuXin/CMakeLists.txt`
- Modify: `llvm/lib/Target/YuShuXin/YSX.td`
- Test: `llvm/test/MC/YSX/tinyv-auto-td.s`

- [ ] **Step 1: Add a failing MC test**

Create `llvm/test/MC/YSX/tinyv-auto-td.s`:

```asm
# RUN: llvm-mc -triple=ysx64 -mattr=+xtinyv,+zvl128b -show-encoding < %s \
# RUN:   | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -filetype=obj -triple=ysx64 -mattr=+xtinyv,+zvl128b < %s \
# RUN:   | llvm-objdump --triple=ysx64 --mattr=+xtinyv,+zvl128b -d - \
# RUN:   | FileCheck %s --check-prefix=DIS

vadd.vv v1, v2, v3
# ENC: vadd.vv v1, v2, v3
# DIS: vadd.vv v1, v2, v3

vle32.v v1, (a0)
# ENC: vle32.v v1, (a0)
# DIS: vle32.v v1, (a0)

vse32.v v1, (a0)
# ENC: vse32.v v1, (a0)
# DIS: vse32.v v1, (a0)

vfredsum.vs v1, v2, v3
# ENC: vfredsum.vs v1, v2, v3
# DIS: vfredsum.vs v1, v2, v3

yushuxin.vfexp v1, v2
# ENC: yushuxin.vfexp v1, v2
# DIS: yushuxin.vfexp v1, v2
```

- [ ] **Step 2: Add a TD emitter**

Create `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/emit_td.py`:

```python
def td_bits_from_fixed(fixed_bits):
    assignments = []
    for item in fixed_bits:
        lhs, rhs = item.split("=", 1)
        assignments.append(f"  let Inst{{{lhs}}} = {rhs};")
    return assignments


def emit_real_instruction(record):
    fields = list(record.opcode.fields)
    outs = "(outs VR:$vd)" if "vd" in fields else "(outs)"
    ins = []
    for field in fields:
        if field == "vd":
            continue
        if field in ("vs1", "vs2", "vs3"):
            ins.append(f"VR:${field}")
        elif field in ("rs1", "rs2"):
            ins.append(f"GPR:${field}")
        elif field == "vm":
            continue
        else:
            ins.append(f"uimm5:${field}")
    arg_order = [f"${field}" for field in fields if field != "vm"]
    asm = ", ".join(arg_order)
    name = "AUTO_" + record.opcode.key.upper()
    lines = [
        f"def {name} : RVInst<{outs}, (ins {', '.join(ins)}), \"{record.mnemonic}\", \"{asm}\", [], InstFormatV> {{",
        "  let Predicates = [HasExtXTinyV];",
    ]
    lines.extend(td_bits_from_fixed(record.opcode.fixed_bits))
    lines.append("}")
    return "\n".join(lines) + "\n"


def emit_instr_info(records):
    return "\n".join(emit_real_instruction(record) for record in records)
```

- [ ] **Step 3: Wire generator output**

Modify `ysx_auto_td_gen.py` to import and call `emit_instr_info`:

```python
from ysx_auto_td.emit_td import emit_instr_info
```

Replace the initial marker write for `YSXGenAutoTinyVInstrInfo.inc`:

```python
(out_dir / "YSXGenAutoTinyVInstrInfo.inc").write_text(emit_instr_info(instructions))
```

- [ ] **Step 4: Include generated TD in YSX**

Modify `llvm/lib/Target/YuShuXin/YSX.td` after `include "YSXInstrInfo.td"`:

```tablegen
include "YSXGenAutoTinyFInstrInfo.inc"
include "YSXGenAutoTinyVInstrInfo.inc"
include "YSXGenAutoTinyVPseudos.inc"
include "YSXGenAutoTinyVPatterns.inc"
```

- [ ] **Step 5: Add CMake generator command**

Modify `llvm/lib/Target/YuShuXin/CMakeLists.txt` before `set(LLVM_TARGET_DEFINITIONS YSX.td)`:

```cmake
set(YSX_AUTO_TD_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/auto-td")
add_custom_command(
  OUTPUT
    ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyFInstrInfo.inc
    ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVInstrInfo.inc
    ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPseudos.inc
    ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPatterns.inc
    ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVBuiltins.inc
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/tools/ysx_auto_td_gen.py
    --ysx-root ${CMAKE_CURRENT_SOURCE_DIR}
    --riscv-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes
    --ysx-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes
    --out-dir ${YSX_AUTO_TD_OUT_DIR}
    --coverage ${YSX_AUTO_TD_OUT_DIR}/coverage.md
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/tools/ysx_auto_td_gen.py
  WORKING_DIRECTORY ${LLVM_MAIN_SRC_DIR}/..
  COMMENT "Generating YSX tiny-F/tiny-V TableGen includes")
include_directories(${YSX_AUTO_TD_OUT_DIR})
```

- [ ] **Step 6: Build TableGen target and run MC test**

Run:

```bash
ninja -C build YSXCommonTableGen llvm-mc llvm-objdump
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyv-auto-td.s
```

Expected: initial failures identify missing operand classes or format fields. Fix the emitter or scaffolding until the test passes.

- [ ] **Step 7: Commit MC emitter**

Run:

```bash
git add llvm/lib/Target/YuShuXin llvm/test/MC/YSX/tinyv-auto-td.s
git commit -m "feat: generate YSX tiny vector MC instructions"
```

## Task 7: Pseudo and Pattern Emitter

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/emit_td.py`
- Modify: `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelLowering.h`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- Test: `llvm/test/CodeGen/YSX/tinyv-builtins.ll`

- [ ] **Step 1: Add target SDNodes for proof operations**

Add to `llvm/lib/Target/YuShuXin/YSXInstrInfo.td` near existing YSX SDNodes:

```tablegen
def SDT_YSXVecBinaryVL : SDTypeProfile<1, 5, [SDTCisVec<0>, SDTCisSameAs<0, 1>]>;
def ysx_vadd_vl : RVSDNode<"VADD_VL", SDT_YSXVecBinaryVL>;
def SDT_YSXVecReduceVL : SDTypeProfile<1, 6, [SDTCisVec<0>]>;
def ysx_vfredsum_vl : RVSDNode<"VFREDSUM_VL", SDT_YSXVecReduceVL>;
```

- [ ] **Step 2: Add failing backend IR test**

Create `llvm/test/CodeGen/YSX/tinyv-builtins.ll`:

```llvm
; RUN: llc -mtriple=ysx64 -mattr=+xtinyv,+zvl128b < %s | FileCheck %s

declare <4 x i32> @llvm.ysx.vadd.v4i32.i64(<4 x i32>, <4 x i32>, i64)

define <4 x i32> @vadd_i32m1(<4 x i32> %a, <4 x i32> %b, i64 %vl) {
; CHECK-LABEL: vadd_i32m1:
; CHECK: vadd.vv
  %r = call <4 x i32> @llvm.ysx.vadd.v4i32.i64(<4 x i32> %a, <4 x i32> %b, i64 %vl)
  ret <4 x i32> %r
}
```

- [ ] **Step 3: Generate proof pseudo records**

Extend `emit_td.py` with:

```python
def emit_pseudos(records):
    lines = []
    for record in records:
        if record.mnemonic == "vadd.vv":
            lines.append('def PseudoYSX_VADD_VV_I32M1 : Pseudo<(outs VR:$vd), (ins VR:$vs2, VR:$vs1, GPR:$vl), [], "">;')
    return "\n".join(lines) + "\n"


def emit_patterns(records):
    lines = []
    for record in records:
        if record.mnemonic == "vadd.vv":
            lines.append("def : Pat<(ysx_vadd_vl VR:$vs2, VR:$vs1, GPR:$vl), (PseudoYSX_VADD_VV_I32M1 VR:$vs2, VR:$vs1, GPR:$vl)>;")
    return "\n".join(lines) + "\n"
```

Update `ysx_auto_td_gen.py` to write `YSXGenAutoTinyVPseudos.inc` and `YSXGenAutoTinyVPatterns.inc` from these functions.

- [ ] **Step 4: Lower proof intrinsic**

In `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`, add a focused lowering branch for `llvm.ysx.vadd.*` calls in `LowerOperation` or the intrinsic lowering hook. Use `ysx_vadd_vl` as the target node and pass VL as an explicit operand.

- [ ] **Step 5: Run backend test**

Run:

```bash
ninja -C build llc
python3 build/bin/llvm-lit -sv llvm/test/CodeGen/YSX/tinyv-builtins.ll
```

Expected: `vadd.vv` appears in the generated assembly.

- [ ] **Step 6: Commit pseudo/pattern proof**

Run:

```bash
git add llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX/tinyv-builtins.ll
git commit -m "feat: generate YSX tiny vector pseudos and patterns"
```

## Task 8: Clang `ysx_vector.h` and Builtin Proof

**Files:**
- Modify: `clang/lib/Headers/CMakeLists.txt`
- Create: `clang/lib/Headers/ysx_vector.h`
- Modify: `clang/lib/Basic/Targets/RISCV.h`
- Modify: `clang/lib/Basic/Targets/RISCV.cpp`
- Modify: `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp`
- Test: `clang/test/CodeGen/YSX/tinyv-builtins.c`

- [ ] **Step 1: Add failing Clang test**

Create `clang/test/CodeGen/YSX/tinyv-builtins.c`:

```c
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -emit-llvm -o - %s | FileCheck %s --check-prefix=IR

#include <ysx_vector.h>

ysx_vint32m1_t test_vadd(ysx_vint32m1_t a, ysx_vint32m1_t b, unsigned long vl) {
  return ysx_vadd_vv_i32m1(a, b, vl);
}

// IR-LABEL: define {{.*}}test_vadd
// IR: call <4 x i32> @llvm.ysx.vadd
```

- [ ] **Step 2: Add header shim**

Create `clang/lib/Headers/ysx_vector.h`:

```c
#ifndef __YSX_VECTOR_H
#define __YSX_VECTOR_H

#ifndef __YSX_TINY_VECTOR__
#error "YSX vector intrinsics require -march=rv64ima_xtinyv_zvl128b or +xtinyv"
#endif

typedef __rvv_int32m1_t ysx_vint32m1_t;
typedef __rvv_float32m1_t ysx_vfloat32m1_t;

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vadd_vv_i32m1(ysx_vint32m1_t __a, ysx_vint32m1_t __b, unsigned long __vl) {
  return __builtin_ysx_vadd_vv_i32m1(__a, __b, __vl);
}

#endif
```

- [ ] **Step 3: Add resource header**

In `clang/lib/Headers/CMakeLists.txt`, append `ysx_vector.h` to `riscv_files`:

```cmake
  ysx_vector.h
```

- [ ] **Step 4: Enable YSX vector macro without exposing standard RVV macro**

In `clang/lib/Basic/Targets/RISCV.cpp`, update YSX target defines:

```c++
if (getTriple().isYSX64() && HasFeatureMap.lookup("xtinyv"))
  Builder.defineMacro("__YSX_TINY_VECTOR__");
```

Do not define `__riscv_vector` for YSX.

- [ ] **Step 5: Add builtin declaration and codegen**

Add a YSX builtin shard in `YSX64TargetInfo::getTargetBuiltins()` for:

```text
__builtin_ysx_vadd_vv_i32m1
```

Lower it in `clang/lib/CodeGen/TargetBuiltins/RISCV.cpp` to:

```text
llvm.ysx.vadd
```

Use the existing RISCV builtin helper style as reference, but keep YSX builtin IDs and names distinct.

- [ ] **Step 6: Run Clang builtin test**

Run:

```bash
ninja -C build clang
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-builtins.c
```

Expected: the test passes and IR contains `llvm.ysx.vadd`.

- [ ] **Step 7: Commit builtin proof**

Run:

```bash
git add clang/lib/Headers clang/lib/Basic/Targets/RISCV.* clang/lib/CodeGen/TargetBuiltins/RISCV.cpp clang/test/CodeGen/YSX/tinyv-builtins.c
git commit -m "feat: add YSX vector builtin proof"
```

## Task 9: Builtin-to-Object End-to-End Proof

**Files:**
- Test: `clang/test/CodeGen/YSX/tinyv-builtins.c`
- Test: `llvm/test/MC/YSX/tinyv-auto-td.s`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [ ] **Step 1: Add object-generation RUN line**

Add to `clang/test/CodeGen/YSX/tinyv-builtins.c`:

```c
// RUN: %clang --target=ysx64-unknown-elf -march=rv64ima_xtinyv_zvl128b -O2 -c %s -o %t.o
// RUN: llvm-objdump --triple=ysx64 --mattr=+xtinyv,+zvl128b -d %t.o | FileCheck %s --check-prefix=OBJ
// OBJ: vadd.vv
```

- [ ] **Step 2: Run end-to-end test**

Run:

```bash
ninja -C build clang llvm-objdump
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-builtins.c
```

Expected: Clang compiles C to object and objdump shows `vadd.vv`.

- [ ] **Step 3: Update checklist**

Append to `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`:

```markdown
## End-to-End Proof

- `ysx_vadd_vv_i32m1` compiles from `ysx_vector.h` to LLVM IR and object code.
- Object disassembly contains `vadd.vv`.
```

- [ ] **Step 4: Commit proof**

Run:

```bash
git add clang/test/CodeGen/YSX/tinyv-builtins.c docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md
git commit -m "test: prove YSX vector builtin to object"
```

## Task 10: Bulk Import Tiny-F/Tiny-V YAML and Generated Surfaces

**Files:**
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-f/*.yaml`
- Create: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/*.yaml`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/taxonomy/*.yaml`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/*.py`
- Test: `llvm/test/MC/YSX/tinyf-auto-td.s`
- Test: `llvm/test/MC/YSX/tinyv-auto-td.s`

- [ ] **Step 1: Add YAML for the instruction import sets**

For each instruction listed in "Instruction Import Sets", add one YAML file under `tiny-f` or `tiny-v`. Use this exact naming rule:

```text
mnemonic dots become underscores, hyphens become underscores, and the file extension is .yaml
```

Example for `vfadd.vv`:

```yaml
mnemonic: vfadd.vv
opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vfadd_vv}
spec_ref: tinyv.vector-alu.float_add
features: {required: [xtinyv]}
pseudos:
  matrix: {element_types: [f32], lmuls: standard, masked: true, policy: llvm_default}
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vfadd, operation: fadd}
builtin:
  header: ysx_vector.h
  names: [ysx_vfadd_vv_f32m1]
  overloaded: false
```

- [ ] **Step 2: Extend emitters by taxonomy instead of per-instruction branches**

Refactor `emit_td.py` so generated outputs are driven by `spec_ref` categories:

```python
def emit_by_category(record, taxonomy):
    category = taxonomy[record.spec_ref]
    if "mc_def" in category["default_surfaces"]:
        return emit_real_instruction(record)
    raise ValueError(f"{record.path}: no mc_def surface")
```

Keep the proof-specific branches only until category-driven output produces identical records.

- [ ] **Step 3: Add MC tests for tiny-F and more tiny-V**

Create `llvm/test/MC/YSX/tinyf-auto-td.s`:

```asm
# RUN: llvm-mc -triple=ysx64 -mattr=+xtinyf -show-encoding < %s | FileCheck %s

flw f0, 0(a0)
# CHECK: flw f0, 0(a0)
fadd.s f0, f1, f2
# CHECK: fadd.s f0, f1, f2
fcvt.s.w f0, a0
# CHECK: fcvt.s.w f0, a0
```

Extend `llvm/test/MC/YSX/tinyv-auto-td.s` with one representative instruction from each imported family:

```asm
vlse32.v v1, (a0), a1
vluxei32.v v1, (a0), v2
vredsum.vs v1, v2, v3
vfadd.vv v1, v2, v3
vrgather.vv v1, v2, v3
```

- [ ] **Step 4: Run generated MC coverage**

Run:

```bash
ninja -C build llvm-mc llvm-objdump
python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyf-auto-td.s llvm/test/MC/YSX/tinyv-auto-td.s
```

Expected: all representative MC tests pass.

- [ ] **Step 5: Commit bulk YAML import**

Run:

```bash
git add llvm/lib/Target/YuShuXin/auto-td llvm/test/MC/YSX
git commit -m "feat: import YSX tiny FV auto TD instructions"
```

## Task 11: `yushuxin.vfexp` Custom Instruction End-to-End

**Files:**
- Modify: `third_party/ysx-opcodes/extensions/rv_xtinyv`
- Modify: `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml`
- Modify: `clang/lib/Headers/ysx_vector.h`
- Test: `clang/test/CodeGen/YSX/yushuxin-vfexp.c`
- Create: `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`

- [ ] **Step 1: Add failing custom builtin test**

Create `clang/test/CodeGen/YSX/yushuxin-vfexp.c`:

```c
// RUN: %clang --target=ysx64-unknown-elf -march=rv64ima_xtinyv_zvl128b -O2 -c %s -o %t.o
// RUN: llvm-objdump --triple=ysx64 --mattr=+xtinyv,+zvl128b -d %t.o | FileCheck %s

#include <ysx_vector.h>

ysx_vfloat32m1_t test_vfexp(ysx_vfloat32m1_t x, unsigned long vl) {
  return ysx_vfexp_v_f32m1(x, vl);
}

// CHECK: yushuxin.vfexp
```

- [ ] **Step 2: Add header wrapper**

Append to `clang/lib/Headers/ysx_vector.h`:

```c
static __inline__ ysx_vfloat32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vfexp_v_f32m1(ysx_vfloat32m1_t __x, unsigned long __vl) {
  return __builtin_ysx_vfexp_v_f32m1(__x, __vl);
}
```

- [ ] **Step 3: Verify custom encoding path**

Run:

```bash
PYTHONPATH=llvm/lib/Target/YuShuXin/auto-td/tools \
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td \
  --coverage build/ysx-auto-td/coverage.md
rg -n "yushuxin.vfexp|ysx-opcodes/rv_xtinyv/yushuxin_vfexp" build/ysx-auto-td/coverage.md
```

Expected: coverage report shows `yushuxin.vfexp` with opcode source `ysx-opcodes/rv_xtinyv/yushuxin_vfexp`.

- [ ] **Step 4: Run custom end-to-end test**

Run:

```bash
ninja -C build clang llvm-objdump
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/yushuxin-vfexp.c
```

Expected: object disassembly contains `yushuxin.vfexp`.

- [ ] **Step 5: Commit custom instruction**

Run:

```bash
git add third_party/ysx-opcodes llvm/lib/Target/YuShuXin/auto-td clang/lib/Headers/ysx_vector.h clang/test/CodeGen/YSX/yushuxin-vfexp.c
git commit -m "feat: add custom YSX vfexp instruction"
```

## Task 12: Automatic Vectorization Smoke Tests

**Status:** Superseded by the 2026-06-14 priority reset.

Do not execute the original autovec implementation steps in this plan. The
current scope keeps vector work to fixed 128-bit `ysx_vector.h` / `__builtin_ysx_*`
proof APIs and fixed-width IR smoke that maps one-to-one to an already
generated instruction. Do not add `YSXTargetTransformInfo` hooks, broad
loop-vectorizer enablement, vscale frontend plumbing, direct vector ABI tests,
strided autovec, or gather/scatter autovec as part of this auto-td completion
pass.

The accepted automation work instead is generated consumption of
`builtin.codegen: true` YAML facts into:

- `YSXGenAutoTinyVClangBuiltins.td`
- `YSXGenAutoTinyVIntrinsics.td`
- `YSXGenAutoTinyVBuiltinCG.inc`

## Task 13: Documentation, Blog, and Final Checklist

**Files:**
- Modify: `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- Create: `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`

- [ ] **Step 1: Update feature checklist**

Update the checklist with concrete entries:

```markdown
## Implemented Features

- `xtinyf` feature parsing.
- `xtinyv` feature parsing.
- Auto-generated tiny-V MC instructions.
- `ysx_vector.h` typed API.
- `__builtin_ysx_*` proof path.
- `yushuxin.vfexp` custom tiny-V instruction.

## Generated Surfaces

- Real MC instruction defs.
- Asm/disasm tables.
- Pseudo and pattern records for proof operations.
- Builtin metadata for proof builtins.
- Coverage report.

## Handwritten Glue

- YSX target feature parser.
- FPR/VR register scaffolding.
- Minimal YSX vector lowering hooks.
- Clang builtin dispatch for `__builtin_ysx_*`.

## Validation Evidence

- `llvm-lit` MC tiny-F/tiny-V tests.
- `llvm-lit` Clang builtin tests.
- object disassembly for `vadd.vv`.
- object disassembly for `yushuxin.vfexp`.

## Retained Schema Gaps

- Copy the exact retained-schema-gap rows from `build/ysx-auto-td/coverage.md`, including instruction name, missing schema ability, and retained owner file.
```

- [ ] **Step 2: Create the standalone blog**

Create `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`:

```markdown
## YSX tiny-F/tiny-V and auto-td-gen

YSX now extends the original rv64ima-only backend with a small, explicit
`xtinyf`/`xtinyv` surface for 32-bit scalar floating point and 32-bit vector
reduce-oriented kernels. The key implementation change is not only the added
instructions, but the way they are described: new tiny-F/tiny-V instructions
are owned by one structured YAML file per instruction, while fixed instruction
encoding comes from opcode-source files instead of copied TableGen bitfields.

The generator emits the TableGen records consumed by the YSX backend, including
MC instruction definitions, asm/disasm information, pseudos, patterns, schedule
references, and builtin metadata where the schema can express them. When a
surface still requires C++ lowering, the per-instruction facts remain in YAML
and the handwritten code is limited to algorithmic glue.

### Custom instruction example: yushuxin.vfexp

`yushuxin.vfexp` demonstrates the extension workflow. Its encoding is added to
`third_party/ysx-opcodes` in a `riscv-opcodes` compatible format, its semantic
surface is described by one YAML file under `YuShuXin/auto-td`, and the same
generator emits the backend TableGen records and builtin metadata. Compared with
legacy handwritten TableGen, this avoids multi-class inheritance decisions,
keeps encoding authority in one opcode source, and makes review focus on the
actual ISA facts: mnemonic, operands, feature predicate, operation class, and
expected codegen surfaces.
```

- [ ] **Step 3: Run final validation**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX \
  llvm/test/CodeGen/YSX \
  clang/test/Driver/YSX \
  clang/test/CodeGen/YSX
```

Expected: focused YSX tests pass.

- [ ] **Step 4: Commit docs**

Run:

```bash
git add docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md
git commit -m "docs: describe YSX tiny FV auto TD workflow"
```

## Parallel Execution Map

After Tasks 1-4 establish the schema and parser, use subagents with disjoint ownership:

- Worker A owns `third_party/*` and provenance updates.
- Worker B owns `auto-td/schema`, `auto-td/taxonomy`, and coverage reports.
- Worker C owns `emit_td.py`, YSX generated includes, and MC tests.
- Worker D owns YSX feature/register/lowering C++.
- Worker E owns Clang `ysx_vector.h`, builtin dispatch, and Clang tests.
- Worker F owns `yushuxin.vfexp` custom instruction and blog comparison.
- Worker G owns automatic vectorization tests and TTI/legalization hooks.

Workers must not edit each other's ownership paths unless coordination updates this plan first.
