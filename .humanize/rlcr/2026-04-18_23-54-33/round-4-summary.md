# Round 4 Summary

## Work Completed
- Removed the remaining default MC-visible non-rv64ima instruction surface identified by Round 3 review:
  `fence.i`, CSR instructions/aliases, counter aliases, privileged/debug/hypervisor mnemonics, and their SelectionDAG patterns.
- Replaced YSX symbolic CSR generation with an empty hand-written lookup path. `YSXSystemOperands.td` now carries no `SysReg` records.
- Replaced `YSXFeatures.td` with a 293-line rv64ima-only feature surface: I, M/Zmmul, A/Zaamo/Zalrsc, 64bit, relax/exact-asm, register reservation, and required LLVM tuning/internal controls.
- Removed direct C++ feature-bit dependencies for unsupported E/C/F/D/Zfinx/V/Ztso/Zalasr/vendor surfaces and fixed unsupported getters to return YSX v1 constants.
- Disabled unsupported interrupt/counter lowering paths so they no longer require CSR or privileged opcodes.
- Updated YSX MC tests for the rv64ima surface, including negative coverage for all Round 3 review examples.

## Files Changed
- Core YSX backend/MC files under `llvm/lib/Target/YuShuXin`, especially `YSXInstrInfo.td`, `YSXFeatures.td`, `YSXSystemOperands.td`, `YSXISelLowering.cpp`, `YSXFrameLowering.cpp`, `YSXSubtarget.h`, and MC parser/printer/streamer helpers.
- YSX MC tests: `unsupported-features.s`, `align.s`, `debug-valid.s`, and `rv64i-aliases-invalid.s`.
- Round metadata: `round-4-contract.md`, this summary, and stop-gate generated tracker/review files.

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `build_ysx_riscv_host_llvm`.
- PASS: manual MC rejection check for `fence.i`, `csrr ...`, `rdcycle/rdtime/rdinstret`, `mret/sret/wfi/dret`, `sfence.vma`, and `hfence.vvma`.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- PASS: source scan found no backend matches for the removed CSR/privileged opcode defs, `YSXDisabled`, or removed feature records such as Zicsr/Zifencei/Zve/Zvl/F/D/Zca/Zbb.
- Line counts: RISCV backend remains 136,068 lines / 188 files; YSX backend is now 59,953 lines / 86 files. This round removed a net 2,643 lines from YSX/test diff and reduced YSX by about 2,653 backend lines from the previous 62,606-line checkpoint.

## Remaining Items
- AC-2 is materially addressed for the reviewed MC leaks.
- AC-3 is advanced but not complete: large copied `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrFormats.td`, frame/vector metadata, and false-return compatibility APIs still need deletion in later rounds.
- Some unsupported code remains unreachable through fixed false helpers; this is intentionally left for the next pruning round rather than blocking this round's MC/feature-surface objective.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output rather than actionable lesson IDs; no lesson change was made.
