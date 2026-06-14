# YSX Tiny-F/Tiny-V Remaining Completion Design

## Goal

Close the remaining YSX tiny-F/tiny-V auto-td work in the existing
`ysx-tiny-fv-ccu` worktree without expanding the scope into a full standard F/V
or full RVV backend.

The first priority is the original auto-td expectation: adding a new YSX
instruction must not require inventing a nested handwritten TableGen class
hierarchy. Encoding and operand facts come from `riscv-opcodes` compatible
opcode sources plus one auto-td YAML file. The generator owns the real
instruction TableGen records and also emits audit manifests for any pseudo,
pattern, and builtin facts declared in YAML.

The work finishes the remaining-plan layer after the initial auto-td builtin
proof. It keeps the already proven schema-first model: instruction facts stay in
YAML, instruction encodings stay in pinned opcode-source files, generated
TableGen owns mechanical instruction records, and handwritten C++ is limited to
backend behavior that cannot be represented mechanically.

## Workspace

All implementation, planning, validation, and commits happen in this existing
worktree:

```text
/home/zhaosiying/.config/superpowers/worktrees/llvm-project-22.1.3-ysx/ysx-tiny-fv-ccu
```

No new git worktree is created for this completion pass.

## Confirmed Scope

This completion pass covers the remaining plan's final unresolved goals after
the priority reset:

1. Make the auto-td instruction-addition contract explicit and guarded.
2. Keep `yushuxin.vfexp` as the custom-instruction proof, including opcode
   source, YAML, generated instruction record, C API smoke, assembly, and object
   evidence.
3. Document how a future `yushuxin.vexp`-style instruction is added now.
4. Keep vectorization work to the smallest smoke needed to show C API or
   fixed-width IR mapping to existing generated instructions.
5. Final validation, review, and documentation.

The current implemented base is assumed to remain intact:

- 50 generated `auto_full` instructions.
- 0 retained schema gaps for the generated instruction surface.
- Explicit `ysx_vector.h` builtin proof paths.
- Selected tiny-F scalar CodeGen proof paths.
- Fixed 128-bit selected tiny-V builtin-to-object proof paths.

## Auto-TD Contract

The strict contract for this completion pass is:

- A real YSX tiny-F/tiny-V instruction record is generated from opcode source
  plus YAML. Handwritten nested instruction TableGen classes are not allowed for
  new instructions in this slice.
- The generated instruction coverage must remain `auto_full: 50` and
  `retained_schema_gap: 0`.
- Custom YSX opcode sources live in `third_party/ysx-opcodes` using the
  `riscv-opcodes` format, so `yushuxin.vfexp` and a future `yushuxin.vexp` use
  the same parser path as standard encodings.
- YAML-declared `pseudos`, `patterns`, and `builtin` facts must not disappear.
  The generator emits stable manifests in `YSXGenAutoTinyVPseudos.inc`,
  `YSXGenAutoTinyVPatterns.inc`, and `YSXGenAutoTinyVBuiltins.inc`.
- Current C-to-ASM/object support for selected builtins remains implemented by
  bounded Clang/LLVM glue. The manifest is the guard and migration point for a
  future generated Clang builtin/intrinsic integration. Documentation must not
  claim that this last Clang/intrinsic layer is already fully generated.

## Vector And ABI Boundary

The vectorization target is intentionally minimal. It is enough to prove that
the existing YSX C API and fixed-width IR smoke map to generated instructions
such as `vadd.vv`, `vredsum.vs`, `vle32.v`, `vse32.v`, `vsetivli`,
`vsetvli`, and `yushuxin.vfexp`.

This pass does not spend time on full direct vector ABI support, full RVV
automatic vectorization, scalable-vector frontend APIs, gather/scatter autovec,
or a generic RVV C ABI. Any previously attempted broad ABI/autovec code should
be removed unless it is needed for the minimal smoke.

## Documentation

The completion pass updates the existing documentation to match the final
behavior:

- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`
- `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`

The docs must distinguish three claims:

1. Generated auto-td instruction surface is complete for the agreed first slice.
2. YAML-declared pseudo/pattern/builtin facts are preserved in generated
   manifests and tested.
3. Explicit builtin CodeGen/object proof paths are supported for selected fixed
   128-bit vector operations.
4. Automatic vectorization, if mentioned, is only a minimal mapping smoke and
   not a backend-completeness claim.
5. Direct tiny-V vector C ABI passing/returning is not a goal for this pass and
   must not be presented as implemented.

## Execution Model

Use goal-mode tracking for the full completion pass.

Use limited parallelism:

- Code-changing implementer work is serial.
- Read-only exploration, spec review, code quality review, and final review may
  run in parallel when they do not write files.
- Subagents should use `gpt-5.5` with `xhigh` reasoning when the tool surface
  allows explicit model selection.

Each code-changing task follows this sequence:

1. Write or extend a failing test.
2. Run the focused test and confirm the expected failure.
3. Implement the smallest YSX-scoped change that passes the test.
4. Run the focused test and relevant adjacent tests.
5. Commit the task.
6. Run spec-compliance review.
7. Run code-quality review.
8. Fix and re-review until no blocking findings remain.

## Validation Strategy

Validation is strict but focused.

Task-level validation:

- Auto-td generator tests check real instruction generation and non-empty
  pseudo/pattern/builtin manifests when YAML declares those facts.
- MC tests check generated tiny-F/tiny-V encodings, including
  `yushuxin.vfexp`.
- Clang smoke tests check selected `ysx_vector.h` APIs lower through LLVM IR to
  assembly/object output.
- Docs task runs documentation self-checks and `git diff --check`.

Integration validation:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td-final-review \
  --coverage build/ysx-auto-td-final-review/coverage.md
```

The coverage report should continue to show:

```text
auto_full: 50
retained_schema_gap: 0
```

Final focused lit validation:

```bash
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX \
  llvm/test/CodeGen/YSX \
  clang/test/Driver/YSX \
  clang/test/CodeGen/YSX
```

The existing `ysx-tiny-fv-ccu/build` directory may be used as the first
validation reference. If build outputs become stale or missing, the plan should
add a minimal rebuild step for the YSX/Clang tools required by the targeted
tests.

## Acceptance Criteria

- The remaining plan is closed without leaving untracked or uncommitted source
  changes in `ysx-tiny-fv-ccu`.
- New instruction records in this slice are generated from opcode source plus
  YAML, not handwritten nested instruction TableGen.
- YAML-declared pseudo/pattern/builtin facts generate tested manifests instead
  of empty placeholder files.
- Minimal vector smoke stays limited to C API or fixed-width IR mapping to legal
  YSX tiny-V instruction paths.
- No generic RVV frontend exposure is introduced for YSX.
- Auto-td generator tests pass.
- Generator coverage still reports 50 `auto_full` instructions and 0 retained
  schema gaps for the generated instruction surface.
- YSX MC, CodeGen, Driver, and Clang CodeGen lit tests pass in the focused final
  validation set.
- Spec, checklist, blog, and remaining plan describe the final state without
  overclaiming full F/V, full RVV, full vector ABI, full automatic
  vectorization, or fully generated Clang builtin/intrinsic integration.
