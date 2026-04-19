Your work is not finished. Read and execute the below with ultrathink.

## Original Implementation Plan

**IMPORTANT**: Before proceeding, review the original plan you are implementing:
@docs/plan.md

This plan contains the full scope of work and requirements. Ensure your work aligns with this plan.

---

## Round Re-anchor (REQUIRED FIRST STEP)

Before writing code:
- Re-read @docs/plan.md
- Re-read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md
- Re-read the most recent round summaries/reviews that led to this round
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-29-contract.md

Your round contract must contain:
- Exactly one **mainline objective**
- The 1-2 target ACs for this round
- Which issues are truly **blocking** that mainline objective
- Which issues are **queued** and explicitly out of scope
- Concrete success criteria for this round

Do not start implementation until the round contract exists.

## Task Lane Rules

Use the Task system (TaskCreate, TaskUpdate, TaskList) with one required tag per task:
- `[mainline]` for plan-derived work that directly advances this round's objective
- `[blocking]` for issues that prevent the mainline objective from succeeding safely
- `[queued]` for non-blocking bugs, cleanup, or follow-up work

Rules:
- `[mainline]` work is the round's primary success condition
- `[blocking]` work is allowed only when it truly blocks the mainline objective
- `[queued]` work must be documented but must NOT replace the round objective
- If a new bug does not block the current objective, tag it `[queued]` and keep moving on mainline work

Before executing each task in this round:
1. Read @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/bitlesson.md
2. Run `bitlesson-selector` for each task/sub-task
3. Follow selected lesson IDs (or `NONE`) during implementation

---
Below is Codex's review result:
<!-- CODEX's REVIEW RESULT START -->
# Round 28 Review Result

Mainline Progress Verdict: ADVANCED

Round 28 fixed the exact Round-27 AC-3 blocker. The copied MIPS/CCMov/load-store-pair source residue is gone from the reviewed YSX backend files, and the previous blocker scans are clean. Full plan completion is still not valid because a broader frontend/test review found another inherited removed-surface leak: `ysx64` still accepts RISC-V FP/vector/CSR inline-asm clobber names through Clang's GCC register-name table, and YSX CodeGen tests still actively contain copied removed clobbers.

## Mainline Gaps

1. AC-2/AC-4 still leak removed FP/vector/CSR register names through YSX Clang inline-asm clobbers.

   `YSX64TargetInfo` derives from `RISCV64TargetInfo` but does not override `getGCCRegNames()` or `getGCCRegAliases()`:

   - `clang/lib/Basic/Targets/RISCV.h:235` defines `class YSX64TargetInfo : public RISCV64TargetInfo`.
   - `clang/lib/Basic/Targets/RISCV.h:250-265` overrides builtins, constraints, features, and target attributes, but not the GCC register-name or alias tables.
   - `clang/lib/Basic/Targets/RISCV.cpp:25-50` exposes inherited RISC-V GCC register names including `f0`-`f31`, `v0`-`v31`, `fflags`, `frm`, `vtype`, `vl`, `vxsat`, `vxrm`, and `sf.vcix_state`.
   - `clang/lib/Basic/Targets/RISCV.cpp:53-71` exposes inherited FP aliases such as `ft0`, `fs0`, and `fa0`.

   Direct probes show YSX accepts these removed clobber names instead of rejecting them:

   ```bash
   printf 'void f(void){ asm volatile("" ::: "f8"); }\n' |
     /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm/bin/clang \
       --target=ysx64-unknown-elf -x c -c -o /tmp/ysx-review-f8.o -
   # EXIT: 0

   printf 'void f(void){ asm volatile("" ::: "vtype", "vl", "vxsat", "vxrm"); }\n' |
     /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm/bin/clang \
       --target=ysx64-unknown-elf -x c -c -o /tmp/ysx-review-vcsr.o -
   # EXIT: 0
   ```

   This contradicts the plan's `rv64ima`-only surface and the Round-7 claim that inherited FP/vector inline-asm surfaces were hardened. The existing tests cover invalid `"f"`/`"vr"` constraints, but not named clobbers.

2. AC-4 still has active copied removed-surface clobbers in YSX-owned CodeGen tests.

   These are not inactive prefixes; they are live IR in tests run for `ysx64`:

   - `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll:19` contains `~{f8},~{f9}`.
   - `llvm/test/CodeGen/YSX/inline-asm-mem-constraint.ll:2350` contains `~{vtype},~{vl},~{vxsat},~{vxrm}`.

   Both tests currently pass with `llc -mtriple=ysx64`, which means the YSX test corpus still relies on copied FP/vector/CSR clobber surface even though YSX is supposed to be pruned to `rv64ima`.

3. The Round-28 MIPS/CCMov source blocker itself is fixed.

   These scans are clean:

   ```bash
   rg -n "MIPS|CCMov|ccmov|mips\\.ccmov|RV64I-CCMOV|RV64-MIPS" \
     llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX \
     clang/test/Driver/YSX clang/test/CodeGen/YSX

   rg -n "useMIPSLoadStorePairs|useMIPSCCMovInsn|isPairableLdStInstOpc|isLdStSafeToPair" \
     llvm/lib/Target/YuShuXin
   ```

## Blocking Side Issues

1. The inherited Clang GCC register-name/alias surface blocks full AC-2 and AC-4 completion.

   This is plan-derived work, not a queued cleanup. It is a user-visible path where `ysx64` accepts removed FP/vector/CSR names. It also explains why stale YSX CodeGen tests can still carry removed clobbers without failing.

## Queued Side Issues

1. Goal tracker immutable AC-list drift remains queued. Continue reviewing against `docs/plan.md`; do not edit the immutable tracker section.
2. CPU/tune target-attribute warn-and-ignore diagnostics remain queued because this review did not find evidence that they leak unsupported feature state.

## Goal Alignment Summary

ACs: 4/4 addressed (3/4 met; AC-2/AC-4 partial for inline-asm clobber names) | Forgotten items: 1 | Unjustified deferrals: 0

- AC-1: Still addressed. `git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV clang/test/Driver/RISCV clang/test/CodeGen/RISCV | wc -l` reports `0`, and `git diff --check HEAD^..HEAD` passes.
- AC-2: Partially unmet. Most reviewed unsupported ISA paths still reject, but YSX Clang accepts removed FP/vector/CSR inline-asm clobber names.
- AC-3: Advanced. The Round-27 MIPS/CCMov/load-store-pair source residue is removed, but the inherited Clang register-name surface is still copied RISC-V support for removed features.
- AC-4: Partially unmet. The CCMOV/MIPS stale tests remain fixed, but YSX-owned CodeGen tests still actively contain copied FP/vector/CSR clobbers.

Forgotten item: the prior Clang inline-asm hardening covered constraints such as `"f"` and `"vr"` but did not cover named clobbers validated through `getGCCRegNames()`/`getGCCRegAliases()`.

Deferred items: none in `Explicitly Deferred`. The clobber/register-name leak must be fixed now; it should not be moved to queued work.

Plan evolution: I updated the mutable tracker to verify the Round-28 MIPS/CCMov cleanup, reopen active work for the inherited Clang clobber/register-name leak, add the new blocking issue, and record the Round-28 review result.

## Required Implementation Plan

1. In `clang/lib/Basic/Targets/RISCV.h`, add `YSX64TargetInfo` overrides for `getGCCRegNames()` and `getGCCRegAliases()`.
2. In `clang/lib/Basic/Targets/RISCV.cpp`, implement those overrides with only retained rv64ima GPR register names and aliases:
   - names: `x0` through `x31`;
   - aliases: `zero`, `ra`, `sp`, `gp`, `tp`, `t0`-`t6`, `s0`-`s11`, `fp`, and `a0`-`a7`;
   - do not include `f*`, `ft*`, `fs*`, `fa*`, `v*`, `fflags`, `frm`, `fcsr`, `vtype`, `vl`, `vlenb`, `vxsat`, `vxrm`, or vendor CSR names.
3. Add YSX Clang negative coverage, preferably in `clang/test/Driver/YSX/target-options.c` or a dedicated YSX frontend test, proving that `ysx64` rejects removed clobber names:
   - `asm volatile("" ::: "f8")`;
   - `asm volatile("" ::: "fs0")`;
   - `asm volatile("" ::: "v0")`;
   - `asm volatile("" ::: "vtype")`;
   - `asm volatile("" ::: "vl")`;
   - `asm volatile("" ::: "vxsat")`;
   - `asm volatile("" ::: "vxrm")`;
   - retain a positive `x9`/`s1` clobber probe.
4. Remove copied removed clobbers from YSX CodeGen tests:
   - delete `~{f8},~{f9}` from `llvm/test/CodeGen/YSX/inline-asm-clobbers.ll` and refresh checks if needed;
   - delete `~{vtype},~{vl},~{vxsat},~{vxrm}` from `llvm/test/CodeGen/YSX/inline-asm-mem-constraint.ll` and refresh checks if needed.
5. Add an LLVM IR negative guard if `llc -mtriple=ysx64` still silently accepts removed named clobbers after the Clang fix. Implement this in the existing YSX unsupported-IR guard by parsing `InlineAsm::ParseConstraints()` and diagnosing clobber codes for removed FP/vector/CSR names. Add focused negative `llvm/test/CodeGen/YSX` coverage for any such backend-level rejection.
6. Re-run:

   ```bash
   ninja -C /home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm LLVMYSXCodeGen llvm-mc clang llc
   ninja -C /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld
   /home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm/bin/llvm-lit -q \
     llvm/test/MC/YSX llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX
   rg -n "MIPS|CCMov|ccmov|mips\\.ccmov|RV64I-CCMOV|RV64-MIPS" \
     llvm/lib/Target/YuShuXin llvm/test/CodeGen/YSX llvm/test/MC/YSX \
     clang/test/Driver/YSX clang/test/CodeGen/YSX
   rg -n "~\\{f[0-9]|~\\{ft|~\\{fs|~\\{fa|~\\{v[0-9]|~\\{vtype|~\\{vl|~\\{vxsat|~\\{vxrm|~\\{fflags|~\\{frm|~\\{fcsr" \
     llvm/test/CodeGen/YSX clang/test/Driver/YSX clang/test/CodeGen/YSX
   git diff --check
   git diff -- llvm/lib/Target/RISCV llvm/test/MC/RISCV llvm/test/CodeGen/RISCV \
     clang/test/Driver/RISCV clang/test/CodeGen/RISCV | wc -l
   ```

## Validation Notes

- PASS: exact Round-27 MIPS/CCMov source/test scan has no matches.
- PASS: deleted MIPS/load-store-pair helper names have no YSX matches.
- PASS: Round-25 stale-prefix scan has no matches.
- PASS: `git diff --check HEAD^..HEAD`.
- PASS: RISCV source/test zero-diff for the reviewed Round-28 commit.
- FAIL: direct Clang probes for `ysx64` accept removed FP/vector/CSR clobber names.
- FAIL: YSX-owned CodeGen tests still contain active copied FP/vector/CSR clobbers.

REQUIRES MORE WORK
<!-- CODEX's REVIEW RESULT  END  -->
---

## Goal Tracker Reference

Before starting work, **read** @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/goal-tracker.md to understand:
- The Ultimate Goal and Acceptance Criteria you're working toward
- Which tasks are Active, Completed, or Deferred
- Which side issues are blocking vs queued
- Any Plan Evolution that has occurred
- The latest side-issue state that needs attention

**IMPORTANT**: Keep the mutable section of `goal-tracker.md` up to date during the round.
Do NOT change the immutable section after Round 0.
If you cannot safely reconcile the tracker yourself, include an optional "Goal Tracker Update Request" section in your summary (see below).

## Mainline Guardrails

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-29-contract.md stable for this round
- Do not let queued issues take over the round
- If Codex reported several findings, classify them into:
  - mainline gaps
  - blocking side issues
  - queued side issues
- Only mainline gaps and blocking side issues should drive the next code changes

---

Note: You MUST NOT try to exit by lying, editing loop state files, or executing `cancel-rlcr-loop`.

After completing the work, please:
0. If the `code-simplifier` plugin is installed, use it to review and optimize your code. Invoke via: `/code-simplifier`, `@agent-code-simplifier`, or `@code-simplifier:code-simplifier (agent)`
1. Commit your changes with a descriptive commit message
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-29-summary.md
3. Run `/home/zhaosiying/.codex/skills/humanize/scripts/rlcr-stop-gate.sh` to advance the loop in-session

## Task Tag Routing Reminder

Follow the plan's per-task routing tags strictly:
- `coding` task -> Claude executes directly
- `analyze` task -> execute via `/humanize:ask-codex`, then integrate the result
- Keep Goal Tracker Active Tasks columns `Tag` and `Owner` aligned with execution

**Optional fallback**: if you could not safely update the mutable section of `goal-tracker.md` directly, include this section in your summary:
```markdown
## Goal Tracker Update Request

### Requested Changes:
- [E.g., "Mark Task X as completed with evidence: tests pass"]
- [E.g., "Add to Blocking Side Issues: bug Y blocks AC-2"]
- [E.g., "Add to Queued Side Issues: cleanup Z is non-blocking"]
- [E.g., "Plan Evolution: changed approach from A to B because..."]
- [E.g., "Defer Task Z because... (impact on AC: none/minimal)"]

### Justification:
[Explain why these changes are needed and how they serve the Ultimate Goal]
```

Codex will review your request and reconcile the Goal Tracker if justified.
