# Round 8 Summary

## Work Completed
- Replaced the broad copied `YSXISAInfo::parseArchString` implementation with
  an exact `rv64ima` parser. The YSX parser no longer accepts RV32, `g`, FP,
  compressed, vector, or arbitrary `z`/`s`/`x` extension groups before later
  validation rejects them.
- Tightened `YSXISAInfo::parseFeatures` to reject non-64-bit feature sets and
  enabled unsupported features instead of adding unknown extensions.
- Removed stale non-IMA pass declarations from `YSX.h`, including copied
  compressed, gather/scatter, vector peephole, VSETVLI, VXRM, VL optimizer, and
  related declarations that are not implemented or not part of YSX `rv64ima`.
- Removed the unused `createYSXVectorMaskDAGMutation` declaration from
  `YSXTargetMachine.h`.
- Added MC coverage proving `.option arch, rv64ima` is accepted while full arch
  strings such as `rv32ima` and `rv64ima_zbb` reject.
- Updated the mutable goal tracker to record Round 8 as an AC-3 source-pruning
  slice while keeping the larger lowering/selection/vector-helper deletion work
  active.

## Files Changed
- `llvm/include/llvm/TargetParser/YSXISAInfo.h`
- `llvm/lib/Target/YuShuXin/YSX.h`
- `llvm/lib/Target/YuShuXin/YSXTargetMachine.h`
- `llvm/test/MC/YSX/unsupported-features.s`
- `.humanize/rlcr/2026-04-18_23-54-33/round-8-contract.md`
- `.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md`

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `ninja clang LLVMYSXCodeGen llvm-mc` in `build_ysx_only_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX/unsupported-features.s`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` in `build_ysx_only_host_llvm` (130 tests).
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` in
  `build_ysx_riscv_host_llvm`.
- PASS: combined build smoke compiles for `ysx64-unknown-elf` and
  `riscv64-unknown-elf` produce ELF64 RISC-V relocatable objects.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Line-count status: `llvm/lib/Target/YuShuXin` is now 60,000 lines, down from
  60,045 before Round 8. `llvm/lib/Target/RISCV` remains 136,068 lines.

## Remaining Items
- AC-3 remains active. The direct `YSXVType` forwarding layer still exists
  because it has live callers across `YSXISelLowering`, `YSXISelDAGToDAG`,
  `YSXInstrInfo`, MC printing, and the asm parser; deleting it requires the
  next larger lowering/selection/MC cleanup slice.
- Retained vector/FP/vendor/RV32/compressed lowering, instruction metadata,
  frame helpers, and false-return subtarget APIs still need deletion.
- `YSXDisassembler.cpp` now visibly reports dead compressed/vector/FP decoder
  helpers as unused warnings; those are queued for the next AC-3 source-pruning
  slice.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: `bitlesson-selector` returned its placeholder output for the Round 8
  contract, audit, implementation, and validation tasks, so no actionable
  lesson was selected or added.
