# Round 28 Summary

## Work Completed
- Removed residual copied MIPS/CCMov/load-store-pair source scaffolding from the YSX backend.
- Deleted unused `useMIPSLoadStorePairs()` and `useMIPSCCMovInsn()` declarations/definitions from `YSXSubtarget.*`.
- Deleted dead `YSXInstrInfo::isPairableLdStInstOpc()` and `YSXInstrInfo::isLdStSafeToPair()` declarations/definitions and the stale MIPS load/store-pairing comment from `YSXInstrInfo.*`.
- Backend size is now 25,987 lines versus RISCV's 136,068 lines; focused YSX tests remain 48,096 lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-28-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-28-summary.md`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`

## Validation
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: `llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`, 132 tests.
- PASS: smoke/negative probes for YSX and RISCV object generation, retained `.insn`/`.reloc`, rejected removed `.reloc`, unsupported `-mattr`, `.option arch rv64ima_zbb`, numeric CSR, removed `.insn` forms, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: `rg -n "MIPS|CCMov|ccmov|mips\.ccmov|RV64I-CCMOV|RV64-MIPS" llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` has no matches.
- PASS: Round-25 stale-prefix scan has no YSX test matches.
- PASS: Round-27 expanded removed-surface scan has no YSX test matches.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV clang/test/Driver/RISCV clang/test/CodeGen/RISCV | wc -l` reports `0`.
- INFO: `code-simplifier` plugin lookup found no local plugin path under the repo or `/home/zhaosiying/.codex`.

## Remaining Items
- No known Round-28 implementation blockers. Completion remains pending Codex stop-gate review.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: BitLesson selector returned only the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output, so no lesson was applied or updated.
