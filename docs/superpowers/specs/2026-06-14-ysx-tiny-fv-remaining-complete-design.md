# YSX Tiny-F/Tiny-V Remaining Completion Design

## Goal

Close the remaining YSX tiny-F/tiny-V auto-td work in the existing
`ysx-tiny-fv-ccu` worktree without expanding the scope into a full standard F/V
or full RVV backend.

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

This completion pass covers the remaining plan's final unresolved goals:

1. Direct tiny-V vector ABI boundary.
2. Minimal automatic vectorization smoke.
3. Final validation, review, and documentation.

The current implemented base is assumed to remain intact:

- 50 generated `auto_full` instructions.
- 0 retained schema gaps for the generated instruction surface.
- Explicit `ysx_vector.h` builtin proof paths.
- Selected tiny-F scalar CodeGen proof paths.
- Fixed 128-bit selected tiny-V builtin-to-object proof paths.

## Direct Vector ABI Boundary

The first task attempts a narrow direct ABI implementation for fixed 128-bit
tiny-V vector values.

The narrow implementation is allowed only if it can be contained in YSX-owned
lowering, calling-convention, copy, spill, reload, and frame-index handling for
the already proven fixed-width vector register class. It must not claim or
import a generic RVV C ABI, expose standard RVV frontend types, or redefine YSX
as a full vector ABI target.

If implementation requires broad ABI design beyond that narrow path, the task
must instead land a checked unsupported boundary. That boundary must include
tests proving direct tiny-V vector passing/returning is rejected or diagnosed
instead of silently miscompiled, and documentation must state that the proven C
path keeps vector values inside explicit builtins and stores results to memory.

The fallback is considered successful only when it prevents future accidental
claims of direct tiny-V C ABI support.

## Automatic Vectorization Smoke

The automatic vectorization target is intentionally minimal.

The completion pass only needs to prove that simple contiguous `i32` add and
integer reduction loops can use the YSX tiny-V path when compiled with
`+xtinyv,+zvl128b`. The final assembly or lowered IR must show legal generated
tiny-V instructions from the existing selected surface, such as `vadd.vv`,
`vredsum.vs`, `vle32.v`, `vse32.v`, `vsetivli`, or `vsetvli` as appropriate.

This does not claim full RVV autovec support. It does not require strided,
indexed, gather/scatter, masked, scalable-vector, or non-i32 automatic
vectorization. Unsupported behavior outside the smoke must remain bounded and
must not enable generic RVV frontend macros or standard RVV intrinsic headers.

## Documentation

The completion pass updates the existing documentation to match the final
behavior:

- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`
- `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`

The docs must distinguish three claims:

1. Generated auto-td instruction surface is complete for the agreed first slice.
2. Explicit builtin CodeGen/object proof paths are supported for selected fixed
   128-bit vector operations.
3. Automatic vectorization is only proven for the minimal smoke cases in this
   pass.

If direct vector ABI falls back to an unsupported boundary, the docs must say so
plainly and must not present it as implemented.

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

- ABI task runs the new direct ABI proof or unsupported-boundary lit test.
- Autovec task runs the new `tinyv-autovec.ll` and `tinyv-autovec.c` tests.
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
- Direct tiny-V vector ABI is either narrowly implemented for the fixed 128-bit
  proof type or explicitly locked down as an unsupported boundary with tests and
  documentation.
- Minimal contiguous `i32` add and reduction automatic-vectorization smoke tests
  pass and lower to legal YSX tiny-V instruction paths.
- No generic RVV frontend exposure is introduced for YSX.
- Auto-td generator tests pass.
- Generator coverage still reports 50 `auto_full` instructions and 0 retained
  schema gaps for the generated instruction surface.
- YSX MC, CodeGen, Driver, and Clang CodeGen lit tests pass in the focused final
  validation set.
- Spec, checklist, blog, and remaining plan describe the final state without
  overclaiming full F/V, full RVV, full vector ABI, or full automatic
  vectorization.
