# Round 29 Summary

## Work Completed
- Restricted YSX Clang GCC register names and aliases to retained rv64ima GPRs.
- Added YSX Clang negative coverage for removed FP/vector/CSR clobber names and a positive retained `x9`/`s1` clobber probe.
- Removed active copied `~{f8},~{f9}` and `~{vtype},~{vl},~{vxsat},~{vxrm}` clobbers from YSX CodeGen tests.
- Added a YSX backend IR guard and negative CodeGen test for removed inline-asm clobbers that bypass Clang.
- Backend size is now 26,043 lines versus RISCV's 136,068 lines; focused YSX tests are now 48,111 lines.

## Files Changed
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-29-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-29-summary.md`
- `clang/lib/Basic/Targets/RISCV.h`
- `clang/lib/Basic/Targets/RISCV.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `llvm/lib/Target/YuShuXin/YSXCodeGenPrepare.cpp`
- `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll`
- `llvm/test/CodeGen/YSX/inline-asm-mem-constraint.ll`
- `llvm/test/CodeGen/YSX/unsupported-inline-asm-clobbers.ll`

## Validation
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- PASS: targeted lit for `clang/test/Driver/YSX/target-options.c`, `inline-asm-clobbers.ll`, `inline-asm-mem-constraint.ll`, and `unsupported-inline-asm-clobbers.ll`, 4 tests.
- PASS: `llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`, 133 tests.
- PASS: direct Clang clobber probes reject `f8`, `fs0`, `v0`, `vtype`, `vl`, `vxsat`, and `vxrm`, while accepting `x9` and `s1`.
- PASS: smoke/negative probes for YSX and RISCV object generation, retained `.insn`/`.reloc`, rejected removed `.reloc`, unsupported `-mattr`, `.option arch rv64ima_zbb`, numeric CSR, removed `.insn` forms, `rv64imaf`, `ysx32`, removed clobbers, scalable-vector IR, direct `llvm.riscv.vsetvli`, and backend IR removed-clobber rejection.
- PASS: `rg -n "MIPS|CCMov|ccmov|mips\.ccmov|RV64I-CCMOV|RV64-MIPS" ...` has no YSX source or test matches.
- PASS: Round-25 stale-prefix scan has no YSX test matches.
- PASS: removed-clobber scan only reports intentional negative coverage in `unsupported-inline-asm-clobbers.ll`.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV clang/test/Driver/RISCV clang/test/CodeGen/RISCV | wc -l` reports `0`.
- INFO: `code-simplifier` plugin lookup found no local plugin path under the repo or `/home/zhaosiying/.codex`.

## Remaining Items
- No known Round-29 implementation blockers. Completion remains pending Codex stop-gate review.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: BitLesson selector returned only the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` output, so no lesson was applied or updated.
