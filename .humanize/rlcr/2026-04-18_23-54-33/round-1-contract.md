# Round 1 Contract

## Mainline Objective

Prune `llvm/lib/Target/YuShuXin` so the backend source itself only retains the
`rv64ima` surface plus the required helper extensions used by that retained
surface, instead of merely rejecting unsupported ISA strings at the frontend.

## Target ACs

- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
- AC-3: YSX is materially smaller than RISCV and contains no support code for
  removed features.

## Blocking Side Issues In Scope

- `YSXInstrInfo.td`, `YSXFeatures.td`, `YSXSchedule.td`, `YSXProcessors.td`,
  `YSXSubtarget.*`, and `YSXTargetMachine.cpp` still expose FP, compressed,
  vector, bitmanip, RV32, packed-SIMD, and vendor-specific support.
- Deleted feature files may still be referenced by TableGen or C++ code after
  pruning, so YSX-only and RISCV+YSX rebuilds must flush out stale references.
- The mutable tracker must stay aligned to `docs/plan.md` even though the
  immutable tracker AC list is incomplete.

## Queued Side Issues Out Of Scope

- Additional frontend/parser minimization beyond the current backend-pruning
  round, as long as `ysx64` remains functionally correct and non-conflicting.
- Cosmetic cleanup or comment rewriting that does not reduce retained feature
  surface or unblock validation.

## Round Success Criteria

- YSX TableGen, scheduler, subtarget, and target-machine sources no longer
  carry removed FP/C/V/RV32/bitmanip/vendor support code.
- Any files only serving removed surfaces are deleted, and stale references are
  fixed.
- YSX-only and RISCV+YSX builds still succeed after pruning.
- YSX smoke tests and YSX LLVM/Clang test directories pass again, with added
  negative coverage for removed surfaces such as `+f`, `+c`, `+zbb`, and
  `.option arch, rv64gc`.
