# Round 3 Review Result

Mainline Progress Verdict: ADVANCED

Round 3 advanced the mainline by fixing the exact FP/vector CSR alias leak from Round 2, adding negative tests for those names, deleting the copied C/V format includes, and pruning the TableGen register/calling-convention surface. It is still not complete. Claude's own "Remaining Items" are not optional follow-up; they are the core AC-3 pruning requirement from `docs/plan.md`, and review found an additional externally visible AC-2 failure through default MC acceptance of non-`rv64ima` CSR/privileged/Zifencei surfaces.

## Goal Alignment Summary

ACs: 4/4 addressed, 2/4 met | Forgotten items: 2 | Unjustified deferrals: 1

- AC-1: Maintained. I verified `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`. I could not rerun Ninja in the external build directory because the sandbox cannot write there.
- AC-2: Advanced but not met. The exact `fflags`/`frm`/`fcsr`/vector CSR aliases now reject, but default `ysx64` MC still accepts non-IMA CSR, privileged, and Zifencei assembly.
- AC-3: Advanced but not met. `YSXFeatures.td`, `YSXISelLowering.*`, `YSXISelDAGToDAG.*`, `YSXFrameLowering.*`, `YSXInstrInfo.*`, subtarget, MC base info, and parser/disassembler helpers still carry large copied FP/C/V/RV32/bitmanip/crypto/vendor/privileged surfaces.
- AC-4: Maintained but incomplete for the current gaps. Round 3 added the requested FP/vector CSR tests, but there is no negative coverage for the remaining default MC leaks such as `fence.i`, `mret`, `sfence.vma`, or `csrr mstatus`.

Tracker update: I updated the mutable section of `goal-tracker.md` to Plan Version 9, added a Round-3 review log entry, kept task3/task6 active, and replaced the now-fixed FP/vector CSR blocker with the broader non-IMA CSR/privileged/Zifencei MC blocker.

## Mainline Gaps

1. AC-2 is still observably wrong: default YSX MC accepts non-`rv64ima` CSR, privileged, and Zifencei surfaces.

   These commands were run against `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/llvm-mc`:

   ```sh
   printf 'fence.i\n' | llvm-mc -triple=ysx64-unknown-elf -
   # exits 0 and prints: fence.i

   printf 'csrr a0, ssp\ncsrr a1, seed\ncsrr a2, mstatus\n' | llvm-mc -triple=ysx64-unknown-elf -
   # exits 0 and prints all three symbolic CSR reads

   printf 'sfence.vma\nhfence.vvma\nmret\nsret\n' | llvm-mc -triple=ysx64-unknown-elf -
   # exits 0 and prints all four privileged/hypervisor mnemonics

   printf 'rdcycle a0\nrdtime a1\nrdinstret a2\n' | llvm-mc -triple=ysx64-unknown-elf -
   # exits 0 and prints all three CSR aliases
   ```

   Source causes:

   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:796` defines `FENCE_I` with no `HasStdExtZifencei` predicate.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:829` and nearby define CSR instructions; `:1068-1078` expose `rdcycle`/`rdtime`/`rdinstret` and `csrr` aliases.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:884`, `:900`, `:923`, and `:958` keep unpredicated privileged/debug instructions such as `mret`, `wfi`, `sfence.vma`, and `dret`; `hfence.vvma` is still accepted despite being behind `HasStdExtH`, because the current feature/default path still leaves that surface reachable.
   - `llvm/lib/Target/YuShuXin/YSXSystemOperands.td:73-85`, `:91-105`, `:113-147`, `:183-252`, `:276-324`, and `:423-492` still define unprivileged, supervisor, hypervisor, machine, debug, AIA, and control-transfer CSR names.

   A backend promised as `rv64ima` should not accept Zicsr/Zifencei/privileged/profile surfaces by default. If YSX v1 intentionally wants a CSR subset, that would be a plan change; under the current plan it is a blocker.

2. AC-3 is still not implemented: the feature universe is still the copied RISC-V universe, not a minimal YSX `rv64ima` surface.

   Representative source evidence:

   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:77-87` keeps E/Zibi/Zic* features beyond I.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:126-189` keeps Zicsr, Zicntr, Zifencei, Zihint*, Zimop, Zicfilp/Zicfiss, and Zilsd surfaces.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:242-285` keeps Ztso and non-IMA atomic extensions such as Zabha, Zacas, Zalasr, and Zawrs.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:289-382` keeps FP/Zfinx/Zhinx surfaces under `YSXDisabledStdExtF`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:385-475` keeps compressed/Zc surfaces under `YSXDisabledStdExtC`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:640-725` keeps vector Zvl/Zve/Zv surfaces under `YSXDisabledStdExtV`.
   - `llvm/lib/Target/YuShuXin/YSXFeatures.td:1128+` still carries a long vendor-extension block under `YSXDisabledVendorFeature...`.

   This also leaks through Clang: `clang -### --target=ysx64-unknown-elf -c -x c /dev/null` emits hundreds of `-target-feature -zbb`, `-zve32x`, `-xsf...`, `-xthead...`, `-experimental-zv...`, etc. A feature surface that is merely disabled is still a retained support surface, not the "materially smaller" YSX backend required by AC-3.

3. AC-3 is still not implemented in the C++ lowering/selection/frame code; unsupported code is parked behind `false` helpers or `#if 0` instead of deleted.

   Representative evidence:

   - `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp` is still 25,584 lines and retains extensive vector/FP/custom intrinsic paths, including `YSXVec` lowering, `Intrinsic::riscv_v*`, crypto/bitmanip intrinsic lowering, FP rounding mode machinery, and `fallBackToDAGISel` GISel commentary.
   - `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:476-545`, `:1855-1954`, and `:2079-2636` still handle RVV and vendor vector intrinsics such as `riscv_vsetvli`, `riscv_vlseg*`, `riscv_vsseg*`, and `riscv_sf_*`.
   - `llvm/lib/Target/YuShuXin/YSXFrameLowering.cpp:571-577`, `:639-654`, `:679-743`, `:1108-1113`, `:1383-1390`, `:1571-1608`, and `:1641-1675` still contain scalable-vector stack, VLENB, and YSXVec spill/scavenge handling.
   - `llvm/lib/Target/YuShuXin/YSXMachineFunctionInfo.h:53-69` and `:133-140` still track FPR transfer state, YSXVec stack state, padding, alignment, and vector-call state.
   - `llvm/lib/Target/YuShuXin/YSXSubtarget.h:165-180`, `:282-342`, and `:357-358` retain false-returning FP/C/V and vector-size query compatibility APIs for copied code.
   - `llvm/lib/Target/YuShuXin/YSX.h:53-60`, `:80-90`, and `:114-118` still declare compress/vector/VXRM/VMV passes that should not exist in an `rv64ima` backend.

   The plan says to remove unsupported support code, not to keep it unreachable. These files are still structured as a disabled RISC-V backend rather than a YSX backend.

4. AC-3 is still not implemented in TableGen/MC metadata.

   Round 3 deleted `YSXInstrFormatsC.td` and `YSXInstrFormatsV.td`, but the shared format and MC metadata still preserve those models:

   - `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:41-54` still defines compressed instruction formats.
   - `llvm/lib/Target/YuShuXin/YSXInstrFormats.td:64-121` and `:209-228` still define vector constraints and YSXVec TSFlags.
   - `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXBaseInfo.h:70-105`, `:291-305`, `:429-445`, and `:771-773` still encode vector policy/VL/SEW/FRM/VXRM metadata and YSXVec block size.
   - `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:1471` still defines `PROBED_STACKALLOC_YSXVec`.

   These are exactly the copied instruction-format and pseudo surfaces that Round 3's contract said should be gone or explicitly justified. They are not justified as required for `rv64ima`.

5. Round-3 success criteria were not met.

   The broad scan requested in the round contract still has backend source matches for `YSXDisabledStdExt`, `YSXDisabledVendorFeature`, `FeatureStdExtZve`, `FeatureStdExtZvl`, `YSXVec`, `GISel`, `vlenb`, `FPR`, `vxrm`, `YSXInstrFormatsC`-style compressed format names, and vector/FP helper names. The summary admits AC-3 is incomplete and defers `YSXFeatures.td`, frame lowering, instr-info, subtarget, DAG-to-DAG, and MC helper cleanup. That deferral is not justified because those files are the main AC-3 deliverable.

## Blocking Side Issues

- The remaining non-IMA MC acceptance blocks AC-2 and AC-4. Add negative tests for the exact inputs above and then remove or gate the source that made them legal.
- The generated target-feature surface blocks AC-3. Even when the driver passes unsupported features as disabled, the public feature names and generated accessors remain copied backend support.
- The current pattern of `#if 0`, false-returning vector/FP helpers, and retained pass declarations blocks AC-3 because it preserves removed subsystems instead of deleting them.

## Queued Side Issues

- `llvm/lib/TargetParser/YSXISAInfo.*` still aliases RISCV parser concepts. Keep this queued unless the required feature pruning exposes it as a build/API blocker.
- Large inactive copied check-prefix blocks under `llvm/test/CodeGen/YSX` are still a cleanup issue after the source surface is corrected. They should not displace the AC-2/AC-3 pruning work.
- Residual comments that say "RISC-V backend" in YSX files should be cleaned once the structural pruning is complete.

## Required Implementation Plan

Claude should execute this as one mainline plan in the next round:

1. Replace `YSXFeatures.td` with a minimal feature file containing only the target mechanics required for `ysx64` plus `I`, `M`/`Zmmul`, `A`/`Zaamo`/`Zalrsc`, `64bit`, `relax`, and demonstrably required tuning-only controls. Delete E, Zicsr, Zifencei, Zicntr/Zihpm, all FP/Zfinx/Zhinx, all compressed/Zc, all vector/Zv/Zve/Zvl, bitmanip, crypto, vendor, supervisor/hypervisor/privileged/profile, non-IMA atomics, and all `YSXDisabled*` compatibility records. Remove generated getter callers rather than preserving stubs.
2. Remove the remaining non-IMA MC surface. Delete `FENCE_I`, CSR instructions and aliases (`csrr`, `csrw`, `csrs`, `csrc`, `rdcycle`, `rdtime`, `rdinstret`) unless the plan is explicitly amended to include Zicsr; under the current `rv64ima` plan they must reject. Delete or gate privileged/debug/hypervisor instructions (`mret`, `sret`, `wfi`, `dret`, `sfence.vma`, `hfence.*`, etc.) so default `ysx64` rejects them. Prune `YSXSystemOperands.td` to no symbolic CSRs outside the retained plan surface.
3. Prune TableGen instruction metadata to the retained I/M/A subset. Remove compressed format records, vector constraint/TSFlag fields, vector policy/VL/SEW/FRM/VXRM operand kinds, `PROBED_STACKALLOC_YSXVec`, and unsupported pseudo/helper definitions. Keep only the 32-bit and RV64 integer formats needed by retained base, M, and A instructions.
4. Prune C++ code to match the new generated surface. Delete vector/FP/vendor/RV32/compressed/bitmanip/crypto lowering and selection from `YSXISelLowering.*`, `YSXISelDAGToDAG.*`, `YSXInstrInfo.*`, `YSXFrameLowering.*`, `YSXMachineFunctionInfo.*`, `YSXSubtarget.*`, `YSXRegisterInfo.*`, `YSXExpandPseudoInsts.cpp`, `YSXAsmParser.cpp`, disassembler, and MC helpers. Remove `#if 0` blocks and false-return compatibility APIs when the only caller is deleted.
5. Add YSX-owned negative tests for `fence.i`, `csrr a0, 0`, `csrr a0, mstatus`, `csrr a0, ssp`, `csrr a0, seed`, `rdcycle a0`, `rdtime a0`, `rdinstret a0`, `mret`, `sret`, `wfi`, `dret`, `sfence.vma`, and `hfence.vvma`. Keep the Round-3 FP/vector CSR negative tests.
6. Rebuild and revalidate both configured builds. Run the previous YSX LLVM/Clang lit subset plus the expanded MC negative tests. Run source scans for `YSXDisabled`, `FeatureStdExtZve`, `FeatureStdExtZvl`, `FeatureStdExtF`, `FeatureStdExtD`, `FeatureStdExtZca`, `YSXVec`, `FPR`, `VRRegClass`, `GPRF`, `GISel`, `Intrinsic::riscv_v`, `riscv_sf_`, `VXRM`, `VLENB`, `YSXInstrFormatsC`, and `YSXInstrFormatsV`; every remaining backend match must be deleted or documented as required for retained `rv64ima`.

## Validation Notes

- Verified the exact FP/vector CSR names from Round 2 now reject.
- Verified default MC still accepts the non-IMA examples listed in Finding 1.
- Could not rerun `ninja` because the build directories are outside this session's writable roots and Ninja failed to create `.ninja_lock`/`VCSRevision.h.tmp`.
- Verified `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

REQUIRES MORE WORK
