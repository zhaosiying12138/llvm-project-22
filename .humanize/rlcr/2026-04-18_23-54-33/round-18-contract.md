# Round 18 Contract

## Mainline Objective

Finish the next AC-3 source-pruning slice by collapsing YSX to a single
`ysx64`/`rv64ima`/`lp64` backend surface and deleting the remaining RV32,
unsupported ABI, always-false removed-extension, compressed/Zc, and vendor MC
compatibility plumbing that still exists as source metadata or dead branches.

## Target ACs

- AC-2: YSX only exposes and accepts the retained `rv64ima` ISA and `lp64` ABI
  surface.
- AC-3: YSX implementation contains no support code for removed RV32,
  unsupported ABI, compressed/Zc, vendor, FP, vector, or other non-IMA
  features.

## Blocking Issues

- `YSXFeatures.td`, `YSXRegisterInfo.td`, `YSXInstrInfo.td`, and
  `YSXInstrInfoA.td` still model RV32 hardware modes or RV32 predicates.
- `MCTargetDesc/YSXBaseInfo.*`, lowering, frame lowering, ELF streamer, and
  register-info paths still carry ILP32 and hard-float ABI variants or fallback
  code even though YSX only supports `lp64`.
- `YSXSubtarget.h`, `YSXISelLowering.cpp`, and `YSXISelDAGToDAG.cpp` still
  contain always-false removed-extension query wrappers and callsites.
- MC fixup, backend, object-writer, expression, code-emitter, target-streamer,
  and compression helper paths still contain compressed/Zc or vendor plumbing.

## Queued Out Of Scope

- Stale inactive YSX test check-prefix trimming is queued until this AC-3
  source-pruning slice compiles and validates.
- Immutable goal-tracker AC drift remains documented; the immutable section is
  not edited.
- CPU/tune target-attribute diagnostics remain queued unless this round exposes
  an actual feature leak.

## Success Criteria

- `Feature32Bit`, `IsRV32`, `RV32`, and RV32 hardware-mode entries are removed
  or collapsed so generated YSX code is unconditionally 64-bit.
- ABI handling accepts only empty/default `lp64`; ILP32, LP64F/D/E, and related
  fallback or eflags paths are deleted.
- Removed-extension `hasStdExt*` and `hasVendor*` wrappers not needed for
  retained I/M/A/Zmmul/Zaamo/Zalrsc/relax behavior are deleted, and callers are
  simplified to scalar rv64ima logic without adding new false wrappers.
- RVC/QC/Andes/Zc fixups, vendor relocation emission, QC MC expression support,
  RVC compression stubs, RVC target-streamer state, and unsupported
  code-emitter/object-writer mappings are gone, while normal rv64ima, call,
  TLS, branch, and RISC-V ELF-compatible relocations still work.
- `git diff --check` is clean.
- `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.
- YSX-only build passes for `LLVMYSXCodeGen llvm-mc clang llc`.
- Combined RISCV+YSX build passes for
  `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld`.
- Focused YSX lit suites pass:
  `llvm/test/MC/YSX`, `llvm/test/CodeGen/YSX`,
  `clang/test/Driver/YSX`, and `clang/test/CodeGen/YSX`.
- Smoke compiles still produce ELF64 RISC-V soft-float objects.
- Negative probes reject `+v`, FP, RV32/`+32bit`, compressed/Zc, vendor
  extension spelling, scalable-vector IR, and direct `llvm.riscv.vsetvli`.
