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

Automatic vectorization is enabled after the same legal type, pseudo, pattern,
and lowering infrastructure is available.

### Feature Surface

The public feature names are:

```text
xtinyf
xtinyv
```

YSX does not claim support for full standard `f` or `v`.

Standard F/V instruction encodings are read from upstream `riscv-opcodes`
entries such as `rv_f` and `rv_v`, then mapped to YSX feature predicates such as
`HasExtXTinyF` and `HasExtXTinyV`.

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
- f32 compare instructions needed for C/IR comparisons
- i32/f32 conversion instructions such as `fcvt.w.s` and `fcvt.s.w`

Explicitly out of scope for the first stage:

- f64 and `D`
- div/sqrt
- FMA
- `lp64f` or `lp64d`

### Tiny-V

Tiny-V uses scalable vector semantics. It supports standard RVV-like VLEN/LMUL
expression, including LMUL and fractional LMUL, but legal data elements are
restricted to:

- i32
- f32
- i1 mask

Initial tiny-V memory operations:

- e32 unit-stride load/store
- e32 strided load/store
- e32 indexed gather/scatter
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

Initial tiny-V shuffle/permute operations:

- slide
- splat/broadcast
- scalar insert/extract
- `vrgather`

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
  name: ysx_vadd_vv_i32m1
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
- RVV-like pseudo/type/mask/policy matrices
- SelectionDAG patterns where structurally expressible
- builtin metadata for `ysx_vector.h` and `__builtin_ysx_*`
- coverage and ownership reports

Necessary C++ lowering, ISel, DAG-to-DAG, and Clang glue can be handwritten, but
only when the behavior is algorithmic or not reasonably representable as
structured data. Per-instruction facts must stay in YAML.

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

The first required end-to-end proof slice is:

- `vle32.v`
- `vse32.v`
- `vadd.vv`
- `vfredsum.vs`

These are the smoke tests for builtin-to-object correctness after the route-2
schema is implemented.

## Automatic Vectorization

Automatic vectorization is a second-stage consumer of the same tiny-V legal
types, pseudo matrix, patterns, and cost model.

The intended scope is:

- contiguous loops
- strided loops
- gather/scatter loops

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
5. Add the minimal C++ lowering glue only if the operation needs custom lowering.
6. Add `ysx_vector.h` API and end-to-end tests.

The final blog must include a dedicated section showing this process step by
step and comparing it with the legacy handwritten TableGen workflow.

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

The final `blog.md` update should be high-level and concise, with a separate
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

- After builtin proof, add smoke tests for contiguous, strided, and
  gather/scatter loops.
- Verify the vectorizer only uses legal tiny-V element types and operations.

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
3. Bulk import the selected tiny-F/tiny-V set.
4. Prove `vle32.v`, `vse32.v`, `vadd.vv`, and `vfredsum.vs` end to end.
5. Add and document `yushuxin.vfexp`.
6. Extend to automatic vectorization tests.

