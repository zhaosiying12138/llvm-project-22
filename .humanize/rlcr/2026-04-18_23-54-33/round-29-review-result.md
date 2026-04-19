# Round 29 Full Goal Alignment Review

Mainline Progress Verdict: ADVANCED

Round 29 fixed the Round-28 blocker. YSX no longer inherits the RISC-V
FP/vector/CSR GCC register-name surface through Clang inline-asm clobbers, the
active copied removed clobbers are gone from YSX-owned CodeGen tests, and a
backend guard now rejects removed clobbers in IR that bypasses Clang. With this
fix, all acceptance criteria from `docs/plan.md` are met and there are no
deferred items.

## Part 1: Goal Tracker Audit

The goal-tracker immutable section still has the known Round-0 drift: it lists
only AC-1 and AC-2, while `docs/plan.md` contains AC-1 through AC-4. I reviewed
against `docs/plan.md` as the source of truth and updated only the mutable
section.

| AC | Status | Evidence (if MET) | Blocker (if NOT MET) | Justification (if DEFERRED) |
|----|--------|-------------------|----------------------|-----------------------------|
| AC-1 | MET | Existing YSX-only build products register only `ysx64`; combined build products register RISCV and YSX together. `llc -mtriple=ysx64-unknown-elf` emits rv64ima code, `git diff --check HEAD^..HEAD` passes, and RISCV source/test diff is `0`. | - | - |
| AC-2 | MET | Default YSX codegen emits `.attribute 5, "rv64ima"`; retained IMA MC smoke assembly succeeds; unsupported FP/vector/Zbb MC probes reject; Clang rejects removed clobbers `f8`, `fs0`, `fa0`, `v0`, `vtype`, `vl`, `vxsat`, `vxrm`, `fcsr`, `fflags`, `frm`, and `vlenb`, while accepting retained `x9`/`s1`. | - | - |
| AC-3 | MET | YSX backend is 26,043 lines versus RISCV's 136,068; the reviewed removed source/test blockers are gone, including MIPS/CCMov/load-store-pair residue and inherited FP/vector/CSR clobber tables. `llc -mattr=help` exposes only retained YSX features: 64bit, I, M, A, Zmmul, Zaamo, Zalrsc, relax, and exact-asm. | - | - |
| AC-4 | MET | YSX-owned LLVM and Clang tests exist under `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`, and `clang/test/Driver/YSX`; Round 29 added Clang clobber negative coverage and `llvm/test/CodeGen/YSX/unsupported-inline-asm-clobbers.ll`; active copied removed clobbers are gone except intentional negative coverage. | - | - |

Forgotten items detection: no new forgotten plan items found. The prior
forgotten clobber path through `getGCCRegNames()`/`getGCCRegAliases()` is now
covered and verified.

Deferred items audit: `Explicitly Deferred` has no rows. No deferral
contradicts the Ultimate Goal.

Goal completion summary:

```text
Acceptance Criteria: 4/4 met (0 deferred)
Active Tasks: 0 remaining
Estimated remaining rounds: 0
Critical blockers: none
```

## Part 2: Mainline Drift Audit

Round 29 had a clear singular objective: close the inherited Clang
register-name/clobber leak and remove stale copied clobber tests. Claude
advanced a mainline AC-2/AC-4 blocker directly. Recent rounds have addressed
distinct blockers rather than circling: `.reloc`, stale copied tests,
MIPS/CCMov source residue, and now clobber/register-name exposure.

```text
Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 0
Queued Side Issues: 2
```

Queued side issues:

1. Goal tracker immutable AC-list drift remains a process artifact; continue
   using `docs/plan.md` as source of truth if any later audit runs.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain non-blocking;
   reviewed probes have not shown unsupported feature leakage.

## Part 3: Implementation Review

Round 29's implementation is sound for the reviewed defect.
`YSX64TargetInfo` now declares and implements YSX-specific
`getGCCRegNames()` and `getGCCRegAliases()` overrides
(`clang/lib/Basic/Targets/RISCV.h:255`,
`clang/lib/Basic/Targets/RISCV.cpp:74`). The YSX table contains only
`x0`-`x31` plus retained GPR aliases, including `s0`/`fp` and `s1`, and does
not include FP registers, vector registers, or FP/vector CSR names.

The backend guard in `llvm/lib/Target/YuShuXin/YSXCodeGenPrepare.cpp:116`
parses inline-asm constraints and diagnoses removed clobbers such as `f*`,
`ft*`, `fs*`, `fa*`, `v*`, `fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vlenb`,
`vxsat`, and `vxrm`. Direct `llc` review on
`llvm/test/CodeGen/YSX/unsupported-inline-asm-clobbers.ll` reports the expected
YSX diagnostic instead of silently accepting the removed clobber.

The test changes match the plan. `clang/test/Driver/YSX/target-options.c:19`
keeps a positive retained `x9`/`s1` clobber probe and lines 20-26 add negative
coverage for removed names. The previous active copied `~{f8},~{f9}` and
`~{vtype},~{vl},~{vxsat},~{vxrm}` clobbers are no longer present in
`inline-asm-clobbers.ll` or `inline-asm-mem-constraint.ll`; the remaining scan
hits only intentional negative coverage.

Validation I independently ran:

- PASS: `git diff --check HEAD^..HEAD`.
- PASS: RISCV source/test diff is `0`.
- PASS: YSX-only `llc --version` registers only `ysx64`; combined build
  registers RISCV and YSX.
- PASS: `llc -mtriple=ysx64-unknown-elf` emits rv64ima code.
- PASS: retained `add`, `mul`, and `amoadd.w` MC smoke assembly succeeds.
- PASS: FP, vector, and `rv64ima_zbb` MC probes reject.
- PASS: Clang rejects removed clobbers and accepts retained `x9`/`s1`.
- PASS: `llvm-lit -q llvm/test/CodeGen/YSX/unsupported-inline-asm-clobbers.ll`
  passes with the YSX-only build.
- PASS: reviewed removed-surface scans for MIPS/CCMov and active stale
  clobbers are clean except intentional negative coverage.

I could not independently rerun the full Clang lit suite in this sandbox:
`llvm-lit` attempts to create files under the external build test exec root,
which is read-only here. I also could not independently rerun Ninja builds for
the same external-write reason. The direct probes above validate the changed
behavior, and Claude's build/lit claims have no contradictory evidence.

## Part 4: Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

1. Bumped Plan Version to 60.
2. Added a Round-29 review plan-evolution entry.
3. Cleared the Round-29 active tasks.
4. Cleared the resolved blocking side issue.
5. Added completed/verified rows for the Round-29 clobber cleanup and final
   revalidation.

The immutable section was not modified.

## Part 5: Progress Stagnation Check

Development is not stagnating. Rounds 24-29 each removed a concrete,
review-identified plan blocker and did not repeat the same unresolved defect:
relocation names, stale copied extension checks, CCMOV/MIPS stale tests,
MIPS/CCMov source residue, and inherited clobber/register-name exposure. The
last reviewed blocker is now fixed.

## Action Items

Mainline Gaps: none.

Blocking Side Issues: none.

Queued Side Issues:

1. Immutable tracker AC-list drift; keep `docs/plan.md` as source of truth.
2. CPU/tune target-attribute diagnostics; revisit only if future policy
   requires hard errors instead of ignored warnings.

COMPLETE
