# Round 23 Summary

## Work Completed
- Deleted the reviewed short-forward-branch and CCMOV tuning residue from YuShuXin.
- Removed dead FP/vector scheduler predicates and scheduler reads with no rv64ima role.
- Removed vector custom-ISD TSFlag helpers and the unused vector multiply-add helper from `YSXSelectionDAGInfo.h`.
- Removed the SFB-only divide-by-power-of-two lowering override and made select lowering use the retained non-CCMOV path directly.
- Removed the unused `YSXGenExegesis.inc` tablegen target after assertions-enabled `llvm-tblgen` exposed that YSX had no exegesis counter bindings and no YSX source consumed the generated file.

## Files Changed
- `llvm/lib/Target/YuShuXin/CMakeLists.txt`
- `llvm/lib/Target/YuShuXin/YSXFeatures.td`
- `llvm/lib/Target/YuShuXin/YSXSchedule.td`
- `llvm/lib/Target/YuShuXin/YSXInstrPredicates.td`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.h`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.h`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-23-contract.md`

## Validation
- `rg "ShortForwardBranch|short-forward-branch|SFB|PseudoCCMOV|PseudoCCSUB|VLDSX0Pred|SingleElementVecFP64SchedPred|ReadFMemBase|ReadFStoreData|HasPassthruOp|HasMaskOp|getMAccOpcode|vector multiply-add" llvm/lib/Target/YuShuXin` produced no matches.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed after CMake regeneration.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed after CMake regeneration.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- Smoke and negative probes passed for YSX-only and combined RISCV+YSX tools: YSX and RISCV smoke compiles produced ELF64 RISC-V soft-float objects, retained `.insn r OP` assembled, and unsupported feature, CSR, Clang, scalable-vector IR, direct `llvm.riscv.vsetvli`, removed `.insn` opcode, and `.insn r4` probes failed as expected.
- Configured `/home/zhaosiying/codebase/compiler/build_ysx_assert_host_llvm` with Ninja, ccache, Clang, `LLVM_ENABLE_ASSERTIONS=ON`, projects `clang;lld`, and experimental target `YSX`; `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_assert_host_llvm LLVMYSXCodeGen llc` passed.
- The first assertions-enabled build attempt failed before the CMake cleanup in `YSXGenExegesis.inc` generation because `llvm-tblgen -gen-exegesis` asserted on missing PFM counter bindings; removing the unused YSX exegesis generation fixed the assertions-enabled build.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `25,987`; reduction `110,081` lines, or `80.90%`; YuShuXin tests `50,754` lines.

## Remaining Items
- No known Round 23 implementation blocker remains before Codex review.
- Final completion still depends on the RLCR stop gate result.
- The `code-simplifier` plugin was not found under the repo or `/home/zhaosiying/.codex`, so no plugin optimization pass was available to run.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector produced the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, so no applicable lesson delta was recorded for this round.
