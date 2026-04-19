# Round 2 Contract

## Mainline Objective

Finish the YSX `rv64ima` pruning by removing the remaining unsupported ISA source surfaces and fixing assembler option validation so removed extensions cannot be enabled through MC.

## Target ACs

- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
- AC-3: YSX implementation contains no support code for removed FP, compressed, vector, RV32, bitmanip, crypto, vendor, or unrelated privileged/profile features.

## Blocking Issues

- Incremental assembler feature toggles still accept `.option arch, +f`, `+c`, `+zbb`, `+v`, and `.option rvc`; these must be rejected and must not leak feature bits after failure.
- YSX TableGen/C++ still carries unsupported feature definitions, register classes, generated-reference compatibility opcodes, MCA RVV behavior, and stale selection/lowering helpers inherited from RISCV.

## Queued Out Of Scope

- The immutable tracker AC omission remains queued because the mutable tracker and round reviews already track AC-3/AC-4 against `docs/plan.md`.
- TargetParser namespace alias cleanup is queued unless it remains externally observable after backend pruning and assembler validation are fixed.

## Success Criteria

- `YSXFeatures.td`, `YSXRegisterInfo.td`, instruction-format includes, MCA sources, `YSX.h`, MC helpers, and selection/lowering code no longer expose the reviewed removed-feature surfaces or `YSXUnsupportedOpcodes.h`.
- YSX rejects `.option arch, +f`, `+c`, `+zbb`, `+v`, `.option rvc`, and preserves old feature bits after failed push/pop toggles.
- `rg` blocker scans over `llvm/lib/Target/YuShuXin` find no retained `FeatureStdExtF`, `FeatureStdExtC`, `FeatureStdExtV`, `FeatureVendor`, `RVV`, `XSf`, `XTHead`, `Xqci`, `LD_RV32`, or `YSXUnsupportedOpcodes` matches outside intentional diagnostics or tests.
- YSX-only and RISCV+YSX static builds pass with `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld`.
- YSX LLVM/Clang lit subset passes, and `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.
