# Round 0 Summary

## What Was Implemented

Configured the RISCV-only build, implemented the default-off Yushuxin vector
`llvm.exp` lowering path, added the hidden RVV pressure-aware scheduler option,
added the conservative RVV load reload pass, wired frame diagnostics, and added
the required workload/report artifacts.

## Files Changed

- `llvm/lib/Target/RISCV/`: feature, instruction, pseudo, lowering, scheduler,
  reload pass, frame diagnostics, and pipeline wiring.
- `llvm/include/llvm/Target/TargetSelectionDAG.td`: SelectionDAG `fexp` pattern
  node exposure for TableGen matching.
- `llvm/test/CodeGen/RISCV/rvv/`: Yushuxin lowering test and pressure workload
  regression file.
- `llvm/test/MC/RISCV/rvv/`: Yushuxin asm/disasm feature-gating test.
- `llvm/test/CodeGen/RISCV/features-info.ll`: feature help coverage.
- `docs/report.md`: build commands, measured spill/reload counts, diagnostics,
  and before/after snippets.

## Validation

- `cmake -S llvm -B build-riscv -G Ninja -DLLVM_TARGETS_TO_BUILD=RISCV -DLLVM_ENABLE_PROJECTS='clang;lld' -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_CCACHE_BUILD=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_USE_LINKER=lld`
- `ninja -C build-riscv llc clang lld FileCheck llvm-mc llvm-objdump count not llvm-config llvm-readobj`
- `build-riscv/bin/llvm-lit -sv llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll llvm/test/MC/RISCV/rvv/yushuxin-vfexp.s llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll llvm/test/CodeGen/RISCV/features-info.ll` passed 4/4.
- `build-riscv/bin/llc -mtriple=riscv64 -mattr=help 2>&1 | rg -n "experimental-yushuxin|Yushuxin"` showed the feature help entry.

## Remaining Items

No implementation items are intentionally deferred. Strict `fexp`, GlobalISel,
dynamic-loop cases, and unsafe alias speculation remain out of scope per plan.

## BitLesson Delta

Action: none
Lesson ID(s): NONE
Notes: No reusable lesson was added; the selector produced placeholder output
and `.humanize/bitlesson.md` had no applicable entries.
