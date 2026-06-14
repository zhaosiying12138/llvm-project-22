# YSX tiny-F/tiny-V Auto TD

YuShuXin started as a deliberately small `ysx64` target: `rv64ima`, `lp64`,
and no inherited floating-point or vector frontend surface. This branch adds a
CCU-oriented tiny-F/tiny-V path while keeping that boundary explicit.

The central change is that new instruction facts are no longer hand-expanded
into TableGen classes. Each instruction has one structured YAML file under
`llvm/lib/Target/YuShuXin/auto-td/instructions/`. Standard encodings are parsed
from pinned `third_party/riscv-opcodes`; custom YSX encodings are parsed from
the compatible `third_party/ysx-opcodes`. The generator emits build-tree
TableGen includes consumed by the YSX target.

## Feature Summary

- `xtinyf`, `xtinyv`, and `zvl128b` feature plumbing for YSX.
- 50 generated `auto_full` instructions: 15 tiny-F and 35 tiny-V, with 0
  retained schema gaps.
- Tiny-F scalar f32 MC and CodeGen support for load/store, add/sub/mul,
  ordered comparisons, i32/f32 conversions, FPR32 spills/reloads, and
  soft-float ABI moves through generated `fmv.x.w`/`fmv.w.x`.
- Tiny-V generated MC coverage for e32 unit-stride, strided, and indexed
  memory; integer ALU; comparisons; merge; reductions; slide/gather/move; and
  `vsetvli`/`vsetivli`.
- `ysx_vector.h` and `__builtin_ysx_*` proof APIs for `vadd`, `vsub`, `vmul`,
  integer `vredsum`, float `vfredsum`, `vrgather`, `vslideup`, and custom
  `vfexp`.
- Fixed 128-bit C-to-object proof path for selected `<4 x i32>` and
  `<4 x float>` builtins, emitting generated `vset*`, `vle32.v`, `vse32.v`,
  ALU/reduce/shuffle records, and `yushuxin.vfexp`.
- Generated pseudo, pattern, and builtin manifests for YAML-declared tiny-V
  facts, so C API intent is visible to tests even before Clang builtin and LLVM
  intrinsic definitions are fully generated.
- Strict decoder handling for `vmerge.vvm` carry-in masks, so invalid mask
  encodings are rejected instead of decoded as `NoRegister`.

## Why Auto TD

Legacy RISC-V TableGen is powerful, but adding one instruction often means
choosing a class hierarchy, copying fixed bit fields, spreading operands,
scheduling, pseudo records, aliases, and patterns across multiple files, and
then adding separate builtin/header glue.

YSX auto-td keeps the facts closer to their authority:

- opcode files own encoding bits
- taxonomy owns common operand/effect shapes
- one YAML file owns each instruction's mnemonic, source key, feature, and
  semantic category
- generated TD owns the mechanical record shape
- C++ stays reserved for behavior that is actually algorithmic

This avoids multi-class inheritance as a prerequisite for adding instructions.
Review becomes checking data provenance and shared generator rules instead of
auditing repeated handwritten bit slices.

## What Is Automatic Now

For a new instruction in the current slice, the real instruction TableGen record
is automatic. The generator reads the opcode source, attaches taxonomy
operands/effects, and emits the `YSX_AUTO_*` record consumed by MC asm,
encoding, disassembly, and backend selection.

The generator also emits manifest lines for YAML-declared pseudo, pattern, and
builtin facts. That is an intentional guard: if an instruction says it has a C
API or intrinsic mapping, the generated files and unit tests show that fact.
The final Clang builtin declaration, LLVM intrinsic declaration, and CGBuiltin
lowering are still bounded handwritten glue in this branch. They are now
isolated as the next automation step instead of being mixed with instruction
encoding TableGen.

## Custom Instruction: yushuxin.vfexp

`yushuxin.vfexp` demonstrates how a custom instruction is added without
forking the framework.

1. Add a compatible opcode entry:

```text
third_party/ysx-opcodes/extensions/rv_xtinyv
yushuxin.vfexp 31..26=0x2a vm vs2 19..15=0 14..12=0x1 vd 6..0=0x0b
```

2. Add one instruction YAML:

```text
llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml
```

The YAML points at `ysx-opcodes/rv_xtinyv/yushuxin_vfexp`, marks it as
`xtinyv`, and assigns the vector f32 unary taxonomy category.

3. Run `ysx_auto_td_gen.py`.

The generator emits `YSX_AUTO_YUSHUXIN_VFEXP`, with operands and fixed bits
derived from the same parser path used for upstream `riscv-opcodes`.

4. Check generated manifests.

The same YAML also declares:

```yaml
patterns:
  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vfexp, operation: fexp}
builtin:
  header: ysx_vector.h
  names: [ysx_vfexp_v_f32m1]
  overloaded: false
```

The generator records those facts in `YSXGenAutoTinyVPatterns.inc` and
`YSXGenAutoTinyVBuiltins.inc`. Unit tests require those manifests to be
non-empty for instructions that declare the fields.

5. Add only bounded C++/Clang glue.

For this proof, the handwritten pieces are reusable or currently not generated:
vector register decode, mask parse/print/encode, fixed-width vector
SelectionDAG handling, frame-index memory handling, Clang builtin declaration,
LLVM intrinsic declaration, and builtin-to-intrinsic lowering. The instruction's
encoding, operand facts, and C API intent stay in YAML and opcode source.

6. Expose and test the C API.

`ysx_vfexp_v_f32m1` calls `__builtin_ysx_vfexp_v_f32m1`, which lowers to
`llvm.ysx.vfexp`. The backend selects it to generated
`YSX_AUTO_YUSHUXIN_VFEXP`, inserts generated `vsetvli` for the requested `vl`,
and validates assembly/object output with `llvm-objdump`.

## Adding The Next Instruction

For a future `yushuxin.vexp`-style instruction:

1. Add the encoding to `third_party/ysx-opcodes/extensions/rv_xtinyv`, using the
   same `riscv-opcodes` field syntax.
2. Add one YAML file such as
   `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vexp.yaml`.
3. Point `opcode_source` at `ysx-opcodes/rv_xtinyv/yushuxin_vexp`, set the
   feature requirement, and choose or add the taxonomy category.
4. If the instruction has a C API, put the builtin name and intrinsic/pattern
   intent in the YAML.
5. Run `ysx_auto_td_gen.py` and check the generated `YSX_AUTO_YUSHUXIN_VEXP`
   instruction plus the pseudo/pattern/builtin manifests.
6. Add MC asm/object tests for the generated instruction.
7. Add the smallest C API glue and a C-to-ASM/object smoke only when the
   instruction needs a C API before generated Clang/LLVM builtin consumption is
   implemented.

The important improvement is that adding the instruction no longer starts by
choosing a TableGen inheritance stack or copying bit slices. That complexity is
centralized in the generator.

## Verification

The final checks passed in the isolated worktree:

- `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py ... --out-dir build/ysx-auto-td-current`
- manifest checks for `auto-td-pattern` and `auto-td-builtin`, including
  `ysx_vfexp_v_f32m1`
- focused lit tests for tiny-F MC, tiny-V MC, invalid `vmerge` disassembly,
  tiny-F CodeGen/ABI/unsupported boundaries, tiny-V builtin ISel, and Clang
  C-to-object builtins
- `git diff --check`

The generator coverage reports `auto_full: 50`,
`auto_with_structured_override: 0`, and `retained_schema_gap: 0`.

## Current Boundary

This branch does not claim full standard `F` or `V`. It claims YSX-owned
`xtinyf` and `xtinyv` subsets. Tiny-F keeps the `lp64` soft-float ABI. Tiny-V
does not yet expose generic RVV frontend types or direct C vector ABI
passing/returning; the proven C path keeps vector values inside explicit
`ysx_vector.h` builtin functions and stores results to memory.

Automatic vectorization is also left for the next layer. The current work gives
that future work a generated instruction base, explicit builtin proof paths,
and locked-down tests for the boundaries that are not supported yet. Any vector
smoke added in this branch should stay at the level of one-to-one mapping from
C API or IR to generated instructions, not a full RVV autovec claim.
