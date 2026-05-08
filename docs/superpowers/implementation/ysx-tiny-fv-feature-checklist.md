# YSX Tiny-F/Tiny-V Feature Checklist

## Implemented Features

- Spec approved: docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md

## Generated Surfaces

- No generated surfaces implemented yet.
- Task 3 added auto-td schemas, taxonomy YAML, and proof instruction YAML; no generated build-tree TD exists yet.

## Vendor Evidence

- Task 2 snapshots: `third_party/riscv-opcodes` at upstream `ef103b65c682e7cb705cff67898c515f5c63175c` and `third_party/riscv-isa-manual` at upstream `2d034e16e3edeaa631aeb863adf8ef3a0b743aad`; both were copied from shallow clones and their upstream `.git` directories were removed.
- Task 2 custom opcode: `third_party/ysx-opcodes/extensions/rv_xtinyv`.

## Handwritten Glue

- No handwritten glue implemented yet.

## Validation Evidence

- Baseline configure command: `cmake -G Ninja -S llvm -B build -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD="YSX" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON`
- Baseline configure result: passed; `build/build.ninja` was generated.
- Baseline build command: `ninja -C build clang llc llvm-mc llvm-objdump`
- Baseline build result: failed at final `bin/clang-22` link with `/usr/bin/ld: final link failed: No space left on device`; `llc`, `llvm-mc`, and `llvm-objdump` linked before the failure, but `clang` was not produced.
- Disk snapshot at failure review time: `df -h . build /tmp` reported `/dev/sde` at `100%` used with `137M` available for all three paths; `df -ih . build /tmp` reported inode use at `14%`, so the blocker is block storage, not inode exhaustion.
- Baseline validation command: `python3 build/bin/llvm-lit -sv llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX`
- Baseline validation result: not run because the baseline tool build failed before producing `build/bin/clang`.

## Retained Schema Gaps

- No retained schema gaps recorded yet.

## Blog Notes

- The standalone final blog at `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md` should describe auto-td-gen, tiny-F/tiny-V, builtin proof, automatic vectorization, and yushuxin.vfexp.
