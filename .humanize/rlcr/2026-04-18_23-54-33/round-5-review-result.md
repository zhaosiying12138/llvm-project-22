# Round 5 Review Result

Mainline Progress Verdict: ADVANCED

Round 5 fixed the narrow compile-path blocker from Round 4: default `ysx64-unknown-elf` and explicit `-march=rv64ima` Clang compiles now produce ELF64 RISC-V relocatable objects, while enabled unsupported `rv64imaf`, `rv64imac`, backend `+f`, and backend `+v` still reject. The work is not complete. Claude explicitly queued AC-3 source pruning out of scope for this round, and the current Clang target path still reuses the RISCV target surface enough to expose vector/RISCV artifacts for `ysx64`.

I updated the mutable section of `goal-tracker.md` to Plan Version 12: the Round-5 compile-path fix is marked verified by review, and the remaining YSX Clang/RISCV target-surface leak is recorded as a blocking AC-2/AC-3 issue. I did not modify the immutable section.

## Goal Alignment Summary

```text
ACs: 3/4 addressed this round (AC-3 not advanced) | Forgotten items: 0 new | Unjustified deferrals: 1
```

- AC-1: Still met. `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`, and no Round-5 change touched RISCV sources.
- AC-2: Advanced but not complete. The supported Clang compile path now works and enabled `f`/`c`/`v` still reject, but Clang still exposes unsupported RISCV/vector target surface for `ysx64`.
- AC-3: Not advanced in Round 5. The round contract explicitly queued source pruning out of scope even though the original plan requires deleting unsupported FP/vector/RV32/compressed/vendor support code.
- AC-4: Advanced. `clang/test/Driver/YSX/target-options.c` now contains real compile RUN lines for default `ysx64-unknown-elf` and explicit `rv64ima`.

## Mainline Gaps

### 1. AC-3 source pruning is still incomplete and was explicitly deferred

Severity: Mainline Gap, blocks AC-3.

Round 5 did not attempt the remaining source-surface deletion. This is not an acceptable deferral because `docs/plan.md` requires the copied backend to remove GISel, RV32, RVV, FP, compressed, non-IMA feature/scheduling/profile code, DAG lowering, DAG-to-DAG selection, custom ISD nodes, pseudos, and scheduling data for removed features.

Representative current evidence:

- `llvm/lib/Target/YuShuXin/YSX.h:53-60` and `:80-90` still declare compressible, vector peephole, VSETVLI, and VXRM passes.
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h:163-216` still exposes false-return compatibility APIs for C/Zca, FP/Zfinx/Zdinx, Zb*, vector, and other removed features.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:64-110` and `:208-282` still define vector constraints, VL/SEW/vector-policy TSFlags, VXRM metadata, and VTYPE metadata.
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp:482-545`, `:1890-1942`, and `:12927-12977` still carry FP legalization and RISCV vector intrinsic/load-store lowering.
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:463-524`, `:2079-2125`, and `:4068-4255` still carry VSETVLI/vendor-vector selection helpers, vector intrinsic cases, vector splat selectors, and scalar-FP-as-int selection.
- `llvm/lib/Target/YuShuXin/YSXInstrInfo.cpp:46-58`, `:179-320`, `:1914-2135`, and `:4449-4474` still retain vector spill/copy stats, vector copy logic behind `#if 0`, vector reassociation logic, and false-return vector helper APIs.
- `llvm/include/llvm/TargetParser/YSXISAInfo.h:12-18` still aliases `RISCVISAInfo` and `RISCVVType`.

Directive implementation plan:

1. Replace `YSXISAInfo.h` with real YSX target-parser helpers that accept only `rv64ima` for `ysx64`, compute only `lp64`, and produce only `+64bit,+i,+m,+a,+zmmul,+zaamo,+zalrsc,+relax` feature output. Stop aliasing `RISCVISAInfo` and `RISCVVType`.
2. Add a YSX-specific Clang target info instead of returning `RISCV64TargetInfo` for `ysx64`. Keep the RISC-V-compatible data layout and ABI where needed, but remove `HasRISCVVTypes`, RISCV vector builtins, vector calling conventions, and unconditional vector predefines from the YSX target.
3. Split or specialize the Clang driver feature path so `ysx64` no longer calls `RISCVISAInfo::toFeatures(AddAllExtension=true)`. The `-###` output for default `ysx64` must contain only the positive rv64ima YSX feature set plus `+relax`; it must not emit the copied negative RISCV extension universe.
4. Delete the unsupported backend source surface in one pruning pass: remove vector/compressed/VXRM/VSETVLI pass declarations and registrations, vector TSFlags and instruction-format metadata, vector/FP/vendor/RV32 lowering and DAG selection helpers, vector frame and spill/copy helpers, and false-return feature compatibility APIs. Let compile errors identify remaining generated references, then delete those callers or the TD records that generate them rather than adding new stubs.
5. Rebuild both YSX-only and RISCV+YSX static builds, then run the focused YSX LLVM/Clang lit suites. Add negative Clang tests that verify `ysx64` does not define `__riscv_v_intrinsic`, does not enable RISCV vector builtins/types, and does not expose copied disabled RISCV features in `-###`.

### 2. Clang still exposes RISCV/vector target surface for `ysx64`

Severity: Mainline Gap, blocks AC-2 and AC-3.

The Round-5 backend filtering makes compilation succeed, but it is a workaround around inherited RISCV Clang plumbing. `clang/lib/Basic/Targets.cpp:480-512` still maps `ysx64` to `RISCV64TargetInfo`. That constructor sets `HasRISCVVTypes = true` at `clang/lib/Basic/Targets/RISCV.h:37-49`, and `RISCVTargetInfo::getTargetDefines` unconditionally defines `__riscv_v_intrinsic` at `clang/lib/Basic/Targets/RISCV.cpp:225-226`.

Manual check:

```sh
clang --target=ysx64-unknown-elf -dM -E -x c /dev/null | rg '__riscv_v_intrinsic'
# #define __riscv_v_intrinsic 1000000
```

Also, `clang --target=ysx64-unknown-elf -### -c -x c /dev/null` still emits the full copied RISCV disabled feature universe, including `-f`, `-v`, `-zbb`, `-zicsr`, `-zifencei`, many `-zve*`/`-zvl*` features, and many vendor `-x*` features. This comes from `clang/lib/Driver/ToolChains/Arch/RISCV.cpp:26-43`, where `RISCVISAInfo::toFeatures(/*AddAllExtension=*/true)` is still used for `ysx64`.

This violates the plan's standalone/minimal YSX surface even though the backend now filters the inherited disabled features before parsing.

## Blocking Side Issues

- The remaining YSX Clang target reuse of `RISCV64TargetInfo` blocks final AC-2/AC-3 closure because it exposes unsupported vector/RISCV frontend surface even when codegen rejects enabled unsupported backend features.
- The retained false-return helpers and `#if 0` blocks in core backend files block AC-3 because the plan requires unsupported implementation code to be removed, not merely hidden behind always-false predicates.

## Queued Side Issues

- Stale copied YSX CodeGen check-prefix blocks remain queued until source pruning is complete.
- Residual copied "RISC-V" comments and statistics names in YSX files should be cleaned after structural pruning.
- The immutable tracker still omits AC-3 and AC-4 from `docs/plan.md`; keep auditing against `docs/plan.md` without editing the immutable section.

## Verified Round-5 Claims

Commands run:

```sh
/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang --target=ysx64-unknown-elf -c -x c /dev/null -o /tmp/codex-ysx-default.o
/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang --target=ysx64-unknown-elf -march=rv64ima -c -x c /dev/null -o /tmp/codex-ysx-rv64ima.o
/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm/bin/clang --target=ysx64-unknown-elf -c -x c /dev/null -o /tmp/codex-combined-ysx-default.o
/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm/bin/clang --target=ysx64-unknown-elf -march=rv64ima -c -x c /dev/null -o /tmp/codex-combined-ysx-rv64ima.o
```

All four produced ELF64 RISC-V relocatable objects. `rv64imaf`, `rv64imac`, `llvm-mc -mattr=+f`, and `llc -mattr=+v` reject with the expected YSX rv64ima-only error. `llvm-mc -mattr=-f,-v,-zbb` assembles valid `add`, and `llvm-mc -mattr=help` shows the curated YSX feature list.

I could not rerun lit in this sandbox: `llvm-lit` attempted to create a temp file under `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/tools/clang/test`, which is read-only under the current sandbox policy.

REQUIRES MORE WORK
