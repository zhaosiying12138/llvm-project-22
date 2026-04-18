# Goal Tracker

<!--
This file tracks the ultimate goal, acceptance criteria, and plan evolution.
It prevents goal drift by maintaining a persistent anchor across all rounds.

RULES:
- IMMUTABLE SECTION: Do not modify after initialization
- MUTABLE SECTION: Update each round, but document all changes
- Every task must be in one of: Active, Completed, or Deferred
- Deferred items require explicit justification
-->

## IMMUTABLE SECTION
<!-- Do not modify after initialization -->

### Ultimate Goal

Create a new standalone `YSX` backend in `llvmorg-22.1.3` by copying the
RISC-V backend into `llvm/lib/Target/YuShuXin`, renaming backend-visible
symbols and files from `RISCV` to `YSX`, and pruning the implementation to a
minimal `rv64ima`-only target exposed as `ysx64`.

The implementation must preserve the existing RISCV backend unchanged, allow
YSX to build without RISCV, and allow RISCV plus YSX to co-build with static
libraries and no collisions in symbols, options, or target registration.

### Acceptance Criteria
<!-- Each criterion must be independently verifiable -->
<!-- Claude must extract or define these in Round 0 -->


Following TDD philosophy, each criterion includes positive and negative tests
for deterministic verification.

- AC-1: YSX exists as a new standalone LLVM backend with its own target
  identity and init entrypoints.
  - Positive Tests (expected to PASS):
    - `llc -mtriple=ysx64-unknown-elf` initializes the YSX backend without
      requiring the RISCV backend in `LLVM_TARGETS_TO_BUILD`.
    - A combined `LLVM_TARGETS_TO_BUILD="RISCV;YSX"` static-library build
      links successfully and registers both targets.
  - Negative Tests (expected to FAIL):
    - Building with only `YSX` enabled must fail if any YSX source still
      includes or links against RISCV target libraries or generated headers.
    - Any duplicate `cl::opt` registration between RISCV and YSX must be
      rejected during combined static builds.
- AC-2: YSX only supports the `rv64ima` ISA/ABI surface.
  - Positive Tests (expected to PASS):
    - `--target=ysx64-unknown-elf` defaults to the `rv64ima` instruction set
      with `lp64`.
    - YSX asm, disassembly, MC, and codegen handle the retained integer,
      multiply/divide, and atomic instructions required by `rv64ima`.
  - Negative Tests (expected to FAIL):
    - YSX rejects RV32, floating-point, compressed, vector, vendor, and other
      removed extensions through target parsing and Clang driver tests.

---

## MUTABLE SECTION
<!-- Update each round with justification for changes -->

### Plan Version: 2 (Updated: Round 0)

#### Plan Evolution Log
<!-- Document any changes to the plan with justification -->
| Round | Change | Reason | Impact on AC |
|-------|--------|--------|--------------|
| 0 | Initial plan | - | - |
| 0 | Marked backend extraction, pruning, integration, and validation complete; retained line-count gap as deferred follow-up | Round work finished and was verified by fresh builds/tests | AC-1, AC-2, AC-3, AC-4 satisfied; historical size target remains non-blocking follow-up |

#### Active Tasks
<!-- Mainline tasks only: each task must directly advance the current round objective and carry routing metadata -->
| Task | Target AC | Status | Tag | Owner | Notes |
|------|-----------|--------|-----|-------|-------|

### Blocking Side Issues
<!-- Only issues that directly block current mainline progress belong here -->
| Issue | Discovered Round | Blocking AC | Resolution Path |
|-------|-----------------|-------------|-----------------|

### Queued Side Issues
<!-- Non-blocking issues stay queued and must NOT replace the round objective -->
| Issue | Discovered Round | Why Not Blocking | Revisit Trigger |
|-------|-----------------|------------------|-----------------|
| Backend line count may remain above the historical 30k target after first pass | 0 | Does not block functional standalone rv64ima support if ACs pass | Revisit after YSX-only and combined builds are green |

### Completed and Verified
<!-- Only move tasks here after Codex verification -->
| AC | Task | Completed Round | Verified Round | Evidence |
|----|------|-----------------|----------------|----------|
| AC-1 | task1: Patch Humanize hook and generate tracked draft/annotated/refined plan docs | 0 | pending verification | `docs/draft.md`, `docs/plan.annotated.md`, `docs/plan.md`, `docs/.humanize/plan_qa/plan.qa.md`, commit `ecb3eee65f06` |
| AC-1 | task2: Copy RISCV to YSX and bulk-rename backend-visible symbols/files | 0 | 0 | `llvm/lib/Target/YuShuXin/`, `llvm/lib/Target/CMakeLists.txt`, `llvm/CMakeLists.txt`; YSX-only and RISCV+YSX builds both succeed |
| AC-2, AC-3 | task3: Remove GISel and prune YSX to rv64ima-only lowering/instruction coverage | 0 | 0 | GISel and non-rv64ima files removed from `llvm/lib/Target/YuShuXin/`; YSX directory reduced to ~109k LOC; unsupported features rejected by YSX tests |
| AC-1, AC-2 | task4: Add LLVM/Clang `ysx64` plumbing and unique YSX option names | 0 | 0 | `llvm/include/llvm/TargetParser/Triple.h`, `llvm/lib/TargetParser/Triple.cpp`, `clang/lib/Basic/Targets.cpp`, `clang/lib/Driver/ToolChains/Clang.cpp`; combined static build + smoke tests for both targets pass |
| AC-4 | task5: Create YSX-owned LLVM and Clang tests from rv64ima-applicable RISCV subsets | 0 | 0 | `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`, `clang/test/Driver/YSX`; 129-test YSX suite passes |
| AC-1, AC-2, AC-4 | task6: Configure fresh builds, validate YSX-only and RISCV+YSX, run targeted tests | 0 | 0 | `/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm` and `/home/zhaosiying/codebase/compiler/build_ysx_riscv_host_llvm`; full YSX suite pass plus dual-target smoke coverage |

### Explicitly Deferred
<!-- Items here require strong justification -->
| Task | Original AC | Deferred Since | Justification | When to Reconsider |
|------|-------------|----------------|---------------|-------------------|
