# YSX Tiny-F/Tiny-V Feature Checklist

## Implemented Features

- Spec approved: docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md
- `xtinyf`, `xtinyv`, and `zvl128b` feature parsing and frontend/TargetParser/MC plumbing.
- `xtinyv` implies `zvl128b` on the YSX path.
- Generated tiny-f MC definitions for 15 selected scalar f32 instructions: `flw`, `fsw`, `fadd.s`, `fsub.s`, `fmul.s`, `fsgnj.s`, `feq.s`, `flt.s`, `fle.s`, `fcvt.w.s`, `fcvt.wu.s`, `fcvt.s.w`, `fcvt.s.wu`, `fmv.x.w`, and `fmv.w.x`.
- Generated tiny-v MC definitions for 35 selected e32 memory, integer ALU, reduction, shuffle/move, `vset*`, and custom instructions, including `vadd.vv`, `vsub.vv`, `vmul.vv`, `vredsum.vs`, `vfredusum.vs`, `vrgather.vv`, `vslideup.vx`, and `yushuxin.vfexp`.
- Generated tiny-v pseudo/pattern/builtin manifests for YAML-declared facts, including `ysx_vfexp_v_f32m1` and `intrinsic=ysx.vfexp`.
- Generated Clang builtin TD, LLVM intrinsic TD, and CGBuiltin dispatch fragments for the explicit `builtin.codegen: true` tiny-v proof APIs.
- YSX custom opcode source for `yushuxin.vfexp` in `third_party/ysx-opcodes`.
- `ysx_vector.h` typed proof APIs for `ysx_vadd_vv_i32m1`, `ysx_vsub_vv_i32m1`, `ysx_vmul_vv_i32m1`, `ysx_vredsum_vs_i32m1`, `ysx_vfredsum_vs_f32m1`, `ysx_vrgather_vv_i32m1`, `ysx_vslideup_vx_i32m1`, and `ysx_vfexp_v_f32m1`.
- Matching `__builtin_ysx_*` Clang proof builtins lowered to YSX-owned LLVM IR intrinsics.
- Fixed 128-bit proof backend path for `<4 x i32>` and `<4 x float>` tiny-v values: C builtin loads/stores select generated `vsetivli`, `vsetvli`, `vle32.v`, `vse32.v`, `vadd.vv`, `vsub.vv`, `vmul.vv`, `vredsum.vs`, `vfredusum.vs`, `vrgather.vv`, `vslideup.vx`, and `yushuxin.vfexp` records.
- Tiny-F LLVM CodeGen for f32 load/store, add/sub/mul, ordered eq/lt/le/gt/ge compare, i32/f32 conversion, FPR32 spills/reloads, and soft-float ABI moves through generated `fmv.x.w`/`fmv.w.x`.
- Tiny-F i64/f32 conversions and unordered f32 compares are explicitly rejected with YSX tiny-F diagnostics instead of selecting incorrect 32-bit instructions.
- O0 proof path handles vector frame-index loads/stores and basic VR copies/spills with generated `vsetivli`, `vle32.v`, `vse32.v`, and `vmv1r.v`.
- Separate Clang `BuiltinsYSX.td` shard for YSX builtins, avoiding accidental reuse of the much larger RISCV/RVV builtin ID table.
- No generic RVV frontend exposure for YSX: no `riscv_vector.h`, `__riscv_vector`, or `__riscv_v_intrinsic` surface is enabled by `xtinyv`.

## Generated Surfaces

- Task 3 added auto-td schemas, taxonomy YAML, and proof instruction YAML.
- Task 6 hooks auto-td generation into the YSX LLVM target CMake as build-tree outputs under the target binary directory's `auto-td` subdirectory.
- Task 6 wires the generated target TableGen fragments into `YSXInstrInfo.td`: `YSXGenAutoTinyFInstrInfo.inc`, `YSXGenAutoTinyVInstrInfo.inc`, `YSXGenAutoTinyVPseudos.inc`, and `YSXGenAutoTinyVPatterns.inc`.
- Task 6 generates `YSXGenAutoTinyVBuiltins.inc` as a build output/dependency only; it is intentionally not included by LLVM target TableGen yet.
- Priority-reset continuation changed `YSXGenAutoTinyVPseudos.inc`, `YSXGenAutoTinyVPatterns.inc`, and `YSXGenAutoTinyVBuiltins.inc` from empty placeholders into generated manifest files for YAML pseudo, pattern, and builtin facts.
- Current continuation adds generated Clang/LLVM consumer fragments for the same YAML facts: `YSXGenAutoTinyVClangBuiltins.td`, `YSXGenAutoTinyVBuiltinCG.inc`, and `YSXGenAutoTinyVIntrinsics.td`.
- Task 7 and the continuation work emit real tiny-f/tiny-v MC instruction records from YAML plus opcode-source fixed bits and `riscv-opcodes/arg_lut.csv` operand field ranges. Current generated coverage is 50 `auto_full` instructions and 0 retained schema gaps.
- Task 7 review fix makes the emitter consume taxonomy operand/effect records instead of inferring assembly operand shape from field-name heuristics.
- Task 7 generated mask asm strings intentionally follow the upstream RISCV `$operand$vm` convention; `printVMaskReg` emits the leading comma for explicit `v0.t`.
- Final review fix models `VL`/`VTYPE` explicitly in auto-td taxonomy/effects: generated vector memory/ALU/reduce/custom records use `[VL, VTYPE]`, and generated `vsetvli`/`vsetivli` define them.

## Vendor Evidence

- Task 2 snapshots: `third_party/riscv-opcodes` at upstream `ef103b65c682e7cb705cff67898c515f5c63175c` and `third_party/riscv-isa-manual` at upstream `2d034e16e3edeaa631aeb863adf8ef3a0b743aad`; both were copied from shallow clones and their upstream `.git` directories were removed.
- Task 2 custom opcode: `third_party/ysx-opcodes/extensions/rv_xtinyv`.

## Handwritten Glue

- Task 5 added YSX feature scaffolding for `xtinyf`, `xtinyv`, and `zvl128b`, with `xtinyv` implying the 128-bit vector length lower bound in the TableGen feature definition and YSX ISA parser paths.
- Task 5 kept default YSX at rv64ima/lp64 and retained the full F/V/C rejection filters while allowing the new YSX-specific optional feature names through Clang driver/frontend, TargetParser, MC, and Subtarget filters.
- Task 5 added frontend macro support for `__riscv_xtinyf`, `__riscv_xtinyv`, and `__riscv_zvl128b` without enabling generic RVV builtins/types or `__riscv_vector` / `__riscv_v_intrinsic` for YSX.
- Task 5 added FPR32, tiny vector M1, mask, and minimal `vl`/`vtype` register scaffolding, plus minimal future instruction format/opcode metadata for `LOAD_FP`, `STORE_FP`, `OP_FP`, `OP_V`, `CUSTOM_0`, and R4 format.
- Task 5 review fix made YSX ISA parsing accept emitted versioned tiny extension forms (`xtinyf1p0`, `xtinyv1p0`, `zvl128b1p0`) and made `.attribute arch` re-emit the parsed canonical arch attribute so tiny features are preserved.
- Task 6 added CMake dependency tracking for the auto-td generator Python files, instruction/schema/taxonomy YAML files, opcode extension source files, and opcode `arg_lut.csv` inputs from `third_party/riscv-opcodes` and `third_party/ysx-opcodes`.
- Task 7 added the minimal YSX MC glue required by generated tiny-v instructions: optional `v0.t` mask parsing/defaulting, mask printing and encoding, vector register disassembly decode helpers, and `.insn` major-opcode retention for `LOAD_FP`, `STORE_FP`, `OP_FP`, `OP_V`, and `CUSTOM_0`.
- Task 7 review fix adds a target operand type for generated vector masks (`YSXOp::OPERAND_VMASK`) and rejects malformed alias entries instead of silently dropping them.
- Task 8 added the first Clang tiny-v builtin proof path: `clang/lib/Headers/ysx_vector.h`, YSX-prefixed builtin declaration `__builtin_ysx_vadd_vv_i32m1`, private frontend macro `__YSX_TINY_VECTOR__`, and LLVM IR intrinsic `llvm.ysx.vadd`.
- Task 8 intentionally models the proof vector type with fixed 128-bit Clang extended vectors (`_ExtVector<4, int>`) instead of standard RVV frontend types, so YSX still does not expose generic RVV resource-header types or macros.
- Task 11 added the matching Clang IR proof path for the custom tiny-v instruction `yushuxin.vfexp`: `ysx_vfexp_v_f32m1` in `ysx_vector.h`, YSX-prefixed builtin declaration `__builtin_ysx_vfexp_v_f32m1`, and LLVM IR intrinsic `llvm.ysx.vfexp`.
- Current continuation generates the Clang builtin, LLVM intrinsic, and CGBuiltin dispatch pieces for YAML entries marked `builtin.codegen: true`. Public `ysx_vector.h` wrappers and backend selector glue remain bounded and explicit.
- Final proof fix routes `ysx64` target builtins through `EmitRISCVBuiltinExpr` while registering only the YSX builtin shard, so YSX can reuse the RISCV builtin lowering function without importing RISCV/RVV builtin declarations.
- Final backend proof fix makes `v4i32` and `v4f32` legal in the YSX vector register class and manually selects the proof intrinsic/load/store DAG nodes to generated auto-td instruction records.
- Final review fix inserts `vsetivli` for fixed 4-lane proof vector memory operations and `vsetvli` for builtin operations using the user-supplied `vl`.
- Final review fix materializes frame-index vector memory bases through a GPR scratch and adds basic VR `copyPhysReg`, `loadRegFromStackSlot`, and `storeRegToStackSlot` support with generated `vmv1r.v`, `vle32.v`, and `vse32.v`.
- Continuation tiny-F backend fix adds generated `fsgnj.s`, `fmv.x.w`, and `fmv.w.x` support to FPR32 copies, soft-float ABI moves, and scalar f32 stack traffic.
- Continuation tiny-V decoder fix gives `vmerge.vvm` carry-in mask operands a strict decoder, so bit 25 = 1 is rejected during disassembly instead of becoming `NoRegister`.

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
- Task 7 test-first files: `llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py` and `llvm/lib/Target/YuShuXin/auto-td/tests/test_tinyv_mc_support.py`.
- Task 7 RED command: `python3 -m unittest llvm/lib/Target/YuShuXin/auto-td/tests/test_generator.py -v` and `python3 -m unittest llvm/lib/Target/YuShuXin/auto-td/tests/test_tinyv_mc_support.py -v`.
- Task 7 RED result: failed as expected before production edits; generator output was still `// generated by ysx_auto_td_gen`, and static MC support checks failed for missing `parseVMaskReg`, `printVMaskReg`, `getVMaskReg`, vector decode helpers, and retained tiny-F/tiny-V major opcodes.
- Task 7 auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Task 7 auto-td unittest result: passed, `Ran 24 tests in 0.212s`, `OK`.
- Task 7 generator smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-task7 --coverage build/ysx-auto-td-task7/coverage.md`
- Task 7 generator smoke result: passed with no stdout/stderr; generated real tiny-v TableGen records and coverage for the original five proof instructions under ignored `build/ysx-auto-td-task7`.
- Task 7 MC lit proof file: `llvm/test/MC/YSX/tinyv-auto-td.s`.
- Task 7 MC lit command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyv-auto-td.s`
- Task 7 MC lit result: not runnable in this worktree because `build/bin/llvm-lit`, `build/bin/llvm-mc`, and `build/bin/llvm-objdump` are absent after the earlier disk-space-limited build cleanup; current `df -h . build /tmp` still reports `/dev/sde` at `100%` with about `1.2G` available.
- Task 7 Python compile command: `python3 -m compileall -q llvm/lib/Target/YuShuXin/auto-td/tools llvm/lib/Target/YuShuXin/auto-td/tests`
- Task 7 Python compile result: passed with no output.
- Task 7 whitespace check command: `git diff --check`
- Task 7 whitespace check result: passed with no output.
- Task 8 test-first files: `llvm/lib/Target/YuShuXin/auto-td/tests/test_clang_builtin_support.py` and `clang/test/CodeGen/YSX/tinyv-builtins.c`.
- Task 8 RED command: `python3 -m unittest llvm/lib/Target/YuShuXin/auto-td/tests/test_clang_builtin_support.py -v`
- Task 8 RED result: failed as expected before production edits; failures reported missing `ysx_vector.h`, missing YSX builtin declaration, missing `IntrinsicsYSX.td`, missing `IntrinsicsYSX.h` CodeGen include, and missing private vector header guard macro.
- Task 8 auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Task 8 auto-td unittest result: passed, `Ran 31 tests in 0.179s`, `OK`.
- Task 8 generator smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-task8 --coverage build/ysx-auto-td-task8/coverage.md`
- Task 8 generator smoke result: passed with no stdout/stderr.
- Task 8 Python compile command: `python3 -m compileall -q llvm/lib/Target/YuShuXin/auto-td/tools llvm/lib/Target/YuShuXin/auto-td/tests`
- Task 8 Python compile result: passed with no output.
- Task 8 whitespace check command: `git diff --check`
- Task 8 whitespace check result: passed with no output.
- Task 8 Clang lit proof file: `clang/test/CodeGen/YSX/tinyv-builtins.c`.
- Task 8 Clang lit command: `python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-builtins.c`
- Task 8 Clang lit result: not runnable in this worktree for the same disk-space-limited build reason as Task 7; `build/bin/llvm-lit`, `build/bin/clang`, and generated TableGen headers such as `build/include/llvm/IR/IntrinsicsYSX.h` are absent.
- Task 11 custom builtin test-first file: `clang/test/CodeGen/YSX/yushuxin-vfexp.c`.
- Task 11 RED command: `python3 -m unittest llvm/lib/Target/YuShuXin/auto-td/tests/test_clang_builtin_support.py -v`
- Task 11 RED result: failed as expected before production edits; failures reported missing `__builtin_ysx_vfexp_v_f32m1`, missing `def vfexp_v_f32m1`, missing `int_ysx_vfexp`, and missing CodeGen lowering case.
- Task 11 auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Task 11 auto-td unittest result: passed, `Ran 31 tests in 0.285s`, `OK`.
- Task 11 custom encoding smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-vfexp --coverage build/ysx-auto-td-vfexp/coverage.md` and `rg -n "yushuxin.vfexp|ysx-opcodes/rv_xtinyv/yushuxin_vfexp" build/ysx-auto-td-vfexp/coverage.md build/ysx-auto-td-vfexp/YSXGenAutoTinyVInstrInfo.inc`
- Task 11 custom encoding smoke result: passed; coverage reports `yushuxin.vfexp` from `ysx-opcodes/rv_xtinyv/yushuxin_vfexp`, and generated TD contains `YSX_AUTO_YUSHUXIN_VFEXP`.
- Task 11 Clang lit command: `python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/yushuxin-vfexp.c`
- Task 11 Clang lit result: not runnable in this worktree because `build/bin/llvm-lit` is absent.
- Post-cleanup build command: `ninja -C build clang llc llvm-mc llvm-objdump FileCheck`
- Post-cleanup build result: passed; the required YSX and Clang tools were produced in this worktree's `build/bin`.
- Final build command: `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- Final build result: passed; fresh final run exited 0 with `ninja: no work to do`.
- Final auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Final auto-td unittest result: passed, `Ran 36 tests`, `OK`.
- Final generator smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-final7 --coverage build/ysx-auto-td-final7/coverage.md`
- Final generator smoke result: passed with no stdout/stderr; coverage reports `auto_full: 50`, `auto_with_structured_override: 0`, and `retained_schema_gap: 0`.
- Final targeted lit command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyf-auto-td.s llvm/test/MC/YSX/tinyv-auto-td.s llvm/test/MC/YSX/tinyv-invalid-disassemble.s llvm/test/CodeGen/YSX/tinyf-isel.ll llvm/test/CodeGen/YSX/tinyf-abi.ll llvm/test/CodeGen/YSX/tinyf-unsupported-f32-to-i64.ll llvm/test/CodeGen/YSX/tinyf-unsupported-f32-to-u64.ll llvm/test/CodeGen/YSX/tinyf-unsupported-i64-to-f32.ll llvm/test/CodeGen/YSX/tinyf-unsupported-unordered-cmp.ll llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll clang/test/CodeGen/YSX/tinyv-builtins.c clang/test/CodeGen/YSX/tinyv-builtins-asm.c`
- Final targeted lit result: passed, `Total Discovered Tests: 12`, `Passed: 12 (100.00%)`.
- Final directory lit command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- Final directory lit result: passed, `Total Discovered Tests: 154`, `Passed: 154 (100.00%)`.
- Final C-to-object proof file: `clang/test/CodeGen/YSX/tinyv-builtins-asm.c`; it checks Clang `-S`, Clang `-emit-obj`, and `llvm-objdump` output for generated `vsetivli`, `vsetvli`, `vle32.v`, selected ALU/reduce/shuffle/custom instructions, and `vse32.v`.
- Final backend ISel proof files: `llvm/test/CodeGen/YSX/tinyf-isel.ll`, `llvm/test/CodeGen/YSX/tinyf-abi.ll`, and `llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll`; they check selected tiny-F scalar instruction selection and selected tiny-V builtin lowering to generated records.
- Final whitespace check command: `git diff --check`
- Final whitespace check result: passed with no output.
- Priority-reset guard command: `python3 -m unittest llvm.lib.Target.YuShuXin.auto-td.tests.test_generator.GeneratorTest.test_generator_writes_real_tinyv_instrinfo_from_opcode_sources -v`
- Priority-reset guard result: passed after generator changes; the tests now check non-empty pseudo/pattern/builtin manifests, `ysx_vfexp_v_f32m1`, and rejection of incomplete or mistyped manifest YAML.
- Priority-reset full auto-td command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Priority-reset full auto-td result: passed, `Ran 39 tests`, `OK`.
- Priority-reset generator smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-final-review --coverage build/ysx-auto-td-final-review/coverage.md`
- Priority-reset generator smoke result: passed; coverage reports `auto_full: 50` and `retained_schema_gap: 0`; generated manifests include `ysx_vfexp_v_f32m1` and `intrinsic=ysx.vfexp`.
- Priority-reset build command: `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- Priority-reset build result: passed; generator reran and YSX TableGen/codegen users rebuilt successfully.
- Priority-reset focused lit command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyf-auto-td.s llvm/test/MC/YSX/tinyv-auto-td.s llvm/test/MC/YSX/tinyv-invalid-disassemble.s llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll clang/test/CodeGen/YSX/tinyv-builtins.c clang/test/CodeGen/YSX/tinyv-builtins-asm.c clang/test/CodeGen/YSX/yushuxin-vfexp.c`
- Priority-reset focused lit result: passed, `Total Discovered Tests: 7`, `Passed: 7 (100.00%)`.
- Priority-reset whitespace command: `git diff --check`
- Priority-reset whitespace result: passed with no output.
- Generated Clang/LLVM consumer smoke command: `python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py --ysx-root llvm/lib/Target/YuShuXin --riscv-opcodes third_party/riscv-opcodes --ysx-opcodes third_party/ysx-opcodes --out-dir build/ysx-auto-td-final-review --coverage build/ysx-auto-td-final-review/coverage.md --clang-builtins-td build/ysx-auto-td-final-review/YSXGenAutoTinyVClangBuiltins.td --clang-builtin-cg-inc build/ysx-auto-td-final-review/YSXGenAutoTinyVBuiltinCG.inc --llvm-intrinsics-td build/ysx-auto-td-final-review/YSXGenAutoTinyVIntrinsics.td`
- Generated Clang/LLVM consumer smoke result: passed; coverage reports `auto_full: 50` and `retained_schema_gap: 0`; generated files contain `def vfexp_v_f32m1`, `Intrinsic::ysx_vfexp`, and `def int_ysx_vfexp`.
- Generated Clang/LLVM build command: `ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf`
- Generated Clang/LLVM build result: passed; CMake reran and Ninja generated `YSX Clang auto-td builtin fragments`, `YSX LLVM intrinsic auto-td fragments`, and `YSX auto TableGen fragments` before rebuilding the affected TableGen and Clang CodeGen users.
- Generated Clang/LLVM build-tree check command: `test -f build/tools/clang/include/clang/Basic/YSXGenAutoTinyVClangBuiltins.td && test -f build/tools/clang/include/clang/Basic/YSXGenAutoTinyVBuiltinCG.inc && test -f build/include/llvm/IR/YSXGenAutoTinyVIntrinsics.td && rg -n "def vfexp_v_f32m1|Intrinsic::ysx_vfexp|def int_ysx_vfexp" build/tools/clang/include/clang/Basic/YSXGenAutoTinyVClangBuiltins.td build/tools/clang/include/clang/Basic/YSXGenAutoTinyVBuiltinCG.inc build/include/llvm/IR/YSXGenAutoTinyVIntrinsics.td`
- Generated Clang/LLVM build-tree check result: passed; the build-tree generated files contain the `yushuxin.vfexp` C API, LLVM intrinsic, and CGBuiltin lowering fragments.
- Generated Clang/LLVM full auto-td unittest command: `python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v`
- Generated Clang/LLVM full auto-td unittest result: passed, `Ran 40 tests`, `OK`.
- Generated Clang/LLVM focused lit command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX/tinyf-auto-td.s llvm/test/MC/YSX/tinyv-auto-td.s llvm/test/MC/YSX/tinyv-invalid-disassemble.s llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll clang/test/CodeGen/YSX/tinyv-builtins.c clang/test/CodeGen/YSX/tinyv-builtins-asm.c clang/test/CodeGen/YSX/yushuxin-vfexp.c`
- Generated Clang/LLVM focused lit result: passed, `Total Discovered Tests: 7`, `Passed: 7 (100.00%)`.

## Retained Schema Gaps

- No retained schema gaps recorded yet.

## Known Remaining Scope

- Bulk tiny-F/tiny-V MC import for the agreed first slice is implemented; generated Clang builtin/LLVM intrinsic/CGBuiltin consumption is implemented for the selected `builtin.codegen: true` proof APIs. Remaining work is broader CodeGen coverage and selector automation beyond the selected scalar tiny-F and explicit tiny-V builtin proof paths.
- Automatic vectorization is intentionally reduced to minimal one-to-one smoke only. This branch proves explicit fixed 128-bit `ysx_vector.h` / `__builtin_ysx_*` use and should not grow a full RVV autovec implementation in the remaining-complete pass.
- Broader backend lowering remains intentionally narrow: the current object proof covers fixed 128-bit `<4 x i32>` and `<4 x float>` values plus selected zero-offset proof vector loads/stores and explicit builtins. vscale frontend plumbing is outside this completion pass.
- Direct C ABI passing/returning of tiny-v vector values remains future work; the current C-to-object proof stores builtin results to memory.
- Full unordered f32 compare lowering and true i64/f32 conversion lowering remain future work; unsupported tests now lock those boundaries down.

## Blog Notes

- The standalone final blog at `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md` describes auto-td-gen, tiny-F/tiny-V, builtin proof, automatic-vectorization boundary, and yushuxin.vfexp.
