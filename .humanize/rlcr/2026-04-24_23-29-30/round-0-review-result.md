Mainline Progress Verdict: ADVANCED

Goal Alignment Summary

ACs: 6/6 addressed | Forgotten items: 0 | Unjustified deferrals: 0

**Mainline Gaps**

1. `rvv-spill-slots` is not a spill-slot count, so AC-5 is not complete. In `llvm/lib/Target/RISCV/RISCVFrameLowering.cpp:1819-1830`, the report counts every `TargetStackID::ScalableVector` object, not actual spill slots. A simple repro with only an RVV alloca:

   ```sh
   build-riscv/bin/llc -mtriple=riscv64 -mattr=+v,+zvl128b -riscv-v-reg-pressure-report -o /dev/null - <<'EOF'
   define void @sv_alloca() {
   entry:
     %x = alloca <vscale x 4 x i32>, align 16
     ret void
   }
   EOF
   ```

   prints `riscv-v-reg-pressure-report: function=sv_alloca rvv-scalable-stack-bytes=16 rvv-spill-slots=1 fixed-stack-estimate=0`, even though there are zero spills. The tracker claimed AC-5 verified, but the implementation currently overcounts non-spill scalable vector frame objects.

2. AC-3 and AC-4 are under-tested relative to the plan, and the summary overstates verification. The only scheduler/reload regression file, `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll:1-17`, has only flag-enabled RUN lines and positive checks. It does not include the required default-off negative test for `-riscv-v-reg-pressure-aware-sched`, and it does not cover the promised reload-pass safety cases for volatile, atomic, may-alias-store, or call-intervened scenarios. The lit run in the summary was just 4 focused tests, so the claim that existing RISCV CodeGen tests kept passing without the new flags is not actually verified either.

3. The scheduler scope is broader than the planned conservative design. `llvm/lib/Target/RISCV/RISCVTargetMachine.cpp:292-300` disables generic load/store clustering for the whole function whenever `-riscv-v-reg-pressure-aware-sched` is set, while the region-pressure check is computed later in `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp:101-106`. That means non-pressure regions lose clustering too, which does not match the plan’s “only for pressure-heavy vector regions” constraint.

4. AC-6 is not fully satisfied by the current report. `docs/report.md:30-55` uses placeholder `INPUT.ll` commands instead of exact reproducible commands, does not explain how the per-workload counts in `docs/report.md:61-68` were isolated from the shared multi-function workload file, and does not include the diagnostics snippets that the plan required alongside the counts.

**Blocking Side Issues**

- None beyond the mainline gaps above.

**Queued Side Issues**

- `goal-tracker.md` had drifted ahead of the evidence by marking AC-3 through AC-6 as completed and verified. I corrected the mutable section directly so the next round starts from an accurate state.

**Directive Next-Step Plan**

1. Fix AC-5 first: change the diagnostics path in `RISCVFrameLowering` to count true spill-slot frame indices rather than every scalable-vector object, then add a regression that proves an RVV alloca does not increment `rvv-spill-slots`.
2. Tighten AC-3 to the planned scope: keep the hidden flag default-off, but only suppress clustering when the current scheduling region is actually classified as high-pressure RVV. Add one default-off negative test and one masked-vector validity check.
3. Finish AC-4 verification with a dedicated reload-pass regression file that contains one same-block safe positive case and explicit negative cases for volatile, atomic, may-alias store, and call-intervened patterns.
4. Rewrite `docs/report.md` with exact commands used for each required workload and optimization level, include the report-flag diagnostics output, and document the exact spill/reload counting procedure used to derive every table row.
