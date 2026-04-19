# Round 8 Contract

## Mainline Objective

Delete the first build-verified slice of retained non-IMA YSX backend source
surfaces so AC-3 advances from frontend hiding toward actual `rv64ima` backend
code removal.

## Target ACs

- AC-3: YSX implementation is materially smaller than RISCV and contains no
  support code for removed features.
- AC-2: The remaining backend-visible ISA surface continues to accept only
  `rv64ima`/`lp64`.

## Blocking Issues

- `YSXISAInfo` still parses broad RISC-V arch strings and still forwards
  `YSXVType` to RISCV vector helpers.
- YSX backend headers and pipeline declarations still expose removed
  compressed, vector, VSETVLI, VXRM, and gather/scatter pass surfaces.
- TableGen/source helpers still carry vector/compressed metadata and frame or
  subtarget compatibility hooks that keep removed implementation paths alive.

## Queued Out Of Scope

- Clang `target("cpu=...")` and `target("tune=...")` warning-vs-error policy
  is queued because Round 7 review classified it as non-blocking and unrelated
  to AC-3 backend source deletion.
- Stale copied YSX CodeGen check-prefix cleanup is queued until backend source
  pruning removes the corresponding implementation surfaces.
- If the full lowering/selection deletion set is too large for one safe commit,
  remaining `YSXISelLowering`, `YSXISelDAGToDAG`, and `YSXInstrInfo` pruning
  must stay active for the next AC-3 round rather than being declared complete.

## Success Criteria

- `YSXISAInfo::parseArchString` accepts exactly `rv64ima` and rejects RV32,
  `g`, FP, compressed, vector, and arbitrary `z`/`s`/`x` extension groups.
- The direct `YSXVType` forwarding layer is removed, and build failures are
  handled by deleting unsupported users in the selected slice rather than
  adding a replacement compatibility shim.
- Removed compressed/vector/VSETVLI/VXRM/gather-scatter pass declarations or
  pipeline hooks disappear from the selected backend files.
- The YSX-only build and combined RISCV+YSX static build still link.
- Focused YSX LLVM/Clang tests pass, and `llvm/lib/Target/RISCV` remains
  unchanged.
