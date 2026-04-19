# Round 24 Contract

## Mainline Objective

Close the YSX `.reloc` removed-relocation-name leak by replacing broad copied
RISC-V relocation-name imports with an explicit rv64ima/object-compatible YSX
whitelist and adding focused MC coverage.

## Target Acceptance Criteria

- AC-2: YSX rejects compressed, vendor, custom, and nonstandard relocation
  names through `.reloc` while retaining object-compatible relocation names.
- AC-3: YSX MC source no longer imports unsupported RISC-V relocation tables or
  exposes removed relocation-name surface.

## Blocking Issues

- `YSXAsmBackend.cpp` imports all names from `RISCV.def` and
  `RISCV_nonstandard.def`, so `.reloc` currently accepts removed compressed,
  vendor, custom, and nonstandard relocation names for `ysx64`.
- Missing negative MC coverage for rejected `.reloc` names lets this surface
  regress.

## Queued Out Of Scope

- Goal tracker immutable AC-list drift remains queued because mutable tracking
  and review continue against `docs/plan.md`.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because
  current evidence does not show unsupported codegen feature leakage.
- Any further unrelated backend source-size cleanup is queued unless required
  to make the relocation whitelist build or test correctly.

## Success Criteria

- `YSXAsmBackend.cpp` uses an explicit whitelist for retained relocation names
  and does not include `RISCV_nonstandard.def` or unsupported compressed/vendor
  relocation names.
- YSX MC tests reject at least `R_RISCV_RVC_BRANCH`, `R_RISCV_RVC_JUMP`,
  `R_RISCV_VENDOR`, `R_RISCV_CUSTOM192`, `R_RISCV_QC_ABS20_U`,
  `R_RISCV_NDS_BRANCH_10`, and `R_RISCV_CHERIOT1_COMPARTMENT_HI`.
- Retained `.reloc` names such as `R_RISCV_NONE`, `R_RISCV_32`,
  `R_RISCV_64`, `R_RISCV_32_PCREL`, and `R_RISCV_SET32` still assemble.
- The required source/test scan has no unsupported YSX source matches and only
  intentional negative-test matches.
- YSX-only build, combined RISCV+YSX build, focused YSX lit suites,
  smoke/negative probes, `git diff --check`, and RISCV zero-diff validation
  pass before commit.
