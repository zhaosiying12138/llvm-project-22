# Round 18 Summary

## Work Completed
- Finished the AC-3 RV32/ABI/removed-extension source-pruning slice for YSX.
- Collapsed remaining YSX hardware-mode and ABI handling to `ysx64`/`rv64ima`/`lp64`.
- Removed residual compressed/Zc/QC/vendor MC plumbing, RegList/StackAdj parser/printer/code-emitter hooks, RVC compression attempts, vendor fixups/relocation emission, and dead Zicfilp/QCI/push-pop paths.
- Deleted remaining false-query/callsite surfaces for unsupported extensions and cleaned stale source comments/metadata for the searched RV32/Zba/Zbb/Zc/RVC/vendor/QC/THead/Andes/Rivos/XRemoved markers.
- Fixed the resulting rv64 inline-asm/GPRPair and immediate-materialization regressions while keeping i32 as an internal TableGen-visible GPR type but not a 32-bit target surface.

## Files Changed
- `llvm/lib/Target/YuShuXin/**`: 44 files changed, including `YSXFeatures.td`, `YSXRegisterInfo.td`, `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, MC target-desc files, asm parser/printer, frame lowering, machine-function info, subtarget, and instruction TD files.
- Deleted `llvm/lib/Target/YuShuXin/YSXLandingPadSetup.cpp` and `llvm/lib/Target/YuShuXin/YSXIndirectBranchTracking.cpp`.
- Updated `llvm/test/MC/YSX/unsupported-features.s` for the new `.insn` 16-bit encoding diagnostic.

## Validation
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` (`132` tests)
- PASS: YSX clang smoke compile produced `ELF 64-bit LSB relocatable, UCB RISC-V, soft-float ABI`.
- PASS: combined RISCV clang smoke compile with `--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64`.
- PASS: negative probes rejected `+v`, `+32bit`, `+zca`, vendor `+xventanacondops`, `rv64imaf`, `ysx32`, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: AC-3 source scan for removed RV32/ABI/compressed/vendor markers returned no YSX source matches.

## Remaining Items
- Stale inactive unsupported YSX check-prefix blocks remain queued for AC-4 cleanup after this AC-3 source-pruning slice is reviewed.
- Current backend line counts: RISCV `136,068`; YSX `27,277`; reduction `108,791` lines, about `79.95%`.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `.humanize/bitlesson.md` still has no concrete entries; `bitlesson-selector` returned the placeholder `NONE` result for this round's tasks.
