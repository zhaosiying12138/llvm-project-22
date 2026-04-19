# Round 24 Summary

## Work Completed
- Replaced the broad copied relocation-name import in
  `YSXAsmBackend::getFixupKind` with an explicit retained standard relocation
  whitelist.
- Removed `RISCV.def` and `RISCV_nonstandard.def` use from the YSX `.reloc`
  name path, so compressed, vendor, custom, and nonstandard relocation names no
  longer assemble through `.reloc`.
- Added focused MC negative coverage for the reviewed unsupported `.reloc`
  names while preserving the existing retained relocation directive coverage.

## Files Changed
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXAsmBackend.cpp`
- `llvm/test/MC/YSX/unsupported-features.s`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-24-contract.md`

## Validation
- `rg "RISCV_nonstandard|R_RISCV_VENDOR|R_RISCV_RVC_|R_RISCV_QC_|R_RISCV_NDS_|R_RISCV_CHERIOT|R_RISCV_CUSTOM" llvm/lib/Target/YuShuXin llvm/test/MC/YSX` now reports only the intentional negative-test RUN lines and no YSX source matches.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- Manual smoke and negative probes passed: YSX and combined RISCV smoke compiles produced ELF64 RISC-V objects, retained `.insn r OP` and retained `.reloc` names assembled, and unsupported `.reloc`, `-mattr`, `.option arch`, CSR, removed `.insn`, Clang unsupported arch, scalable-vector IR, and direct vector intrinsic probes failed as expected.
- `llvm-mc -triple=ysx64 -mattr=help` has no reviewed unsupported markers such as `short-forward-branch`, `RVC`, `CUSTOM`, or `vendor`.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `26,038`; reduction `110,030` lines, or `80.86%`; YuShuXin tests `50,762` lines.

## Remaining Items
- No known Round 24 implementation blocker remains before Codex review.
- Final completion still depends on the RLCR stop gate result.
- The `code-simplifier` plugin was not found under the repo or
  `/home/zhaosiying/.codex`, so no plugin optimization pass was available to
  run.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector produced the placeholder `LESSON_IDS:
  <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`,
  so no applicable lesson delta was recorded for this round.
