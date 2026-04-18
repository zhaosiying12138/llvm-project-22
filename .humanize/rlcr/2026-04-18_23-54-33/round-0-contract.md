# Round 0 Contract

## Mainline Objective

Establish a standalone `YSX` backend skeleton from the RISCV backend, reduce it
to a self-contained `rv64ima` target surface, and wire up the new `ysx64`
LLVM/Clang target identity so the backend can be built and tested without any
dependency on the RISCV backend.

## Target ACs

- AC-1: standalone YSX backend identity and non-conflicting init/options
- AC-2: rv64ima-only target surface with unsupported ISA features removed or
  rejected

## Blocking Side Issues In Scope

- Any remaining compile-time dependency from YSX to RISCV headers, target
  libraries, generated includes, or hidden `cl::opt` registrations
- Triple/driver plumbing gaps that prevent `ysx64` from reaching the backend
- Build-system or tablegen breakages caused by deleting GISel and removed
  feature files

## Queued Side Issues Out Of Scope

- Additional size-driven refactors once the backend is functionally standalone
- Broader cleanup of copied comments or stylistic simplifications that do not
  affect build/test correctness
- Expansion beyond the rv64ima-applicable LLVM and Clang test subsets

## Round Success Criteria

- `llvm/lib/Target/YuShuXin` exists with YSX-named target entrypoints and no
  hard dependency on `llvm/lib/Target/RISCV`
- `ysx64` is recognized by LLVM/Clang target plumbing
- The YSX backend rejects removed ISA surfaces and keeps only rv64ima support
- YSX-owned LLVM and Clang test directories exist for the retained surface
- Fresh build directories can at least configure and start building the
  YSX-only plus RISCV+YSX variants, with remaining failures narrowed to concrete
  backend issues rather than missing target registration/plumbing
