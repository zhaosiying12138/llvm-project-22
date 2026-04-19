# Round 23 Implementation Review

Mainline Progress Verdict: ADVANCED

Round 23 advanced the mainline by deleting the exact short-forward-branch,
CCMOV, scheduler, and custom-ISD helper residue called out in Round 22. The
required Round-23 source scan is clean, RISCV backend diff remains zero, and
`git diff --check` is clean. The full `docs/plan.md` goal is still not complete:
YSX still exposes removed compressed/vendor/custom RISC-V relocation surface
through `.reloc`.

## Goal Alignment Summary

ACs: 4/4 addressed | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: MAINTAINED. The RISCV backend diff remains zero, and the reviewed
  Round-23 changes do not add a new RISCV dependency.
- AC-2: PARTIAL. The reviewed unsupported feature/CSR/`.insn` surfaces remain
  closed, but `.reloc` still accepts removed compressed/vendor/custom RISC-V
  relocation names for `ysx64`.
- AC-3: PARTIAL. The Round-23 SFB/CCMOV/scheduler/custom-ISD residue is gone,
  but the YSX MC layer still imports broad RISC-V relocation name tables that
  include removed non-rv64ima surface.
- AC-4: PARTIAL. Existing focused suites reportedly pass, but there is no
  negative YSX MC coverage for rejected `.reloc` compressed/vendor/custom
  relocation names.

Tracker state was corrected in the mutable section of `goal-tracker.md`: Plan
Version 48 records this Round-23 review, keeps task3/task6 active, replaces the
resolved SFB/scheduler blocker with the `.reloc` relocation-surface blocker, and
marks the Round-23 deletion slice as review-partial. The immutable section was
not modified.

## Mainline Gaps

1. AC-2/AC-3 remain incomplete: YSX accepts removed RISC-V relocation names
   through `.reloc`.

   `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXAsmBackend.cpp:48` through
   `YSXAsmBackend.cpp:54` imports all names from both `RISCV.def` and
   `RISCV_nonstandard.def` into `YSXAsmBackend::getFixupKind`. That includes
   compressed relocations such as `R_RISCV_RVC_BRANCH`, vendor/nonstandard
   names such as `R_RISCV_QC_ABS20_U`,
   `R_RISCV_NDS_BRANCH_10`, and
   `R_RISCV_CHERIOT1_COMPARTMENT_HI`, plus the generic vendor/custom relocation
   range. `YSXELFObjectWriter.cpp:74` then returns relocation fixup kinds
   directly, so the names assemble successfully instead of rejecting.

   Manual probes with the current YSX-only `llvm-mc` confirm the leak:

   ```text
   .reloc ., R_RISCV_RVC_BRANCH, sym                 # accepted
   .reloc ., R_RISCV_QC_ABS20_U, sym                 # accepted
   .reloc ., R_RISCV_NDS_BRANCH_10, sym              # accepted
   .reloc ., R_RISCV_CHERIOT1_COMPARTMENT_HI, sym    # accepted
   .reloc ., R_RISCV_VENDOR, sym                     # accepted
   ```

   This is the same class of visible removed-surface leak as the earlier `.insn`
   opcode issue. It directly contradicts the plan requirement that YSX reject
   compressed, vendor, and other removed surfaces and not preserve unsupported
   feature support code.

## Blocking Side Issues

1. The `.reloc` relocation-name leak blocks completion of the current mainline
   objective. It is both user-visible AC-2 behavior and copied unsupported MC
   support code under AC-3.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued and non-blocking because
   review continues against `docs/plan.md`.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued. I found
   no new evidence that they leak unsupported target features into IR/codegen.

## Required Implementation Plan

Claude should complete this as the next mainline slice:

1. In `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXAsmBackend.cpp`, replace the
   broad `RISCV.def`/`RISCV_nonstandard.def` `StringSwitch` import with an
   explicit whitelist of relocation names that YSX actually supports for
   rv64ima/RISC-V ELF compatibility. Do not include `R_RISCV_RVC_BRANCH`,
   `R_RISCV_RVC_JUMP`, `R_RISCV_VENDOR`, any `R_RISCV_CUSTOM*`, or any
   `RISCV_nonstandard.def` names.
2. Keep existing YSX `.reloc` coverage for retained object-compatible names
   such as `R_RISCV_NONE`, `R_RISCV_32`, `R_RISCV_64`,
   `R_RISCV_32_PCREL`, and `R_RISCV_SET32` passing.
3. Add negative MC tests, preferably in
   `llvm/test/MC/YSX/unsupported-features.s` or a dedicated YSX `.reloc`
   negative test, covering at minimum `R_RISCV_RVC_BRANCH`,
   `R_RISCV_RVC_JUMP`, `R_RISCV_VENDOR`, `R_RISCV_CUSTOM192`,
   `R_RISCV_QC_ABS20_U`, `R_RISCV_NDS_BRANCH_10`, and
   `R_RISCV_CHERIOT1_COMPARTMENT_HI`.
4. Require `rg "RISCV_nonstandard|R_RISCV_VENDOR|R_RISCV_RVC_|R_RISCV_QC_|R_RISCV_NDS_|R_RISCV_CHERIOT|R_RISCV_CUSTOM" llvm/lib/Target/YuShuXin llvm/test/MC/YSX`
   to show no YSX source matches and only intentional negative-test matches.
5. Rebuild the YSX-only and RISCV+YSX configured target sets, rerun the focused
   YSX lit suites, rerun retained rv64ima smoke tests and unsupported-feature
   probes, run `git diff --check`, and verify
   `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

REQUIRES MORE WORK
