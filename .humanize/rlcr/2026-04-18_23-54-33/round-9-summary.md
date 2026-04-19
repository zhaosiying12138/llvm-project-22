# Round 9 Summary

## Work Completed

- Wrote the Round 9 contract with one mainline objective: remove the
  `YSXVType` forwarding layer and the MC-visible copied tune/profile feature
  surface.
- Removed `namespace YSXVType` from `YSXISAInfo.h` and eliminated all
  `YSXVType` references under YSX.
- Disabled YSX MC VTYPE text parsing and vector VTYPE pretty-printing paths.
- Removed copied vector/vendor/profile tune feature definitions for
  `log-vrgather`, `enable-vsetvli-sched-heuristic`,
  `optimized-zero-stride-load`, `optimized-nf*-segment-load-store`,
  `vl-dependent-latency`, `dlen-factor-2`, `no-sink-splat-operands`,
  `conditional-cmv-fusion`, `single-element-vec-fp64`,
  `vxrm-pipeline-flush`, `prefer-vsetvli-over-read-vlenb`, and vendor profile
  selectors.
- Tightened both YSX feature filters so enabled MC/subtarget `-mattr` features
  outside `64bit`, `i`, `m`, `a`, `zmmul`, `zaamo`, `zalrsc`, `relax`, and
  `exact-asm` report `YSX only supports the rv64ima ISA`.
- Added negative MC coverage for `+vxrm-pipeline-flush`, `+log-vrgather`,
  `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and
  `+andes45`.

## Files Changed

- `.humanize/rlcr/2026-04-18_23-54-33/round-9-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`
- `llvm/include/llvm/TargetParser/YSXISAInfo.h`
- `llvm/lib/Target/YuShuXin/YSXFeatures.td`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.{h,cpp}`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp`
- `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp`
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXInstPrinter.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp`
- `llvm/lib/Target/YuShuXin/YSXISelLowering.{h,cpp}`
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- `llvm/lib/Target/YuShuXin/YSXExpandPseudoInsts.cpp`
- `llvm/lib/Target/YuShuXin/YSXInstrPredicates.td`
- `llvm/lib/Target/YuShuXin/{MCTargetDesc/YSXBaseInfo.h,YSXRegisterInfo.h}`
- `llvm/test/MC/YSX/unsupported-features.s`

## Validation

- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX/unsupported-features.s`
- PASS: `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` discovered 130 tests.
- PASS: YSX-only clang smoke compile for `--target=ysx64-unknown-elf` produced an ELF64 RISC-V soft-float relocatable.
- PASS: YSX-only `-###` emits only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax`.
- PASS: Manual YSX-only and combined `llvm-mc` probes reject `+vxrm-pipeline-flush`, `+log-vrgather`, `+single-element-vec-fp64`, `+prefer-vsetvli-over-read-vlenb`, and `+andes45`.
- PASS: `ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`
- PASS: Combined YSX and RISCV clang smoke compiles both produced ELF64 RISC-V soft-float relocatables.
- PASS: `git diff --check`
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Line Counts

- Original RISCV backend: 136,068 lines across 188 files.
- Current YSX backend: 59,449 lines across 86 files.
- Current reduction from original RISCV: 76,619 lines, about 56.3%.
- Round 9 YSX backend reduction: 551 lines.

## Remaining Items

- Direct copied vector helper use remains through `RISCVVType` in lowering,
  DAG selection, and register metadata. This is not a YSX forwarding shim, but
  it still represents retained copied vector source that must be deleted in
  later AC-3 pruning.
- Large copied vector/FP/RV32/compressed bodies remain in `YSXISelLowering`,
  `YSXISelDAGToDAG`, `YSXInstrInfo`, TableGen metadata, frame lowering, and
  disassembler helpers.
- The current 59,449-line backend likely needs several more large deletion
  rounds to approach the expected 30,000-line target.

## BitLesson Delta

- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for each Round 9
  task, so it was treated as `NONE`.
