# Round 21 Summary

## Work Completed
- Closed the reviewed `.insn` acceptance gap by limiting YuShuXin major opcodes to the retained RV64IMA surface.
- Removed the remaining R4/vector/floating/custom `.insn` scaffolding from the YuShuXin TableGen and MC metadata path.
- Added explicit MC negative coverage for removed opcode names, removed numeric opcode values, and the removed `r4` `.insn` format.
- Confirmed RISCV remains untouched and YuShuXin stays independently buildable.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`
- `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp`
- `llvm/test/MC/YSX/unsupported-features.s`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-21-contract.md`

## Validation
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- Smoke and negative probes passed for YSX-only and combined RISCV+YSX tools: retained RV64IMA assembly works; unsupported `+v`, 32-bit, `+zca`, vendor extensions, Zbb `.option arch`, CSR assembly, floating ABI, scalable vector IR, direct `llvm.riscv.vsetvli`, removed `.insn` opcode names, removed `.insn` numeric opcodes, and `.insn r4` fail as expected.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Source scan for removed opcode/R4 scaffolding under `llvm/lib/Target/YuShuXin` found no remaining matches; only the intended unsupported-feature test lines match.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `26,588`; reduction `109,480` lines, or `80.46%`; YuShuXin tests `50,754` lines.

## Remaining Items
- No known Round 21 implementation blocker remains before Codex review.
- Final completion still depends on the RLCR stop gate result.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector produced the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, so no applicable lesson delta was recorded for this round.
