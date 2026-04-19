# Round 20 Summary

## Work Completed
- Wrote the Round 20 contract before implementation and kept the objective
  scoped to the Round 19 review blocker.
- Deleted dead Zibi/CLUI/VTYPE/RVKR operand enum entries, validators, asm parser
  helpers, and dead Zibi/slist code-emitter helpers.
- Removed the no-op `YSXInsertReadWriteCSR` pass from sources, CMake, target
  initialization, and the pre-regalloc pipeline.
- Removed dead CSR parser/printer/class scaffolding that had no retained
  rv64ima instruction users.
- Removed copied SiFive CLIC interrupt frame state, stubs, and call sites, plus
  the generic interrupt callee-save path now that YSX lowering rejects interrupt
  handlers.

## Files Changed
- `llvm/lib/Target/YuShuXin` source, TableGen, and CMake files only.
- `.humanize/rlcr/2026-04-18_23-54-33/round-20-contract.md`.
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md` mutable section only.

## Validation
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc`
- PASS: `env HUMANIZE_MAX_LINES=0 ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: `env HUMANIZE_MAX_LINES=0 /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- PASS: smoke compiles for YSX and combined RISCV+YSX produce ELF64 RISC-V
  soft-float relocatable objects.
- PASS: negative probes reject `+v`, `+32bit`, `+zca`, `+xventanacondops`,
  `.option arch, rv64ima_zbb`, `csrr`, `rv64imaf`, `ysx32`, scalable-vector IR,
  and direct `llvm.riscv.vsetvli`.
- PASS: `llvm-mc -mattr=help` contains none of the reviewed unsupported
  Zibi/RVKR/VTYPE/Zbb/Zca/vendor markers.
- PASS: `git diff --check`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` reports `0`.
- Line counts: RISCV backend `136,068`; YSX backend `26,678`; reduction
  `109,390` lines (`80.39%`).

## Remaining Items
- No known blocking implementation items remain from the Round 19 review.
- Queued non-blocking item remains unchanged: CPU/tune target-attribute
  warn-and-ignore diagnostics.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `.humanize/bitlesson.md` has no entries and the selector returned the
  placeholder output, so no lesson update was warranted.
