# Round 15 Summary

## Work Completed
- Deleted the copied `YSX_UNSUPPORTED_ISD` compatibility namespace from
  `YSXSelectionDAGInfo.h`.
- Removed direct unsupported custom-ISD lowering/selection references from
  `YSXISelLowering.cpp` and `YSXISelDAGToDAG.cpp`, keeping only generated
  rv64ima scalar custom nodes needed by calls, branches, atomics, addresses,
  and integer legalization.
- Collapsed large vector/FP/RV32/vendor lowering, combine, known-bits, VP, and
  YSXVec helper surfaces into rv64ima-only behavior, then deleted unused helper
  declarations and bodies exposed by that prune.
- Regenerated the affected YSX CodeGen checks whose old copied RISCV
  expectations depended on now-deleted DAG-combine/known-bits/select
  optimizations.
- Current backend source line count:
  - Original RISCV backend: 136,068 lines.
  - Current YSX backend: 33,050 lines.
  - Reduction: 103,018 lines, about 75.7%.

## Files Changed
- `llvm/lib/Target/YuShuXin/YSXSelectionDAGInfo.h`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.h`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- 17 focused `llvm/test/CodeGen/YSX/*.ll` files with regenerated checks for the
  simplified rv64ima lowering output.

## Validation
- `bitlesson-selector --task "Round 15 update YSX CodeGen FileCheck expectations after rv64ima lowering prune" --paths llvm/test/CodeGen/YSX --bitlesson-file .humanize/bitlesson.md` returned no actionable lesson.
- `bitlesson-selector --task "Round 15 remove unused YSXISelLowering unsupported-vector helper stubs" --paths llvm/lib/Target/YuShuXin/YSXISelLowering.cpp,llvm/lib/Target/YuShuXin/YSXISelLowering.h --bitlesson-file .humanize/bitlesson.md` returned no actionable lesson.
- Unsupported custom-ISD reference script reports `bad_refs=0` for
  `YSXSelectionDAGInfo.h`, `YSXISelLowering.cpp`, and
  `YSXISelDAGToDAG.cpp`.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm llvm-mc clang llc` passed.
- `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` passed: 132 tests.
- `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passed.
- YSX smoke compile produced an ELF64 RISC-V soft-float relocatable object.
- RISCV `rv64ima/lp64` smoke compile in the combined build produced an ELF64
  RISC-V soft-float relocatable object.
- Negative probes rejected YSX `+v`, `rv64imaf`, scalable-vector IR, and direct
  `llvm.riscv.vsetvli` with deterministic diagnostics.
- `git diff --check` passed.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Remaining Items
- AC-3 remains incomplete outside this custom-ISD/lowering slice: BaseInfo,
  TableGen/RegisterInfo, Subtarget VLEN/YSXVec helpers, generated vector pseudo
  tables, and dead Disassembler FP/vector helpers still need pruning.
- Some copied YSX CodeGen files still carry non-active or stale check-prefix
  surfaces beyond the 17 regenerated files; this stays queued behind source
  pruning.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: No reusable new failure pattern was discovered; build/test failures
  were direct consequences of the intentional lowering prune and were handled
  by YSX-owned check regeneration.
