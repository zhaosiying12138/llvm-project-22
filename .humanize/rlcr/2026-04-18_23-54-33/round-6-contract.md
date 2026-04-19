# Round 6 Contract

## Mainline Objective

Make the Clang and TargetParser surface for `ysx64` YSX-specific so it exposes only the supported `rv64ima`/`lp64` frontend feature set instead of inherited RISCV vector and disabled-extension state.

## Target Acceptance Criteria

- AC-2: `ysx64` rejects removed frontend ISA surfaces and exposes only the supported rv64ima/lp64 target feature and predefine surface.
- AC-3: YSX no longer preserves the copied RISCV TargetParser/Clang target-surface implementation as the frontend entrypoint for `ysx64`.

## Blocking Issues

- `clang/lib/Basic/Targets.cpp` still maps `ysx64` to `RISCV64TargetInfo`, which exposes RISCV vector target behavior and `__riscv_v_intrinsic`.
- The Clang driver still routes `ysx64` through `RISCVISAInfo::toFeatures(..., AddAllExtensions=true)`, so `-###` emits the copied disabled RISCV extension universe and relies on YSX backend filtering.
- `llvm/include/llvm/TargetParser/YSXISAInfo.h` still aliases RISCV target-parser types instead of providing a small YSX parser surface.

## Queued Out Of Scope

- Deleting the remaining unsupported backend implementation in `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, `YSXSubtarget.h`, `YSX.h`, and frame helpers.
- Trimming stale copied YSX CodeGen check-prefix blocks.
- Cleaning residual copied "RISC-V" comments and statistics names.
- Changing RISC-V-compatible ELF/object semantics for YSX v1.

## Success Criteria

- `clang --target=ysx64-unknown-elf -### -c` emits only the YSX positive feature set for rv64ima plus `+relax` and does not emit disabled RISCV extension/vendor/vector features such as `-v`, `-zve64x`, `-zvl128b`, or `-xventanacondops`.
- `clang --target=ysx64-unknown-elf -dM -E -x c /dev/null` does not define `__riscv_v_intrinsic` and still defines the expected rv64/lp64/IMA-compatible macros.
- Default and explicit `-march=rv64ima` Clang compiles still produce ELF64 RISC-V relocatable objects.
- Enabled unsupported `rv64imaf`, `rv64imac`, and `rv64imav` still reject through YSX-owned tests.
- Focused YSX LLVM/Clang lit tests pass, the combined RISCV+YSX static build still links, and `llvm/lib/Target/RISCV` remains unchanged.
