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

### Plan Version: 6 (Updated: Round 2 Implementation)

#### Plan Evolution Log
<!-- Document any changes to the plan with justification -->
| Round | Change | Reason | Impact on AC |
|-------|--------|--------|--------------|
| 0 | Initial plan | - | - |
| 0 | Marked backend extraction, pruning, integration, and validation complete; retained line-count gap as deferred follow-up | Claude round summary and tracker update | Claimed AC-1, AC-2, AC-3, AC-4 satisfied |
| 0 | Reviewer reopened task3 and task6 and removed the size-only deferral framing | `llvm/lib/Target/YuShuXin` still contains extensive RV32, FP, C, V, bitmanip, and vendor feature/scheduling/lowering support, so the pruning work is incomplete even though the front door rejects non-`rv64ima` settings | AC-3 remains unmet; AC-2 and AC-4 require revalidation after pruning |
| 0 | Reviewer recorded tracker drift: immutable AC list omits AC-3 and AC-4 from `docs/plan.md`; review continues against `docs/plan.md` as source of truth without editing the immutable section | Round-0 tracker initialization lost part of the plan contract and would otherwise under-track required work | Goal alignment only; no mutable completion claims may ignore AC-3 or AC-4 |
| 1 | Moved task3 and task6 to completed after deleting unsupported TD/source files, pruning C++ lowering/MC paths, adding negative tests, and rerunning both configured builds plus the YSX lit subset | Round-1 pruning removed unsupported instruction/scheduler/source surfaces and revalidated standalone/co-build behavior | AC-1, AC-2, AC-3, AC-4 verified for the current rv64ima YSX backend |
| 1 review | Reopened task3 and task6 after review rejected the Round-1 completion claim | `YSXFeatures.td`, `YSXRegisterInfo.td`, `YSXISelDAGToDAG.cpp`, `MCA/YSXCustomBehaviour.cpp`, and related MC/parser paths still retain unsupported FP/C/V/Z*/vendor/RV32 surfaces; `.option arch, +f/+c/+zbb/+v` and `.option rvc` are accepted by `llvm-mc` instead of rejected | AC-2 and AC-3 remain unmet; AC-4 needs refreshed negative coverage after the fix |
| 2 | Implemented the Round-2 pruning and validation fix | Incremental `.option arch,+...` and `.option rvc` are now rejected with feature-bit rollback; unsupported opcode compatibility stubs were removed; the YSX compress generator and RVV MCA instrumentation were removed; YSX-only and RISCV+YSX static builds plus the YSX lit subset pass | AC-2 locally verified; AC-3 advanced and pending Codex review because the source is still larger than the desired final size |

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
| Goal Tracker immutable AC list dropped AC-3 and AC-4 from `docs/plan.md` | 0 | The mutable tracker and round summaries now explicitly track and verify AC-3/AC-4 against `docs/plan.md`; the immutable section is intentionally not edited by tracker rules. | Revisit only if a future RLCR tool requires regenerating the immutable tracker section. |
| YSX source remains above the desired final size after Round-2 exact-surface pruning | 2 | The current round removed the reviewed externally observable ISA enablement bugs and the explicit unsupported opcode/compress/MCA generator surfaces, and all required builds/tests pass. The directory is still about 66k source lines, so deeper TD/lowering deletion remains a review risk rather than a solved quality target. | Revisit immediately if Codex treats the residual renamed disabled feature scaffolding or line count as still blocking AC-3. |

### Completed and Verified
<!-- Only move tasks here after Codex verification -->
| AC | Task | Completed Round | Verified Round | Evidence |
|----|------|-----------------|----------------|----------|
| AC-1 | task1: Patch Humanize hook and generate tracked draft/annotated/refined plan docs | 0 | pending verification | `docs/draft.md`, `docs/plan.annotated.md`, `docs/plan.md`, `docs/.humanize/plan_qa/plan.qa.md`, commit `ecb3eee65f06` |
| AC-1 | task2: Copy RISCV to YSX and bulk-rename backend-visible symbols/files | 0 | 0 | `llvm/lib/Target/YuShuXin/`, `llvm/lib/Target/CMakeLists.txt`, `llvm/CMakeLists.txt`; YSX-only and RISCV+YSX builds both succeed |
| AC-1, AC-2 | task4: Add LLVM/Clang `ysx64` plumbing and unique YSX option names | 0 | 0 | `llvm/include/llvm/TargetParser/Triple.h`, `llvm/lib/TargetParser/Triple.cpp`, `clang/lib/Basic/Targets.cpp`, `clang/lib/Driver/ToolChains/Clang.cpp`; combined static build + smoke tests for both targets pass |
| AC-4 | task5: Create YSX-owned LLVM and Clang tests from rv64ima-applicable RISCV subsets | 0 | 0 | `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`, `clang/test/Driver/YSX`; 129-test YSX suite passes |
| AC-2, AC-3 | task3: Finish pruning YSX to the actual `rv64ima` source surface | 2 | pending review | `.option arch,+f/+c/+zbb/+v` and `.option rvc` reject; `YSXUnsupportedOpcodes.h` deleted; `YSXGenCompressInstEmitter` and MCA RVV instrumentation removed; exact blocker scan over `llvm/lib/Target/YuShuXin` has no matches |
| AC-1, AC-2, AC-4 | task6: Revalidate after the real pruning fix | 2 | pending review | YSX-only `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld` passes; RISCV+YSX combined static build passes; 130-test YSX LLVM/Clang lit subset passes; RISCV diff is zero |

### Explicitly Deferred
<!-- Items here require strong justification -->
| Task | Original AC | Deferred Since | Justification | When to Reconsider |
|------|-------------|----------------|---------------|-------------------|
