# Round 6 Review Result

Mainline Progress Verdict: ADVANCED

Round 6 fixed the default Clang driver frontend leak that Round 5 exposed: default `ysx64` no longer emits the RISCV disabled-extension universe, no longer defines the RVV macros, and no longer exposes RVV type/builtin names through the tested default path. The work is still not complete. The Round 6 contract explicitly queued the remaining AC-3 backend source pruning out of scope, and review found an additional inherited Clang target-surface leak through `target` attributes and explicit cc1 target features.

I updated the mutable section of `goal-tracker.md` to Plan Version 14. The default-driver/RVV probe fix is now recorded as verified by review, while the broader frontend-surface completion claim is rejected and a new blocking issue tracks the inherited target-attribute/target-feature leak.

## Goal Alignment Summary

```text
ACs: 4/4 addressed, 2/4 still incomplete | Forgotten items: 1 | Unjustified deferrals: 1
```

- AC-1: Preserved. `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`, and the default YSX smoke compile still produces an ELF64 RISC-V relocatable object.
- AC-2: Advanced but not complete. Default driver `-###`, predefines, and RVV type/builtin probes are fixed, but inherited Clang target attributes and explicit cc1 target features can still expose unsupported RISCV FP/vector state.
- AC-3: Advanced only for the default Clang/TargetParser path. The copied backend still retains vector/FP/vendor/RV32/compressed lowering, selection, metadata, pass declarations, false-return compatibility APIs, and direct `YSXVType` forwarding to `RISCVVType`.
- AC-4: Advanced with default-driver and RVV negative tests, but missing tests for target attributes, explicit target features, and unsupported FP/vector asm constraints.

## Mainline Gaps

### 1. AC-2 frontend surface still leaks unsupported RISCV features through target attributes and explicit cc1 features

Severity: Mainline Gap, blocks AC-2 and AC-4.

`YSX64TargetInfo` is a subclass of `RISCV64TargetInfo` and only overrides a small set of hooks in `clang/lib/Basic/Targets/RISCV.h:235-276`. It does not override `parseTargetAttr`, `initFeatureMap`, `handleTargetFeatures`, or `isValidFeatureName`, so those paths still use the inherited RISCV implementation. The inherited implementations call `RISCVISAInfo` directly in `clang/lib/Basic/Targets/RISCV.cpp:348-376`, `:420-447`, and `:507-579`.

Manual checks:

```sh
printf 'void f(void) __attribute__((target("arch=+v"))); void f(void){}\n' |
  clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o -
```

This succeeds and emits function attributes containing `+v`, `+f`, `+d`, `+zicsr`, `+zve*`, and `+zvl*`. Compiling the same input to an object then reaches a backend fatal error: `YSX only supports the rv64ima ISA`.

```sh
clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang +v \
  -dM -E -x c /dev/null
```

This defines `__riscv_v`, `__riscv_vector`, and vector length macros for YSX. That contradicts the Round 6 claim that YSX exposes no RISCV vector frontend surface.

Directive implementation plan:

1. Add YSX-specific overrides in `YSX64TargetInfo` for `initFeatureMap`, `handleTargetFeatures`, `parseTargetAttr`, and `isValidFeatureName`.
2. In those overrides, accept only the default feature set and exact `rv64ima`/`lp64` surface: `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, optional `+/-relax`, and reserved GPR features if they are intentionally supported by the driver.
3. Reject `target("arch=+...")` unless every requested extension is already in the YSX rv64ima set. Reject full-arch target attributes unless the full arch string is exactly `rv64ima`.
4. Diagnose unsupported target attributes before IR emission; do not let them reach YSXSubtarget as backend fatals.
5. Add Clang negative tests for `target("arch=+v")`, `target("arch=rv64imaf")`, and `-Xclang -target-feature +v`. The tests must check for frontend diagnostics and must check that no unsupported target feature reaches emitted LLVM IR.
6. Audit and override unsupported inline asm constraint handling inherited from RISCV. At minimum, add negative tests for FP/vector constraints such as `"f"` and `"vr"` so YSX rejects them as unsupported frontend constraints instead of accepting them until register allocation fails.

### 2. AC-3 backend source pruning is still incomplete and was explicitly deferred

Severity: Mainline Gap, blocks AC-3.

The Round 6 contract queued backend deletion out of scope, and Claude's summary repeats that vector/FP/vendor/RV32/compressed lowering, selection, metadata, pass declarations, frame helpers, and false-return compatibility APIs remain. This is not a valid final state under `docs/plan.md`, which requires the copied backend to remove support code for removed features.

Representative current evidence:

- `llvm/include/llvm/TargetParser/YSXISAInfo.h:206-240` still forwards `YSXVType` directly to `RISCVVType` vector helpers.
- `llvm/lib/Target/YuShuXin/YSX.h:53-60` and `:80-90` still declare compressible, gather/scatter, vector peephole, VSETVLI, and VXRM passes.
- `llvm/lib/Target/YuShuXin/YSXSubtarget.h:163-217` still keeps false-return APIs for compressed, FP, vector, bitmanip, and other removed extensions.
- `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:208-285` still reserves TSFlags for vector constraints, VL, vector policy, VXRM, VTYPE, and vector overlap metadata.
- `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:463-528` still contains scalable-vector callee-save splitting and an unreachable `#if 0` YSXVec stack probing body.
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, and `YSXInstrInfo.cpp` still contain extensive `YSXVType`, vector lowering/selection, and FP feature checks.

Directive implementation plan:

1. Remove the remaining vector compatibility layer from `YSXISAInfo.h`. Delete `YSXVType` forwarding and then delete or rewrite all YSX users rather than adding new compatibility stubs.
2. Prune vector/compressed/VXRM/VSETVLI pass declarations, registrations, and pipeline hooks from `YSX.h`, `YSXTargetMachine`, and pass initialization code.
3. Remove vector TSFlags, VTYPE/VL operands, VXRM metadata, vector overlap metadata, and generated helpers from TableGen and `MCTargetDesc/YSXBaseInfo.h`.
4. Delete unsupported lowering and selection code from `YSXISelLowering.*` and `YSXISelDAGToDAG.cpp`, including fixed/scalable vector lowering, RVV intrinsic handling, FP legalization paths, vendor-vector helpers, and RV32-only cases.
5. Reduce frame lowering and register info to scalar RV64 GPR stack handling only. Delete scalable-vector frame paths, vector callee-save helpers, and unreachable `#if 0` bodies.
6. Remove false-return feature compatibility APIs from `YSXSubtarget.h` after their callers are gone. Remaining APIs should correspond to real rv64ima features only.
7. Rebuild YSX-only and RISCV+YSX static builds, run the focused YSX LLVM/Clang suites, and add negative tests that fail if removed vector/FP metadata or pass names become reachable again.

## Blocking Side Issues

- The inherited target-attribute path turns unsupported YSX frontend input into unsupported backend feature attributes and can end in a backend fatal instead of a normal Clang diagnostic. This blocks safe AC-2 closure.
- The retained backend compatibility stubs and unreachable vector bodies block AC-3 because the plan requires unsupported implementation code to be deleted, not hidden behind false predicates.

## Queued Side Issues

- Stale copied YSX CodeGen check-prefix blocks remain queued until source pruning is complete.
- Residual copied "RISC-V" comments/statistic names and broad inherited file names in Clang can be cleaned after the structural AC-2/AC-3 issues are resolved.
- The immutable tracker still omits AC-3 and AC-4 from `docs/plan.md`; continue auditing against `docs/plan.md` without modifying the immutable section.

## Verified Round-6 Claims

Verified with the existing YSX-only build:

```sh
clang --target=ysx64-unknown-elf -### -c -x c /dev/null
```

The cc1 line contains only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax` YSX target features, plus `-target-abi lp64`.

```sh
clang --target=ysx64-unknown-elf -dM -E -x c /dev/null
```

This no longer defines `__riscv_v_intrinsic` or `__riscv_vector`, and it still defines the expected rv64 I/M/A/Zmmul/Zaamo/Zalrsc macros.

Default and explicit `-march=rv64ima` Clang compiles produce ELF64 RISC-V soft-float relocatable objects. `-march=rv64imav` rejects with `YuShuXin only supports -march=rv64ima`. RVV type and builtin probes fail as expected. I did not rerun full lit or Ninja builds in this review sandbox.

REQUIRES MORE WORK
