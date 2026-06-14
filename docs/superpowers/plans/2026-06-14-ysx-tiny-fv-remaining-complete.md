# YSX Tiny-F/Tiny-V Remaining Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development or superpowers:executing-plans to
> implement this plan task-by-task. This file is the authoritative plan after
> the 2026-06-14 priority reset.

**Goal:** Make the original auto-td promise the first acceptance gate: new YSX
tiny-F/tiny-V instructions are added from `riscv-opcodes` compatible opcode
sources plus one auto-td YAML file, without hand-authoring nested TableGen
instruction class structures. Keep vectorization work to minimal C API or IR
smoke that maps one-to-one onto already generated instructions.

**Architecture:** Opcode files own encoding bits. YAML owns the instruction
identity, feature, taxonomy, pseudo, pattern, and builtin facts. The generator
owns real instruction TableGen records, audit manifests, and the minimal Clang
builtin / LLVM intrinsic / CGBuiltin dispatch fragments for YAML entries marked
`builtin.codegen: true`. Bounded handwritten glue remains only for the public
`ysx_vector.h` wrappers and backend selector details that are not yet generated.

**Tech Stack:** LLVM YSX backend C++/TableGen, Clang YSX resource header and
CodeGen tests, Python auto-td generator, lit/FileCheck, CMake/Ninja build in
`build`.

---

## Ground Rules

- Worktree:
  `/home/zhaosiying/.config/superpowers/worktrees/llvm-project-22.1.3-ysx/ysx-tiny-fv-ccu`
- Branch: `ysx-tiny-fv-ccu`
- Use the existing `build` directory first.
- Code-changing implementation is serial.
- Read-only exploration and review may use limited parallel subagents.
- Do not implement full direct tiny-V vector C ABI in this pass.
- Do not implement broad RVV autovec in this pass.
- Remove any earlier over-scoped ABI/autovec code unless needed by the minimal
  smoke.
- Do not expose standard RVV frontend APIs for YSX.
- Generated Clang builtin/intrinsic integration must stay limited to
  `builtin.codegen: true` proof APIs that map one-to-one to generated
  instructions.

## Current Fact Baseline

- The generated MC instruction surface contains 50 `auto_full` instructions:
  15 tiny-F and 35 tiny-V.
- Coverage must remain `retained_schema_gap: 0`.
- `yushuxin.vfexp` is the custom-instruction proof:
  `third_party/ysx-opcodes/extensions/rv_xtinyv` plus
  `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml`
  generate `YSX_AUTO_YUSHUXIN_VFEXP`.
- Existing selected C API proof paths now use generated Clang builtin,
  generated LLVM intrinsic, and generated CGBuiltin dispatch fragments for the
  `builtin.codegen: true` YAML slice.
- The generator manifest guard makes YAML-declared `pseudos`, `patterns`, and
  `builtin` facts visible in generated files so they cannot silently drift.

## Task 1: Baseline And Cleanup

- [x] Confirm branch and dirty state with `git status --short` and
  `git branch --show-current`.
- [x] Remove over-scoped uncommitted fixed-vector ABI, TTI, generic autovec, and
  reduction-lowering code from the previous attempt.
- [x] Remove untracked direct-ABI/autovec tests that belonged to the
  over-scoped attempt.
- [x] Rebuild only if the existing `build` tools are stale:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
```

## Task 2: Auto-TD Manifest Guard

**Files:**

- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/model.py`
- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/loader.py`
- `llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td/emit_td.py`
- `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py`

- [x] Add a failing generator test proving non-empty pseudo/pattern/builtin
  output is required when YAML declares those facts.
- [x] Teach the loader to retain `pseudos`, `patterns`, and `builtin`.
- [x] Emit stable manifest comments in:
  - `YSXGenAutoTinyVPseudos.inc`
  - `YSXGenAutoTinyVPatterns.inc`
  - `YSXGenAutoTinyVBuiltins.inc`
- [x] Verify the targeted generator test passes.
- [x] Run the full auto-td generator unit suite.

## Task 3: Documentation Contract

**Files:**

- `docs/superpowers/specs/2026-06-14-ysx-tiny-fv-remaining-complete-design.md`
- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`
- `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [x] Update the remaining-complete design so auto-td is the first priority and
  broad ABI/autovec work is out of scope.
- [x] Update the original auto-td design with the stricter current contract:
  generated instruction records are implemented; generated pseudo/pattern/
  builtin manifests are implemented; generated Clang builtin, LLVM intrinsic,
  and CGBuiltin dispatch consumption is implemented only for entries explicitly
  marked `builtin.codegen: true`.
- [x] Update the blog with:
  - the idea and motivation
  - the implementation shape
  - what the current implementation achieves
  - how to add one new instruction now
  - the honest boundary around manual Clang/LLVM glue
- [x] Update the feature checklist with the manifest guard and the reduced
  vectorization scope.

## Task 4: Minimal Instruction-Addition Workflow

The documented workflow for a future `yushuxin.vexp`-style instruction is:

1. Add the encoding to `third_party/ysx-opcodes/extensions/rv_xtinyv` or use a
   pinned upstream `third_party/riscv-opcodes` entry for standard instructions.
2. Add one YAML file under
   `llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/` or `tiny-f/` with:
   `mnemonic`, `opcode_source`, `spec_ref`, feature requirements, and optional
   `pseudos`, `patterns`, and `builtin`.
3. Run:

```bash
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td-current \
  --coverage build/ysx-auto-td-current/coverage.md
```

4. Check generated instruction and manifest output:

```bash
rg -n "YUSHUXIN_VEXP|yushuxin.vexp|auto-td-(pseudo|pattern|builtin)" \
  build/ysx-auto-td-current
```

5. Add MC asm/object tests for the generated instruction record.
6. If a C API is needed, add `builtin` metadata with `codegen: true` only when
   the prototype has a supported one-to-one lowering shape. The generator emits
   the Clang builtin TD, LLVM intrinsic TD, and CGBuiltin dispatch fragment.
7. Keep any remaining manual work to the public `ysx_vector.h` wrapper and
   backend selector path. Add a C-to-ASM/object smoke proving the API maps to
   the generated instruction and does not require a full vector ABI.

## Task 5: Minimal Vector Smoke Only

- [x] Keep existing `ysx_vector.h` C API smoke that maps selected builtins to
  generated instructions such as `vadd.vv`, `vredsum.vs`, `vle32.v`, `vse32.v`,
  and `yushuxin.vfexp`.
- [x] Do not add full direct vector ABI tests.
- [x] Do not add broad loop-vectorizer coverage.
- [x] Keep optional IR smoke fixed-width only in this pass, requiring
  one-to-one mapping to existing generated instructions and no new ABI claim.
- [x] Keep the current public C API proof fixed at 128-bit
  `ysx_vint32m1_t` / `ysx_vfloat32m1_t` wrappers. Do not describe it as a full
  scalable-vector C ABI.

## Task 6: Verification

Run:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
rm -rf build/ysx-auto-td-final-review
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td-final-review \
  --coverage build/ysx-auto-td-final-review/coverage.md \
  --clang-builtins-td build/ysx-auto-td-final-review/YSXGenAutoTinyVClangBuiltins.td \
  --clang-builtin-cg-inc build/ysx-auto-td-final-review/YSXGenAutoTinyVBuiltinCG.inc \
  --llvm-intrinsics-td build/ysx-auto-td-final-review/YSXGenAutoTinyVIntrinsics.td
rg -n "auto_full: 50|retained_schema_gap: 0|yushuxin.vfexp|auto-td-builtin" \
  build/ysx-auto-td-final-review
rg -n "def vfexp_v_f32m1|Intrinsic::ysx_vfexp|def int_ysx_vfexp" \
  build/ysx-auto-td-final-review
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX/tinyf-auto-td.s \
  llvm/test/MC/YSX/tinyv-auto-td.s \
  llvm/test/MC/YSX/tinyv-invalid-disassemble.s \
  llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll \
  clang/test/CodeGen/YSX/tinyv-builtins.c \
  clang/test/CodeGen/YSX/tinyv-builtins-asm.c \
  clang/test/CodeGen/YSX/yushuxin-vfexp.c
git diff --check
```

Expected:

- All commands exit 0.
- Coverage still reports `auto_full: 50` and `retained_schema_gap: 0`.
- Generated tiny-V builtins manifest contains `ysx_vfexp_v_f32m1`.
- Generated tiny-V pattern manifest contains `intrinsic=ysx.vfexp`.
- Generated Clang/LLVM fragments contain `vfexp_v_f32m1`,
  `Intrinsic::ysx_vfexp`, and `int_ysx_vfexp`.
- Lit output proves MC and C-to-ASM/object smoke for `yushuxin.vfexp`.

## Acceptance Criteria

- Worktree has no untracked or uncommitted source changes after commit.
- New instruction records in this slice are generated from opcode source plus
  YAML, not handwritten nested instruction TableGen.
- YAML-declared pseudo/pattern/builtin facts generate tested manifests.
- `yushuxin.vfexp` remains covered from opcode source to generated instruction
  record, MC asm/object, and selected C API smoke.
- Docs explain the idea, implementation, effect, and new-instruction workflow
  clearly enough to seed future blog generation.
- Docs explicitly state that generated Clang builtin/intrinsic/CGBuiltin
  consumption is complete for the `builtin.codegen: true` proof slice, while
  public header wrappers and broader selector automation remain bounded/manual.
- Vectorization scope is reduced to minimal one-to-one mapping smoke.
- No generic RVV frontend exposure, full direct vector ABI claim, or full
  autovec claim is introduced.
