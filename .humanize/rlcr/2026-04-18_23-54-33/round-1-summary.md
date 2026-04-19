# Round 1 Summary

## Work Completed
- Pruned YSX to the retained rv64ima surface by removing unsupported instruction and scheduler TD files for C/F/D/Q/P/V/Z*/vendor extensions and excluding unsupported YSX codegen passes from CMake.
- Disabled or simplified stale FP, compressed, vector, RV32, vendor, and unsupported bitmanip paths in MC emission/parsing, disassembly, instruction info, frame/register lowering, DAG-to-DAG selection, and SelectionDAG lowering.
- Removed YSX-specific RVV/vector command-line knobs and RVV-only register allocator plumbing, and made copied subtarget vector/FP/vendor helper queries return unavailable.
- Added negative MC coverage for rejected extensions (`+f`, `+c`, `+zbb`) and `.option arch, rv64gc`, then refreshed YSX CodeGen checks against the pruned output.

## Files Changed
- `llvm/lib/Target/YuShuXin/**`: backend pruning, unsupported TD/source deletion, and compatibility stubs for remaining unreachable generated references.
- `llvm/test/MC/YSX/unsupported-features.s`: new negative coverage for unsupported ISA surfaces.
- `llvm/test/CodeGen/YSX/*.ll`: regenerated checks after pruning changed selected code sequences.
- `.humanize/rlcr/2026-04-18_23-54-33/*`: round-1 contract, prompt, review artifacts, tracker, and summary.

## Validation
- YSX-only build: `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld` in `build_ysx_only_host_llvm` passed.
- RISCV+YSX combined static build: `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld` in `build_ysx_riscv_host_llvm` passed.
- YSX regression subset: `llvm-lit -sv llvm/test/CodeGen/YSX llvm/test/MC/YSX clang/test/CodeGen/YSX clang/test/Driver/YSX` passed `130/130`.
- RISCV untouched check: `git diff -- llvm/lib/Target/RISCV | wc -l` returned `0`.
- Line-count check for YSX C++/H/TD files after deletion/subtarget cleanup returned `67095 total`.

## Remaining Items
- The backend still carries some generated-reference compatibility stubs to keep the copied LLVM integration buildable after deleting unsupported TD/source files. They are not exposed through YSX target validation and can be collapsed further in a follow-up if the acceptance bar requires a lower physical line count than this round reached.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: No reusable lesson change was identified in this round.
