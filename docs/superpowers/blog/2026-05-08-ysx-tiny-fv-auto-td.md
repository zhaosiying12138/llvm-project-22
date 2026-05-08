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

4. Add only shared C++ glue.

For this proof, the handwritten pieces are reusable: vector register decode,
mask parse/print/encode, fixed-width vector SelectionDAG handling, and
frame-index memory handling. The instruction's encoding and operand facts stay
in YAML and opcode source.

5. Expose and test the C API.

`ysx_vfexp_v_f32m1` calls `__builtin_ysx_vfexp_v_f32m1`, which lowers to
`llvm.ysx.vfexp`. The backend selects it to generated
`YSX_AUTO_YUSHUXIN_VFEXP`, inserts generated `vsetvli` for the requested `vl`,
and validates assembly/object output with `llvm-objdump`.

## Verification

The final checks passed in the isolated worktree:

- `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py ... --out-dir build/ysx-auto-td-current`
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
and locked-down tests for the boundaries that are not supported yet.
