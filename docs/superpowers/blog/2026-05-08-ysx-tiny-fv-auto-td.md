# YSX tiny-F/tiny-V Auto TD

YuShuXin started from a deliberately small `ysx64` target: `rv64ima`, `lp64`,
and no inherited floating-point or vector frontend surface. This branch adds the
foundation for a CCU-oriented tiny-F/tiny-V path without copying the legacy
RISC-V TableGen hierarchy into YSX.

The central change is ownership. Each new instruction is described once in a
structured YAML file under `llvm/lib/Target/YuShuXin/auto-td/instructions/`.
Standard encodings come from the pinned upstream `third_party/riscv-opcodes`
snapshot. YSX custom encodings come from the compatible
`third_party/ysx-opcodes` source. The generator combines those inputs and emits
build-tree TableGen includes for the YSX target.

## What Exists Now

- `xtinyf`, `xtinyv`, and `zvl128b` feature plumbing for YSX.
- Build-tree auto-td generation wired into `llvm/lib/Target/YuShuXin/CMakeLists.txt`.
- Real generated MC records for `vadd.vv`, `vle32.v`, `vse32.v`,
  `vsetivli`, `vsetvli`, `vmv1r.v`, `vfredusum.vs`, `vfredsum.vs`, and
  `yushuxin.vfexp`.
- Minimal vector register, mask, printer, encoder, and decoder glue for the
  generated tiny-v instructions.
- `ysx_vector.h` proof APIs for `ysx_vadd_vv_i32m1` and
  `ysx_vfexp_v_f32m1`.
- YSX-prefixed Clang builtins lowered to `llvm.ysx.vadd` and `llvm.ysx.vfexp`.
- A separate Clang `BuiltinsYSX.td` shard, so YSX does not inherit the RISCV/RVV
  builtin namespace.
- A fixed 128-bit proof backend path from C builtins through assembly and object
  disassembly for `vsetivli`, `vsetvli`, `vle32.v`, `vse32.v`, `vadd.vv`, and
  `yushuxin.vfexp`.
- Basic O0 frame-index and VR copy/spill support for the proof vector type via
  generated `vsetivli`, `vle32.v`, `vse32.v`, and `vmv1r.v`.

This is a proof slice, not the complete tiny-F/tiny-V import. The current
branch proves the framework and two explicit builtins; broader tiny-F/tiny-V
coverage, automatic vectorization, and direct C vector ABI passing/returning
remain future work.

## Why Auto TD

Legacy TableGen usually asks the target author to pick or invent a chain of
classes: instruction format classes, pseudo classes, scheduling classes, policy
variants, pattern fragments, and aliases often live in different files. That
structure is powerful, but it spreads one instruction's facts across many
places.

The YSX auto-td flow keeps the ISA facts together:

- mnemonic and feature predicate live in one YAML file
- encoding comes from opcode source, not copied bit slices
- operand roles and effects come from taxonomy
- generated asm/disasm and MC records use the same parsed fields
- coverage reports show which surfaces are generated and which still need
  handwritten glue

That means adding an instruction is mostly data entry plus shared generator
logic. Reviewers look at the mnemonic, opcode source key, semantic category,
operands, and expected surfaces instead of auditing a hand-expanded TableGen
inheritance tree.

## Custom Instruction: yushuxin.vfexp

`yushuxin.vfexp` demonstrates the extension workflow.

1. Add a compatible opcode-source entry:

```text
third_party/ysx-opcodes/extensions/rv_xtinyv
yushuxin.vfexp 31..26=0x2a vm vs2 19..15=0 14..12=0x1 vd 6..0=0x0b
```

2. Add one instruction YAML:

```text
llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml
```

That YAML names the mnemonic, opcode source, tiny-v feature requirement,
semantic category, pseudo/pattern intent, and `ysx_vector.h` builtin name.

3. Run `ysx_auto_td_gen.py`.

The generator parses the opcode fields, resolves register operands through the
taxonomy, emits `YSX_AUTO_YUSHUXIN_VFEXP`, and records coverage showing the
source as `ysx-opcodes/rv_xtinyv/yushuxin_vfexp`.

4. Add shared glue only where the schema cannot own behavior.

For this proof, the reusable C++ glue is mask parsing/printing/encoding and
vector register decode, plus narrow SelectionDAG and frame-index handling for
the fixed 128-bit proof path. The per-instruction facts remain in YAML and
opcode source files.

5. Expose the proof C API.

`ysx_vfexp_v_f32m1` in `ysx_vector.h` calls
`__builtin_ysx_vfexp_v_f32m1`, which lowers to the `llvm.ysx.vfexp` IR
intrinsic. The proof backend then selects that intrinsic to the generated
`YSX_AUTO_YUSHUXIN_VFEXP` record, inserts generated `vsetvli` for the requested
`vl`, and validates the final object with `llvm-objdump`.

## Verification

The final checks passed in the isolated worktree:

- `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py ... --out-dir build/ysx-auto-td-final7`
- `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyv-auto-td.s clang/test/CodeGen/YSX/tinyv-builtins.c clang/test/CodeGen/YSX/yushuxin-vfexp.c llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll clang/test/CodeGen/YSX/tinyv-builtins-asm.c`
- `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- `git diff --check`

The directory-level lit run discovered 146 YSX tests and passed all 146.
The final generator coverage reports 8 `auto_full` instructions and 0 retained
schema gaps.

## Legacy Contrast

In the handwritten path, adding `yushuxin.vfexp` would typically require
choosing a TableGen instruction class, copying fixed encoding bits, adding asm
operands, deciding a pseudo class, wiring patterns, adding aliases, then
separately touching builtin/header glue. Several of those steps are not ISA
facts; they are local summaries of how previous instructions happened to be
organized.

The auto-td path removes that guessing. The opcode file is the encoding
authority. YAML is the per-instruction semantic authority. The generator owns
the mechanical TableGen shape. Handwritten C++ is reserved for reusable behavior
that is genuinely algorithmic.

## Current Boundary

The branch proves the framework and a narrow C-to-object path for explicit
`ysx_vector.h` builtins. It does not yet claim the complete tiny-F/tiny-V import
or automatic vectorization. It also does not claim direct C ABI passing or
returning of vector values; the proven C path keeps vectors inside builtin
functions and stores results to memory. Those next layers need broader generated
instruction coverage, legal type support, and lowering rules.
