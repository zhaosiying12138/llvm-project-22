# Round 25 Summary

## Work Completed
- Removed inactive copied C/Zca alignment check prefixes and compressed
  instruction expectations from `llvm/test/MC/YSX/align.s` while preserving the
  active rv64ima relax/norelax alignment checks.
- Removed inactive copied Zcmp check blocks from
  `llvm/test/CodeGen/YSX/callee-saved-gprs.ll`, leaving only active RV64I and
  RV64I-with-frame-pointer YSX checks.
- Reworded stale comments so the remaining tests describe the retained YSX
  surface rather than removed extensions.

## Files Changed
- `llvm/test/MC/YSX/align.s`
- `llvm/test/CodeGen/YSX/callee-saved-gprs.ll`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-25-contract.md`

## Validation
- `rg "C-OR-ZCA|C-EXT|ZCA|ZCMP|RV64IZCMP|RV32|LP64E|XTHEAD|RV64IA-TSO" llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` has no matches.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX/align.s llvm/test/CodeGen/YSX/callee-saved-gprs.ll` passed with 2 tests discovered.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- Manual smoke and negative probes passed: YSX and combined RISCV smoke compiles produced ELF64 RISC-V objects, retained `.insn r OP` and retained `.reloc` names assembled, and unsupported `.reloc`, `-mattr`, `.option arch`, CSR, removed `.insn`, Clang unsupported arch, scalable-vector IR, and direct vector intrinsic probes failed as expected.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `26,038`; reduction `110,030` lines, or `80.86%`; YuShuXin tests `50,069` lines.

## Remaining Items
- No known Round 25 implementation blocker remains before Codex review.
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
