# Round 1 Summary

## Work Completed
- Fixed RVV pressure diagnostics so `rvv-spill-slots` counts only scalable
  vector frame objects that are true `MachineFrameInfo` spill slots.
- Scoped load/store clustering suppression to high-pressure RVV scheduling
  regions by wrapping the generic cluster mutations under the pressure flag.
- Added default-off scheduler coverage, masked-vector validity coverage, and
  reload rematerialization safety tests for safe, volatile, may-alias-store,
  call-intervened, atomic, and ordered cases.
- Rewrote `docs/report.md` with the exact workload extraction script,
  measurement loop, spill/reload counts, diagnostics output, and snippets.

## Files Changed
- `.humanize/rlcr/2026-04-24_23-29-30/`: round 1 contract, prompt/review
  state, tracker updates, and this summary.
- `llvm/lib/Target/RISCV/RISCVFrameLowering.cpp`
- `llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp`
- `llvm/lib/Target/RISCV/RISCVMachineScheduler.h`
- `llvm/lib/Target/RISCV/RISCVTargetMachine.cpp`
- `llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-reload.ll`
- `llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-reload-atomic.mir`
- `docs/report.md`

## Validation
- `ninja -C build-riscv llc` passed.
- `build-riscv/bin/llvm-lit -sv llvm/test/CodeGen/RISCV/rvv/yushuxin-vfexp.ll llvm/test/MC/RISCV/rvv/yushuxin-vfexp.s llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-aware-sched-workloads.ll llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-reload.ll llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-reload-atomic.mir llvm/test/CodeGen/RISCV/features-info.ll` passed 6/6.
- Direct alloca diagnostic repro printed `rvv-spill-slots=0`.
- Regenerated report measurements from the checked-in workload file with the
  commands now recorded in `docs/report.md`.
- `git diff --check` passed.

## Remaining Items
- No known unresolved implementation items.

## BitLesson Delta
- Action: none
- Lesson ID(s): NONE
- Notes: No reusable lesson was added; the selector again returned placeholder
  output and `.humanize/bitlesson.md` has no applicable entries.
