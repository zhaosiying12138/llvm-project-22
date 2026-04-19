# Round 2 Summary

## Work Completed
- Fixed YSX assembler option validation for the reviewed AC-2 gap: `.option arch,+f`, `+c`, `+zbb`, `+v`, and `.option rvc` now reject with `YSX only supports arch string rv64ima`, and failed incremental feature updates restore the previous feature bits.
- Added/updated MC regression coverage for unsupported `-mattr` features, full and incremental `.option arch` strings, `.option rvc`, `.option norvc`, and push/pop rollback after a failed unsupported toggle.
- Removed `MCTargetDesc/YSXUnsupportedOpcodes.h` and deleted or neutralized the stale generated-reference code paths that required it.
- Added a YSX feature whitelist in subtarget/MC construction so command-line unsupported ISA features fail early with `YSX only supports the rv64ima ISA`.
- Removed the YSX compressed-instruction tablegen product and replaced YSX RVC compression/uncompression hooks with rv64ima-only no-op implementations.
- Replaced the inherited RVV MCA instrumentation with a minimal YSX instrument manager that disables MCA instruments instead of referencing vector pseudo tables.
- Cleaned selected stale helper code and exact reviewed blocker strings from `llvm/lib/Target/YuShuXin` without changing `llvm/lib/Target/RISCV`.

## Files Changed
- YSX backend sources under `llvm/lib/Target/YuShuXin`, including AsmParser, MCTargetDesc, MCA, SelectionDAG, InstrInfo, Subtarget, RegisterInfo, feature/format TD files, and related headers.
- YSX negative coverage in `llvm/test/MC/YSX/unsupported-features.s`.
- RLCR tracking files in `.humanize/rlcr/2026-04-18_23-54-33/`.

## Validation
- PASS: `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld` in `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm`.
- PASS: `ninja LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang opt lld` in `/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm`.
- PASS: `./bin/llvm-lit -q llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX` from the YSX-only build directory, with 130 discovered tests.
- PASS: `./bin/llvm-mc -triple=ysx64 -mattr=+f,+c,+zbb,+v /dev/null` fails with `LLVM ERROR: YSX only supports the rv64ima ISA`.
- PASS: `printf '.option arch, +f\n.option rvc\n' | ./bin/llvm-mc -triple=ysx64-unknown-elf -` rejects both directives with the YSX rv64ima diagnostic.
- PASS: `rg -n "FeatureStdExtF|FeatureStdExtC|FeatureStdExtV|FeatureVendor|RVV|XSf|XTHead|Xqci|LD_RV32|YSXUnsupportedOpcodes|YSXGenCompressInstEmitter|getVXMemOpInfo|YSXVInversePseudosTable" llvm/lib/Target/YuShuXin` has no matches.
- PASS: `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## Remaining Items
- The YSX source tree is still about 66k lines of `.cpp/.h/.td`, above the desired roughly 30k target. This round removed the reviewed externally observable ISA enablement bugs and the explicit unsupported opcode/compress/MCA surfaces, but deeper semantic deletion of renamed disabled TD/lowering scaffolding remains a review risk for AC-3.
- The disassembler still emits unused-helper warnings for decoder callbacks inherited from removed FP/C/vector/vendor surfaces. They are not linked failures, but they indicate additional TD/disassembler pruning is still possible.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: The selector output did not identify a usable project-specific lesson; no lesson file changes were made.
