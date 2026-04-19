# Round 3 Contract

## Mainline Objective

Remove the remaining externally visible unsupported MC surface and the largest retained unsupported YSX source surfaces so the backend moves from front-door rejection toward a real `rv64ima`-only implementation.

## Target ACs

- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
- AC-3: YSX implementation contains no support code for removed FP, compressed, vector, RV32, bitmanip, crypto, vendor, privileged/profile, or GISel-only features.

## Blocking Issues

- Default `ysx64` MC accepts removed FP/vector CSR names (`fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vxsat`, `vxrm`, `vlenb`) without any unsupported `-mattr` or `.option` input.
- YSX TableGen and C++ still carry unsupported source surfaces under renamed disabled feature scaffolding, including broad feature definitions, FP/vector register and calling-convention tables, compressed/vector instruction-format includes, GISel-only TD constructs, and vector/FP lowering/frame helpers.

## Queued Out Of Scope

- TargetParser alias cleanup remains queued unless it becomes a build/API blocker while pruning YSX backend source.
- Large inactive copied YSX CodeGen check-prefix blocks remain queued unless active RUN lines invoke unsupported ISA variants.

## Success Criteria

- `llvm-mc -triple=ysx64-unknown-elf` rejects `csrr`/CSR aliases for `fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vxsat`, `vxrm`, and `vlenb`, with YSX-owned negative tests covering those exact names.
- `YSXSystemOperands.td`, `YSXCallingConv.td`, `YSXRegisterInfo.td`, and `YSXInstrInfo.td` no longer expose FP/vector CSR, register, ABI, or compressed/vector/GISel-only instruction-format surfaces needed only by removed features.
- The broader Round-3 scans for `YSXDisabledStdExt`, `YSXDisabledVendorFeature`, `FeatureStdExtZve`, `FeatureStdExtZvl`, `FPR`, `VRRegClass`, `YSXVec`, `GICustom`, `GISel`, `YSXInstrFormatsC`, `YSXInstrFormatsV`, `vlenb`, `fflags`, `fcsr`, and `vxrm` either have no backend source matches or each remaining match is explicitly justified as required for retained `rv64ima`.
- YSX-only and RISCV+YSX static builds pass, the YSX LLVM/Clang lit subset passes, and `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.
