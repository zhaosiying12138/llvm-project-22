# Round 29 Contract

## Mainline Objective

Close the inherited YSX Clang inline-asm clobber register-name surface so `ysx64` exposes only retained rv64ima GPR clobber names and tests no longer rely on removed FP/vector/CSR clobbers.

## Target ACs

- AC-2: YSX must reject removed FP, vector, CSR, and vendor clobber names through Clang.
- AC-4: YSX-owned tests must not contain active copied clobbers for removed surfaces and must include negative coverage for the rejected names.

## Blocking Issues

- `YSX64TargetInfo` inherits RISC-V GCC register names and aliases, allowing removed FP/vector/CSR clobber names such as `f8`, `fs0`, `v0`, `vtype`, `vl`, `vxsat`, and `vxrm`.
- YSX CodeGen tests still actively contain copied removed clobbers: `~{f8},~{f9}` and `~{vtype},~{vl},~{vxsat},~{vxrm}`.

## Queued Issues

- Goal tracker immutable AC-list drift remains queued; continue using `docs/plan.md` as the complete AC source without editing the immutable tracker section.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because they do not block this clobber/register-name cleanup round.

## Success Criteria

- `YSX64TargetInfo` overrides GCC register names and aliases with only `x0`-`x31` plus retained GPR aliases.
- Clang `ysx64` rejects the reviewed removed clobber names and still accepts retained GPR clobbers such as `x9` and `s1`.
- YSX CodeGen tests no longer contain active removed FP/vector/CSR clobber strings.
- If the backend still accepts removed clobber names at IR level, a YSX unsupported-IR guard and negative CodeGen coverage are added.
- YSX-only build targets, combined RISCV+YSX build targets, focused YSX lit, clobber probes, removed-surface scans, `git diff --check`, and RISCV source/test zero-diff pass.
