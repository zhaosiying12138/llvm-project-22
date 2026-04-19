# Round 27 Summary

## Work Completed
- Removed the remaining inactive copied CCMOV/MIPS check blocks from YSX CodeGen select tests.
- Deleted inactive `RV64I-CCMOV` checks from `select-and.ll`, `select-or.ll`, and `select-cc.ll`.
- Deleted inactive `RV64-MIPS` checks from `select-cond.ll`.
- Preserved active `RV64I`/`RV64` checks and all IR bodies.
- Backend size remains 26,038 lines versus RISCV's 136,068 lines; focused YSX tests are now 48,096 lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-27-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-27-summary.md`
- `llvm/test/CodeGen/YSX/select-and.ll`
- `llvm/test/CodeGen/YSX/select-or.ll`
- `llvm/test/CodeGen/YSX/select-cc.ll`
- `llvm/test/CodeGen/YSX/select-cond.ll`

## Validation
- PASS: `llvm-lit -q llvm/test/CodeGen/YSX/select-and.ll llvm/test/CodeGen/YSX/select-or.ll llvm/test/CodeGen/YSX/select-cc.ll llvm/test/CodeGen/YSX/select-cond.ll`, 4 tests.
- PASS: `llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`, 132 tests.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: smoke/negative probes for YSX and RISCV object generation, retained `.insn`/`.reloc`, rejected removed `.reloc`, unsupported `-mattr`, `.option arch rv64ima_zbb`, numeric CSR, removed `.insn` forms, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: CCMOV/MIPS scan for `ccmov|mips\.ccmov|RV64I-CCMOV|RV64-MIPS` has no YSX source or test matches.
- PASS: Round-25 stale-prefix scan has no YSX test matches.
- PASS: expanded stale-prefix/mnemonic scan for the reviewed removed surfaces has no YSX test matches.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV | wc -l` reports `0`.
- INFO: `code-simplifier` plugin lookup found no local plugin path under the repo or `/home/zhaosiying/.codex`.

## Remaining Items
- No known Round-27 implementation blockers. Completion remains pending Codex stop-gate review.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: BitLesson selector returned only the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output, so no lesson was applied or updated.
