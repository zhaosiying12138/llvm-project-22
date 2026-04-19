# Round 4 Contract

## Mainline Objective

Remove the remaining externally visible non-`rv64ima` MC instruction/CSR surface and start replacing the copied feature/metadata universe with a minimal YSX `rv64ima` surface while keeping YSX-only and RISCV+YSX builds green.

## Target ACs

- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
- AC-3: YSX contains no support code for removed features.

## Blocking Issues

- Default `ysx64` MC still accepts non-IMA assembly: `fence.i`, generic CSR aliases, unprivileged counter aliases, privileged/debug/hypervisor instructions, and symbolic non-IMA CSR names.
- `YSXFeatures.td`, instruction metadata, MC base info, and C++ lowering/frame helpers still expose copied FP/C/V/RV32/vendor/privileged feature and helper surfaces.
- Existing negative tests do not cover the remaining non-IMA MC leaks from the Round-3 review.

## Queued Out Of Scope

- TargetParser/ISAInfo alias cleanup remains queued unless feature pruning exposes a build blocker.
- Large inactive copied CodeGen check-prefix blocks remain queued until backend source/MC surface pruning is correct.
- Cosmetic RISC-V wording cleanup remains queued unless touching the same code for a functional deletion.

## Success Criteria

- `llvm-mc -triple=ysx64-unknown-elf` rejects the Round-3 review examples: `fence.i`, generic/symbolic CSR reads such as `csrr a0, mstatus`, `csrr a0, ssp`, `csrr a0, seed`, counter aliases `rdcycle`/`rdtime`/`rdinstret`, and privileged/hypervisor/debug instructions such as `mret`, `sret`, `wfi`, `dret`, `sfence.vma`, and `hfence.vvma`.
- `llvm/test/MC/YSX/unsupported-features.s` contains negative coverage for the above examples while retaining the FP/vector CSR negative coverage from Round 3.
- The feature and metadata source is materially reduced toward a retained I/M/A-only target, with no `YSXDisabled*`, vector Zve/Zvl, FP feature, compressed feature, or vendor-feature compatibility records left unless a build-required survivor is explicitly documented.
- YSX-only and RISCV+YSX static builds pass after the pruning slice.
- The YSX LLVM/Clang lit subset passes, and `git diff -- llvm/lib/Target/RISCV | wc -l` remains `0`.
