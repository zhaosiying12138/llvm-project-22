# YSX Tiny-F/Tiny-V Auto TD Design

## Goal

Extend the standalone YuShuXin `ysx64` backend beyond its current `rv64ima`
surface with a tiny 32-bit floating-point and vector subset for CCU-oriented
local reduce kernels.

The implementation must not add new handwritten instruction TableGen for the
new tiny-F/tiny-V surface. New instructions are described by structured YAML
under the YSX backend, get their encoding from read-only opcode repositories,
and are emitted into build-tree generated TableGen includes.

The first implementation route is schema-first: build enough generator schema,
taxonomy, pseudo, pattern, and builtin generation support before bulk importing
the selected tiny-F/tiny-V instruction set. A small end-to-end proof slice still
acts as a mandatory acceptance gate after the schema exists.

## Current Branch Status

This worktree implements the schema-first foundation, a bulk generated tiny-F
and tiny-V MC surface, and a selected C builtin to object-code proof slice. It
does not yet complete automatic vectorization or a general tiny-V C ABI.

Completed in this branch:

- pinned `riscv-opcodes` and `riscv-isa-manual` snapshots plus YSX-owned
  `third_party/ysx-opcodes`
- structured YAML schema, taxonomy, validation, coverage reporting, and
  build-tree generated TableGen outputs
- real generated tiny-F MC records for the selected 32-bit scalar FP load,
  store, add/sub/mul, compare, convert, sign-inject copy, and GPR/FPR bit-move
  instructions
- real generated tiny-V MC records for 35 selected e32 memory, integer ALU,
  reduction, shuffle/move, `vset*`, and custom `yushuxin.vfexp` instructions
- generated audit manifests for YAML-declared tiny-V pseudo, pattern, and
  builtin facts, including `yushuxin.vfexp`
- YSX feature plumbing for `xtinyf`, `xtinyv`, and `zvl128b`
- minimal FPR/VR/register/mask scaffolding and MC glue for generated tiny-v
  asm, encoding, disassembly, and optional `v0.t`
- `ysx_vector.h` proof APIs and Clang target builtins for `vadd`, `vsub`,
  `vmul`, integer `vredsum`, float `vfredsum`, `vrgather`, `vslideup`, and
  custom `vfexp`
- a separate Clang `BuiltinsYSX.td` shard so YSX builtins do not inherit the
  RISCV/RVV builtin declaration table
- minimal backend selection for the proof path: fixed 128-bit `v4i32` and
  `v4f32` values select to generated `vle32.v`, `vse32.v`, `vsetvli`,
  `vsetivli`, `vadd.vv`, `vsub.vv`, `vmul.vv`, `vredsum.vs`,
  `vfredusum.vs`, `vrgather.vv`, `vslideup.vx`, and `yushuxin.vfexp`
  instruction records
- tiny-F CodeGen for f32 load/store, add/sub/mul, ordered eq/lt/le/gt/ge
  comparisons, i32/f32 conversions, stack spill/reload, and soft-float ABI
  GPR/FPR moves through generated `fmv.x.w` and `fmv.w.x`
- basic VR copy/spill and O0 frame-index support for the fixed-width proof
  path through generated `vmv1r.v`, `vsetivli`, `vle32.v`, and `vse32.v`
- targeted C-to-object lit proof plus directory-level YSX MC/CodeGen/Driver and
  Clang CodeGen lit validation

Known remaining work:

- wire the generated builtin/pattern manifests into generated Clang builtin and
  LLVM intrinsic TableGen consumption
- keep any automatic-vectorization work to minimal one-to-one smoke that maps
  C API or IR operations to existing generated instructions
- broaden backend lowering beyond the fixed-width proof vector types, selected
  builtins, and zero-offset proof vector loads/stores
- direct C ABI passing/returning of tiny-v vector values; the proven C path
  keeps vector values inside explicit builtin functions and stores results to
  memory
- full unordered f32 compares and true i64/f32 conversions; the current tiny-F
  subset rejects those cases instead of selecting incorrect 32-bit operations

## Workspace

Work is isolated in this worktree:

```text
/home/zhaosiying/.config/superpowers/worktrees/llvm-project-22.1.3-ysx/ysx-tiny-fv-ccu
```

The worktree was created from:

```text
f5fd71c011926e556dcec6411a837493eac74638
```

All build directories for this task must live inside this worktree. The primary
build directory is:

```text
build/
```

## Existing YSX Baseline

The current backend is intentionally restricted to `ysx64` + `rv64ima` + `lp64`.
It rejects RV32, floating point, compressed instructions, vector instructions,
and inherited RISC-V frontend/vector surfaces.

This project intentionally relaxes that boundary only for the new tiny-F and
tiny-V features described here. Existing `rv64ima` handwritten TD is not
migrated in this first stage.

## Confirmed Functional Scope

### Overall CodeGen Goal

The priority is CodeGen, not MC-only support. The target must support a Clang
and LLVM path for the new instructions.

The first user-facing path is explicit YSX vector builtins:

```text
ysx_vector.h / __builtin_ysx_* -> LLVM IR/intrinsic -> DAG/pseudo -> asm -> object
```

Automatic vectorization is only a later consumer after the same legal type,
pseudo, pattern, and lowering infrastructure is available. It is not part of
the current acceptance gate unless it is a one-to-one smoke from C API or
fixed-width IR to an already generated instruction.

### Feature Surface

The public feature names are:

```text
xtinyf
xtinyv
```

YSX does not claim support for full standard `f` or `v`.

Standard F/V instruction encodings are read from upstream `riscv-opcodes`
entries such as `rv_f` and `rv_v`, then mapped to YSX feature predicates such as
`HasStdExtXTinyF` and `HasStdExtXTinyV`.

Custom YSX instruction encodings are read from a separate
`third_party/ysx-opcodes` repository snapshot that uses a `riscv-opcodes`
compatible format. Custom YSX instructions may still belong semantically to
`xtinyv`.

### Tiny-F

Tiny-F keeps the existing `lp64` soft-float ABI. Function arguments and return
values keep using the soft-float ABI; f32 registers and f32 instructions may be
used inside generated function bodies.

Initial tiny-F operations:

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
- `fsgnj.s`
- `fmv.x.w`
- `fmv.w.x`

Explicitly out of scope for the first stage:

- f64 and `D`
- div/sqrt
- FMA
- `lp64f` or `lp64d`
- unordered compare lowering that requires NaN classification
- true i64/f32 conversions

### Tiny-V

Tiny-V is designed to remain compatible with future scalable vector semantics,
but the current proof path uses fixed 128-bit m1 C API and IR values. Any
future RVV-like VLEN/LMUL expression, including LMUL and fractional LMUL,
remains outside the current completion pass. Legal data elements for the proof
surface are restricted to:

- i32
- f32
- i1 mask

Implementation note: the first proof C API intentionally uses fixed 128-bit
Clang extended vectors (`<4 x i32>` and `<4 x float>`) inside store-shaped
functions so the branch can prove YSX-owned builtins, instruction selection,
assembly, and object emission without exposing the standard RVV frontend type
system or committing to a vector C ABI. The scalable-vector design remains the
direction for the broader tiny-v import.

Initial tiny-V memory operations:

- e32 unit-stride load/store
- e32 strided load/store
- e32 unordered indexed gather/scatter
- masked and unmasked forms

Initial tiny-V ALU operations:

- add
- sub
- mul
- min
- max
- compare
- merge/select
- i32 bitwise and/or/xor

Initial tiny-V reductions:

- sum
- min
- max
- and
- or
- xor
- unordered f32 sum/min/max at the MC layer

Initial tiny-V shuffle/permute operations:

- slide
- splat/broadcast
- `vrgather`
- register moves

Mask/tail policy generation is limited to the LLVM codegen default policy
surface needed by this implementation. The first stage does not attempt a full
`ta/tu/ma/mu` test matrix.

### CCU Boundary

The first stage implements ordinary local vector reduce instructions and does
not define a CCU communication instruction or synchronization model. CCU-specific
multi-die communication intrinsics can be designed later on top of this local
tiny-V foundation.

## Auto TD Requirements

### Scope

The first stage auto-generates only the new tiny-F and tiny-V instruction
surface. Existing `rv64ima` YSX handwritten TD remains in place.

Generated TD is build-tree output and must not be committed.

The committed source of truth is:

- YSX-owned instruction YAML
- YSX-owned taxonomy YAML
- generator code
- read-only opcode snapshots
- provenance files

### Directory Layout

```text
llvm/lib/Target/YuShuXin/auto-td/
  instructions/
    tiny-f/
      fadd_s.yaml
      flw.yaml
    tiny-v/
      vadd_vv.yaml
      vle32_v.yaml
      yushuxin_vfexp.yaml
  taxonomy/
    scalar-float.yaml
    vector-memory.yaml
    vector-alu.yaml
    vector-reduce.yaml
    vector-shuffle.yaml
  schema/
    instruction.schema.yaml
    taxonomy.schema.yaml
  tools/
    ysx_auto_td_gen.py
```

Third-party snapshots:

```text
third_party/riscv-opcodes/
third_party/riscv-isa-manual/
third_party/ysx-opcodes/
third_party/YSX_THIRD_PARTY_PROVENANCE.md
```

### Third-Party Policy

`third_party/riscv-opcodes` and `third_party/riscv-isa-manual` are pinned
snapshots. They must not be modified by this project.

`third_party/riscv-opcodes` is the encoding and field authority for standard
F/V instructions.

`third_party/riscv-isa-manual` is used only for provenance, version, and section
references. The generator must not infer instruction semantics from prose.

`third_party/ysx-opcodes` is a YSX-owned opcode source for custom instructions.
It follows the `riscv-opcodes` file format so the same parser path can be used.

### YAML Ownership

Each generated instruction has exactly one instruction YAML file, keyed by ISA
mnemonic / opcode source key. That one file owns the real instruction and its
derived codegen surfaces: pseudos, mask/policy/type variants, patterns,
builtin mapping, and coverage ownership.

YAML must remain structured. It must not contain:

- raw TD
- TableGen AST
- `def : Pat`
- handwritten bit slices copied from opcodes
- raw C++
- old RISC-V helper class names such as `RVInst*`, `VUnitStrideLoad`, `VRED_*`,
  or `VPseudo*`

Allowed structured content includes:

- `mnemonic`
- `opcode_source`
- `spec_ref`
- semantic operation class
- operands and roles
- effects
- feature predicates
- schedule class
- pseudo matrix rules
- pattern mappings
- builtin/header metadata
- parser/decoder hook names
- custom lowering hook names
- explicit `retained_schema_gap`

### Example YAML Shape

```yaml
mnemonic: vadd.vv
opcode_source:
  repo: riscv-opcodes
  extension: rv_v
  key: vadd_vv
spec_ref: tinyv.vector-alu.add

features:
  required: [xtinyv]

builtin:
  header: ysx_vector.h
  names: [ysx_vadd_vv_i32m1]
  overloaded: false

pseudos:
  matrix:
    element_types: [i32]
    lmuls: standard
    masked: true
    policy: llvm_default

patterns:
  - kind: intrinsic_to_pseudo
    intrinsic: ysx.vadd
  - kind: vvl_node_to_pseudo
    operation: add
```

### Generated Surfaces

The schema-first implementation targets generation of:

- real MC instruction defs
- operand encoding and decoder fields
- asm matcher and asm writer inputs
- disassembler inputs
- basic schedule references
- audit manifests for RVV-like pseudo/type/mask/policy matrices
- audit manifests for SelectionDAG pattern intent where structurally
  expressible
- audit manifests for `ysx_vector.h` and `__builtin_ysx_*` builtin metadata
- coverage and ownership reports

The current branch consumes generated real instruction records directly. The
pseudo/pattern/builtin files are generated as stable comment manifests rather
than active Clang/LLVM builtin definitions. Necessary C++ lowering, ISel,
DAG-to-DAG, and Clang glue can be handwritten, but only when the behavior is
algorithmic or the current build cannot yet consume that generated surface.
Per-instruction facts must stay in YAML.

### Build Integration

CMake runs `ysx_auto_td_gen.py` before the relevant TableGen invocations.

The YSX top-level TableGen includes build-directory generated files such as:

```text
YSXGenAutoTinyFInstrInfo.inc
YSXGenAutoTinyVInstrInfo.inc
YSXGenAutoTinyVPseudos.inc
YSXGenAutoTinyVPatterns.inc
YSXGenAutoTinyVBuiltins.inc
```

Generated includes must be active inputs. Missing generated includes should make
the build fail rather than allowing legacy handwritten TD to silently cover the
new surface.

### Coverage and Gaps

The generator emits a coverage report that classifies every instruction and
surface as one of:

- `auto_full`
- `auto_with_structured_override`
- `retained_schema_gap`

`retained_schema_gap` is allowed for future non-proof instructions, but it must
name the missing schema ability and retained owner. Silent gaps are forbidden.

The first end-to-end proof instructions cannot have retained schema gaps.

## C Intrinsics and Header Surface

The first public C API is `ysx_vector.h`.

The naming style is RVV-like but YSX-prefixed:

```text
ysx_vint32m1_t
ysx_vfloat32m1_t
ysx_vle32_v_i32m1
ysx_vadd_vv_i32m1
```

The project must not expose `riscv_vector.h` or define `__riscv_vector` for
this tiny-V surface.

The first required MC/generated proof slice is:

- `vle32.v`
- `vse32.v`
- `vlse32.v`
- `vsse32.v`
- `vluxei32.v`
- `vsuxei32.v`
- `vsetivli` / `vsetvli` inserted around proof memory and ALU operations
- `vadd.vv`
- `vsub.vv`
- `vmul.vv`
- `vmin.vv`
- `vmax.vv`
- `vand.vv`
- `vor.vv`
- `vxor.vv`
- `vmseq.vv`
- `vmslt.vv`
- `vmerge.vvm`
- canonical `vfredusum.vs`, plus public alias `vfredsum.vs`
- integer reductions `vredsum/min/max/and/or/xor.vs`
- float reductions `vfredmin/max.vs`
- `vrgather.vv`
- `vslideup.vx`
- `vslidedown.vx`
- `vmv.v.x`
- `vmv.v.v`
- custom `yushuxin.vfexp`

The C-to-object smoke tests cover store-shaped `ysx_vadd_vv_i32m1`,
`ysx_vsub_vv_i32m1`, `ysx_vmul_vv_i32m1`, `ysx_vredsum_vs_i32m1`,
`ysx_vfredsum_vs_f32m1`, `ysx_vrgather_vv_i32m1`,
`ysx_vslideup_vx_i32m1`, and `ysx_vfexp_v_f32m1` proof APIs.

## Automatic Vectorization

Automatic vectorization is a second-stage consumer of the same tiny-V legal
types, pseudo matrix, patterns, and cost model.

For the current completion pass, automatic vectorization is deliberately reduced
to minimal smoke only. A test is useful only when it proves a C API or
fixed-width IR operation maps one-to-one to already generated instructions. It
must not become a vscale frontend ABI or full RVV autovec project.

Longer-term intended scope, outside the current completion pass and not an
actionable task in this branch, is:

- future contiguous loop-vectorizer experiments
- future strided loop-vectorizer experiments
- future gather/scatter loop-vectorizer experiments

Automatic vectorization should not be used as the first debug path. The first
debug path is explicit `ysx_vector.h` builtin code.

## Custom Instruction Demonstrator

The project must add a custom YSX instruction:

```text
yushuxin.vfexp
```

It belongs semantically to tiny-V / `xtinyv`.

Its encoding is defined in `third_party/ysx-opcodes`, using a format compatible
with upstream `riscv-opcodes`. The encoding must not conflict with selected
standard tiny-F/tiny-V encodings.

The instruction is implemented through the same auto-td-gen path:

1. Add an opcode entry to `third_party/ysx-opcodes`.
2. Add one instruction YAML under `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/`.
3. Add or reuse taxonomy for vector f32 unary math.
4. Generate the MC def, asm/disasm, pseudo/pattern, and builtin metadata.
5. Check the generated pseudo/pattern/builtin manifests so C API facts did not
   silently disappear.
6. If a C API is needed and the shape is supported, set `builtin.codegen: true`
   so the Clang builtin TD, LLVM intrinsic TD, and CGBuiltin dispatch fragment
   are generated.
7. Add only the public `ysx_vector.h` wrapper and backend selector smoke still
   needed around the generated Clang/LLVM builtin consumption.

The final blog must be written as a new standalone document:

```text
docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md
```

It must include a dedicated section showing this process step by step and
comparing it with the legacy handwritten TableGen workflow.

## Documentation Requirements

The spec file is the design authority. If goals or implementation scope change,
this spec must be updated in the same worktree.

The implementation must maintain a concise feature checklist for final blog
writing. The final checklist should distinguish:

- implemented features
- generated surfaces
- handwritten C++/Clang glue that remains
- tests passed
- known retained schema gaps

The standalone blog document should be high-level and concise, with a separate
section for `yushuxin.vfexp` and the auto-td-gen extension workflow. It should
argue concretely that this framework:

- avoids multi-class handwritten TableGen inheritance for new instructions
- keeps encoding authority in opcode files instead of copied bitfields
- keeps per-instruction knowledge in one structured YAML file
- reduces drift between asm, disasm, pattern, pseudo, builtin, and tests
- makes custom instruction extension auditable and easier to review

## Validation Strategy

### Generator

- Reject raw TD, TD AST, raw C++, copied encoding bit slices, unknown opcode
  source keys, and silent gaps.
- Parse pinned `riscv-opcodes` and `ysx-opcodes` entries into fixed bits and
  variable fields.
- Generate stable snapshot outputs for representative tiny-F/tiny-V YAML.
- Emit coverage reports.

### TableGen and Build

- YSX-only build consumes generated includes.
- Combined RISCV+YSX build has no target init, option, or generated record
  conflicts.
- Missing generated includes fail the build.

### MC

- `llvm-mc` assembles generated tiny-F/tiny-V instructions.
- `llvm-objdump` disassembles them consistently.
- Object encodings match opcode-source authority.

### Builtin End-to-End

- C source using `ysx_vector.h` compiles with `--target=ysx64`.
- Clang lowers selected `__builtin_ysx_*` operations.
- `llc` generates YSX assembly.
- Object output disassembles to expected tiny-V instructions.

### Automatic Vectorization

- Do not add broad automatic vectorization in the current completion pass.
- Future smoke in this pass may cover only C API or fixed-width IR when it maps
  one-to-one to already generated tiny-V instructions.
- Contiguous, strided, and gather/scatter loop-vectorizer coverage remains
  future work after the explicit builtin path and generated metadata are stable.

## Multi-Agent Implementation Strategy

Use parallel agents only after the implementation plan has disjoint work scopes.

Likely parallel workstreams:

- third-party snapshots, provenance, and opcode parser
- YAML schema, taxonomy, and coverage report
- real MC TD emitter and TableGen integration
- RVV-like pseudo and SelectionDAG pattern emitter
- Clang builtin and `ysx_vector.h` plumbing
- minimal C++ lowering and DAG-to-DAG import from RISCV
- tests and validation harness

Critical dependencies:

- schema and opcode parsing precede all emitters
- real MC emitter precedes pseudo/pattern emitter
- builtin end-to-end depends on Clang glue, generated pseudos/patterns, and C++
  lowering
- automatic vectorization follows the explicit builtin proof

## Accepted Route

The selected route is schema-first:

1. Build schema, taxonomy, parser, emitter, coverage, and build integration.
2. Statistically classify representative tiny-F/tiny-V instruction YAML.
3. Bulk import the selected tiny-F/tiny-V MC set.
4. Prove scalar tiny-F load/store/arithmetic/compare/conversion and soft-float
   ABI moves through LLVM CodeGen.
5. Prove selected tiny-V instructions through explicit C builtins to
   assembly/object, including `vadd`, `vsub`, `vmul`, reductions, shuffle, and
   custom `yushuxin.vfexp`.
6. Extend to automatic vectorization tests in a later stage.
