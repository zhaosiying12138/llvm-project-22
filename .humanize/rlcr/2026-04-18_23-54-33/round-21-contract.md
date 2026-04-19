# Round 21 Contract

## Mainline Objective

Close the remaining YSX `.insn` removed-ISA acceptance path by pruning dead
FP/vector/custom opcode and format scaffolding and restricting `.insn` to the
retained rv64ima major opcodes.

## Target Acceptance Criteria

- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
- AC-3: YSX implementation is materially smaller than RISCV and contains no
  support code for removed features.

## Blocking Issues

- `.insn` accepts removed FP/vector/custom opcode names such as `OP_FP`, `OP_V`,
  `MADD`, and `CUSTOM_0`.
- `.insn` accepts numeric removed major opcode values such as `83`, `87`, `67`,
  and `11`.
- Dead R4/FRM/vector dependency TableGen scaffolding remains despite having no
  retained rv64ima instruction users.

## Queued Out Of Scope

- Goal tracker immutable AC-list drift remains documented and unchanged.
- CPU/tune target-attribute warn-and-ignore diagnostics stay queued unless this
  round discovers unsupported-feature leakage.

## Success Criteria

- `YSXInstrFormats.td`, `YSXBaseInfo.h`, and `YSXInstrInfo.td` no longer expose
  dead R4/FRM/vector dependency scaffolding or removed opcode names.
- `YSXAsmParser::parseInsnDirectiveOpcode` rejects numeric `.insn` opcodes
  outside the retained rv64ima major opcode set.
- YSX MC tests cover removed named and numeric `.insn` opcodes.
- YSX-only and RISCV+YSX static builds, focused YSX lit, smoke/negative probes,
  `git diff --check`, and RISCV zero-diff checks pass.
