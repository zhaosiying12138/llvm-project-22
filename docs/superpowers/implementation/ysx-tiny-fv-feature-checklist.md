# YSX Tiny-F/Tiny-V Feature Checklist

## Implemented Features

- Spec approved: docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md

## Generated Surfaces

- Task 3 added auto-td schemas, taxonomy YAML, and proof instruction YAML.
- Task 6 hooks auto-td generation into the YSX LLVM target CMake as build-tree outputs under the target binary directory's `auto-td` subdirectory.
- Task 6 wires the generated target TableGen fragments into `YSXInstrInfo.td`: `YSXGenAutoTinyFInstrInfo.inc`, `YSXGenAutoTinyVInstrInfo.inc`, `YSXGenAutoTinyVPseudos.inc`, and `YSXGenAutoTinyVPatterns.inc`.
- Task 6 generates `YSXGenAutoTinyVBuiltins.inc` as a build output/dependency only; it is intentionally not included by LLVM target TableGen yet.

## Vendor Evidence

- Task 2 snapshots: `third_party/riscv-opcodes` at upstream `ef103b65c682e7cb705cff67898c515f5c63175c` and `third_party/riscv-isa-manual` at upstream `2d034e16e3edeaa631aeb863adf8ef3a0b743aad`; both were copied from shallow clones and their upstream `.git` directories were removed.
- Task 2 custom opcode: `third_party/ysx-opcodes/extensions/rv_xtinyv`.

## Handwritten Glue

- Task 5 added YSX feature scaffolding for `xtinyf`, `xtinyv`, and `zvl128b`, with `xtinyv` implying the 128-bit vector length lower bound in the TableGen feature definition and YSX ISA parser paths.
- Task 5 kept default YSX at rv64ima/lp64 and retained the full F/V/C rejection filters while allowing the new YSX-specific optional feature names through Clang driver/frontend, TargetParser, MC, and Subtarget filters.
- Task 5 added frontend macro support for `__riscv_xtinyf`, `__riscv_xtinyv`, and `__riscv_zvl128b` without enabling generic RVV builtins/types or `__riscv_vector` / `__riscv_v_intrinsic` for YSX.
- Task 5 added FPR32, tiny vector M1, mask, and minimal `vl`/`vtype` register scaffolding, plus minimal future instruction format/opcode metadata for `LOAD_FP`, `STORE_FP`, `OP_FP`, `OP_V`, `CUSTOM_0`, and R4 format.
- Task 5 review fix made YSX ISA parsing accept emitted versioned tiny extension forms (`xtinyf1p0`, `xtinyv1p0`, `zvl128b1p0`) and made `.attribute arch` re-emit the parsed canonical arch attribute so tiny features are preserved.
- Task 6 added CMake dependency tracking for the auto-td generator Python files, instruction/schema/taxonomy YAML files, and opcode extension source files from `third_party/riscv-opcodes` and `third_party/ysx-opcodes`.

## Validation Evidence

- Baseline configure command: `cmake -G Ninja -S llvm -B build -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD="YSX" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON`
- Baseline configure result: passed; `build/build.ninja` was generated.
- Baseline build command: `ninja -C build clang llc llvm-mc llvm-objdump`
- Baseline build result: failed at final `bin/clang-22` link with `/usr/bin/ld: final link failed: No space left on device`; `llc`, `llvm-mc`, and `llvm-objdump` linked before the failure, but `clang` was not produced.
- Disk snapshot at failure review time: `df -h . build /tmp` reported `/dev/sde` at `100%` used with `137M` available for all three paths; `df -ih . build /tmp` reported inode use at `14%`, so the blocker is block storage, not inode exhaustion.
- Baseline validation command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- Baseline validation result: not run because the baseline tool build failed before producing `build/bin/clang`.
- Task 5 test-first file: `clang/test/Driver/YSX/tiny-fv-target-options.c`.
- Task 5 RED attempt: `build/bin/llvm-lit -sv clang/test/Driver/YSX/tiny-fv-target-options.c`
- Task 5 RED result: not runnable; `/bin/bash: line 1: build/bin/llvm-lit: No such file or directory`. Additional checks showed `build/bin/clang`, `build/bin/llvm-lit`, `build/bin/FileCheck`, `build/bin/llvm-tblgen`, and `build/build.ninja` are absent in this worktree.
- Task 5 disk snapshot before implementation: `df -h . build` reported `/dev/sde` at `100%` used with `1.2G` available, so no full build was attempted.
- Task 5 source-level smoke attempt: `/home/zhaosiying/codebase/software/LLVM-19.1.3-Linux-X64/bin/llvm-tblgen -gen-register-info -I llvm/lib/Target/YuShuXin -I llvm/include llvm/lib/Target/YuShuXin/YSX.td -o /tmp/ysx-reginfo.inc`
- Task 5 source-level smoke result: not valid for this tree; LLVM 19 `llvm-tblgen` failed on LLVM 22 TableGen syntax with `llvm/include/llvm/IR/Intrinsics.td:687:23: error: Unknown operator` for `!listflatten`.
- Task 5 cheap regression command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests`
- Task 5 cheap regression result: passed, latest run reported `Ran 11 tests in 0.173s`, `OK`.
- Task 5 whitespace check command: `git diff --check`
- Task 5 whitespace check result: passed with no output.
- Task 5 review-fix test-first file: `llvm/test/MC/YSX/tiny-fv-attributes.s`.
- Task 5 review-fix RED attempt: `build/bin/llvm-lit -sv llvm/test/MC/YSX/tiny-fv-attributes.s`
- Task 5 review-fix RED result: not runnable; `/bin/bash: line 1: build/bin/llvm-lit: No such file or directory`.
- Task 5 review-fix cheap regression command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests`
- Task 5 review-fix cheap regression result: passed, `Ran 11 tests in 0.122s`, `OK`.
- Task 5 review-fix tblgen smoke attempt: `/home/zhaosiying/codebase/software/LLVM-19.1.3-Linux-X64/bin/llvm-tblgen -gen-register-info -I llvm/lib/Target/YuShuXin -I llvm/include llvm/lib/Target/YuShuXin/YSX.td -o /tmp/ysx-reginfo-review-fix.inc`
- Task 5 review-fix tblgen smoke result: not valid for this tree; LLVM 19 `llvm-tblgen` failed on LLVM 22 TableGen syntax with `llvm/include/llvm/IR/Intrinsics.td:687:23: error: Unknown operator` for `!listflatten`.
- Task 5 review-fix whitespace check command: `git diff --check`
- Task 5 review-fix whitespace check result: passed with no output.
- Task 6 source-level test-first file: `llvm/lib/Target/YuShuXin/auto-td/tests/test_cmake_integration.py`.
- Task 6 RED command: `python3 llvm/lib/Target/YuShuXin/auto-td/tests/test_cmake_integration.py -q`
- Task 6 RED result: failed as expected before production edits, `Ran 5 tests in 0.001s`, `FAILED (failures=5)`, with failures for missing build-tree output definitions, generator command wiring, dependency glob coverage, `LLVM_TARGET_DEPENDS`, and generated TD includes.
- Task 6 source-level regression command: `python3 llvm/lib/Target/YuShuXin/auto-td/tests/test_cmake_integration.py -q`
- Task 6 source-level regression result: passed, `Ran 5 tests in 0.001s`, `OK`.
- Task 6 auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Task 6 auto-td unittest result: passed, `Ran 16 tests in 0.181s`, `OK`.
- Task 6 generator smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-task6 --coverage build/ysx-auto-td-task6/coverage.md`
- Task 6 generator smoke result: passed with no stdout/stderr; generated five TD stubs and `coverage.md` under ignored `build/ysx-auto-td-task6`.

## Retained Schema Gaps

- No retained schema gaps recorded yet.

## Blog Notes

- The standalone final blog at `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md` should describe auto-td-gen, tiny-F/tiny-V, builtin proof, automatic vectorization, and yushuxin.vfexp.
