# Round 26 Summary

## Work Completed
- Removed stale inactive copied CodeGen check blocks for removed YSX surfaces called out by the Round-25 review: compressed/RVE/FP, Zicfilp, Zabha/Zalasr, XAndes, SFB, CCMOV, and VTCONDOPS.
- Cleaned all 13 files reported by the broadened removed-surface scan and preserved active retained prefixes such as `RV64I`, `RV64`, `RV64IA`, and `RV64I-ZALRSC`.
- Renamed the active `jumptable-swguarded.ll` `NO-ZICFILP` prefix to `CHECK` and removed the stale inactive `lpad` block so the file no longer carries removed Zicfilp text.
- Backend size remains 26,038 lines versus RISCV's 136,068 lines; focused YSX tests are now 48,338 lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-26-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-26-summary.md`
- `llvm/test/CodeGen/YSX/add-before-shl.ll`
- `llvm/test/CodeGen/YSX/add_sext_shl_constant.ll`
- `llvm/test/CodeGen/YSX/atomic-cmpxchg-branch-on-result.ll`
- `llvm/test/CodeGen/YSX/atomic-cmpxchg.ll`
- `llvm/test/CodeGen/YSX/atomic-load-store.ll`
- `llvm/test/CodeGen/YSX/atomic-load-zext.ll`
- `llvm/test/CodeGen/YSX/branch-relaxation-rv64.ll`
- `llvm/test/CodeGen/YSX/calling-conv-preserve-most.ll`
- `llvm/test/CodeGen/YSX/calls.ll`
- `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll`
- `llvm/test/CodeGen/YSX/jumptable-swguarded.ll`
- `llvm/test/CodeGen/YSX/select-binop-identity.ll`
- `llvm/test/CodeGen/YSX/select-const.ll`

## Validation
- PASS: targeted changed-test lit, 13 tests.
- PASS: `llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`, 132 tests.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: smoke/negative probes for YSX and RISCV object generation, retained `.insn`/`.reloc`, rejected removed `.reloc`, unsupported `-mattr`, `.option arch rv64ima_zbb`, numeric CSR, removed `.insn` forms, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: broadened stale-prefix scan for `ZICFILP|XANDESPERF|RV64C|RV64E|RV64IF|RV64ID|RV64IFD|SFB64|VTCONDOPS|CMV-FUSION|ZABHA|ZALASR|LP64E|RV32|XTHEAD|ZCMP|ZCA|RV64IA-TSO` has no YSX test matches.
- PASS: additional mnemonic scan only reports retained A-extension `sc.*` expectations, IR local names, and one intentional negative `xventanacondops` driver check.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV | wc -l` reports `0`.
- INFO: `code-simplifier` plugin lookup found no local plugin path under the repo or `/home/zhaosiying/.codex`.

## Remaining Items
- No known Round-26 implementation blockers. Completion remains pending Codex stop-gate review.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: BitLesson selector returned only the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output, so no lesson was applied or updated.
