# Round 5 Summary

## Work Completed
- Fixed the supported YSX Clang compile path by filtering inherited disabled RISCV target features before YSX subtarget parsing.
- Preserved strict rejection for enabled unsupported features while allowing harmless disabled inherited features such as `-f`, `-v`, and `-zbb`.
- Kept `llvm-mc -mattr=help` on the curated YSX help surface so help output does not expose copied 32-bit/vector tuning feature names.
- Added real Clang compile coverage for default `ysx64-unknown-elf` and explicit `-march=rv64ima`.
- Added MC coverage that disabled unsupported inherited features do not reject valid rv64ima assembly.
- Updated the mutable goal tracker to mark the Clang compile-path blocker resolved and keep AC-3 source pruning active.

## Files Changed
- `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXMCTargetDesc.cpp`
- `llvm/lib/Target/YuShuXin/YSXSubtarget.cpp`
- `clang/test/Driver/YSX/target-options.c`
- `llvm/test/MC/YSX/unsupported-features.s`
- `.humanize/rlcr/2026-04-18_23-54-33/round-5-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja LLVMYSXCodeGen clang llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `clang --target=ysx64-unknown-elf -c` produces a 64-bit RISC-V ELF relocatable object.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64ima -c` produces a 64-bit RISC-V ELF relocatable object.
- PASS: `clang --target=ysx64-unknown-elf -march=rv64imaf/-march=rv64imac -c` rejects with `YuShuXin only supports -march=rv64ima`.
- PASS: `llvm-mc -triple=ysx64 -mattr=-f,-v,-zbb` assembles valid `add` input.
- PASS: `llvm-mc -triple=ysx64 -mattr=help` lists only the curated YSX CPU/features: rv64, I/M/A, Zmmul, Zaamo, Zalrsc, relax, and exact-asm.
- PASS: `llvm-mc -triple=ysx64 -mattr=+f` and `llc -mtriple=ysx64-unknown-elf -mattr=+v` still reject with `YSX only supports the rv64ima ISA`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in `build_ysx_riscv_host_llvm`.
- PASS: Combined build `clang --target=ysx64-unknown-elf` and explicit `-march=rv64ima` smoke compiles both produce ELF objects.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.

## Remaining Items
- AC-3 remains active: `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, `YSXSubtarget.h`, `YSX.h`, frame helpers, and TargetParser/Clang parser aliases still carry copied unsupported source surface.
- AC-4 is improved for the real Clang compile path, but future source-pruning rounds should keep rerunning the same Clang compile tests.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for implementation and validation tasks, so no actionable lesson was selected or added.
