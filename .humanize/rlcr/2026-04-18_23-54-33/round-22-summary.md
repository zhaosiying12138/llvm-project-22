# Round 22 Summary

## Work Completed
- Deleted the reviewed dead YuShuXin selector scaffolding for removed non-rv64ima paths.
- Removed `AddrRegRegScale` and `AddrRegZextRegScale` TableGen complex patterns.
- Removed the matching DAG selector declarations/implementations and the scaled-address helper functions.
- Removed copied `selectSHXADDOp`, `selectSHXADD_UWOp`, `selectSF_VC_X_SE`, and stale `performCombineVMergeAndVOps` hooks.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.td`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.h`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `.humanize/rlcr/2026-04-18_23-54-33/round-22-contract.md`

## Validation
- `rg "AddrRegRegScale|AddrRegZextRegScale|selectSHXADD|SHXADD|selectSF_VC_X_SE|performCombineVMergeAndVOps" llvm/lib/Target/YuShuXin` produced no matches.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed with 132 tests discovered.
- Smoke and negative probes passed for YSX-only and combined RISCV+YSX tools: YSX and RISCV smoke compiles produced ELF64 RISC-V soft-float objects, retained `.insn r OP` assembled, and unsupported feature, CSR, Clang, scalable-vector IR, direct `llvm.riscv.vsetvli`, removed `.insn` opcode, and `.insn r4` probes failed as expected.
- `llvm-mc -triple=ysx64 -mattr=help` did not expose reviewed unsupported markers such as `32bit`, vector tuning knobs, vendor tuning knobs, or vector register markers.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Current line counts: RISCV backend `136,068`; YuShuXin backend `26,241`; reduction `109,827` lines, or `80.71%`; YuShuXin tests `50,754` lines.

## Remaining Items
- No known Round 22 implementation blocker remains before Codex review.
- Final completion still depends on the RLCR stop gate result.
- The `code-simplifier` plugin was not found under the repo or `/home/zhaosiying/.codex`, so no plugin optimization pass was available to run.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector produced the placeholder `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, so no applicable lesson delta was recorded for this round.
