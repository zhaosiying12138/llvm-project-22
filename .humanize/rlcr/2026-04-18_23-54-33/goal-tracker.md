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

### Plan Version: 15 (Updated: Round 7 implementation)

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
| 2 review | Rejected the Round-2 completion claim and reopened task3/task6 | The exact blocker-string scan was insufficient: YSX still carries renamed FP/C/V/bitmanip/crypto/vendor/supervisor feature definitions, FP/vector register classes, compressed/vector format includes, GISel-only TableGen artifacts, vector/FP/vendor lowering and frame helpers, and MC still accepts FP/vector CSRs such as `fflags`, `fcsr`, `frm`, `vtype`, `vl`, `vlenb`, `vxsat`, and `vxrm` | AC-2 and AC-3 remain unmet; AC-4 needs focused negative tests after the real pruning and CSR fix |
| 3 | Implemented the first real Round-3 source pruning slice | Removed user-visible FP/vector CSR aliases and added negative tests; deleted C/V instruction-format includes and `.insn 16` support; pruned FP/vector register classes from `YSXRegisterInfo.td`; simplified calling convention, CSR insertion, selected MC/parser/disassembler/register/lowering paths to GPR-only behavior; YSX-only and RISCV+YSX static builds plus the 130-test YSX lit subset pass | AC-2 advanced for CSR leakage and C `.insn`; AC-3 advanced but remains open because `YSXFeatures.td`, frame lowering, instr-info, subtarget, and other copied surfaces still need further deletion |
| 3 review | Rejected the Round-3 completion claim and kept task3/task6 active | The specific FP/vector CSR aliases now reject, but default MC still accepts non-`rv64ima` CSR/privileged/Zifencei surfaces such as `csrr mstatus`, `sfence.vma`, `hfence.vvma`, `mret`, `sret`, and `fence.i`; the backend also still carries the copied unsupported feature universe, vector/FP lowering and frame helpers, stale pass declarations, and generated target-feature exposure | AC-2 and AC-3 remain unmet; AC-4 needs expanded negative coverage for the remaining externally visible surfaces |
| 4 | Implemented the Round-4 MC/feature-surface pruning slice | Replaced `YSXFeatures.td` with a small rv64ima-oriented feature file, removed symbolic CSR records, removed default MC visibility for `fence.i`, CSR/counter aliases, privileged/debug/hypervisor mnemonics, and added negative MC tests for the reviewed examples | AC-2 advanced for MC; AC-3 advanced for feature records but remains open because large unsupported lowering/selection/metadata/pass surfaces are still present |
| 4 review | Rejected the Round-4 completion claim and kept task3/task6 active | `clang --target=ysx64-unknown-elf -c` now reaches the YSX backend with many unsupported negative RISCV feature flags and fatally rejects its own default invocation; `clang --target=ysx64-unknown-elf -march=rv64ima -c` fails the same way. Source review also finds retained vector/FP/vendor/RV32/compressed support in `YSXISelLowering.cpp`, `YSXISelDAGToDAG.cpp`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`, frame/subtarget helpers, and pass declarations. | AC-2 and AC-4 are blocked by the Clang compile regression; AC-3 remains unmet |
| 5 | Fixed the supported Clang compile path and added real compile coverage | YSX feature filtering now ignores inherited disabled RISCV features before subtarget parsing while still rejecting enabled unsupported features; `clang/test/Driver/YSX/target-options.c` now compiles default `ysx64-unknown-elf` and explicit `-march=rv64ima`; focused YSX lit and RISCV+YSX build validation pass | AC-2 and AC-4 compile-path blocker resolved; AC-3 remains active for source-surface pruning |
| 5 review | Verified the Round-5 compile-path fix but rejected completion | Default and explicit `rv64ima` Clang compiles now produce ELF64 RISC-V relocatable objects in the available YSX-only and combined builds, and enabled `rv64imaf`/`+v` still reject. However, Round 5 deliberately queued AC-3 pruning, Clang still routes `ysx64` through `RISCV64TargetInfo`, `-###` still exposes the RISCV disabled feature universe, and predefines still expose `__riscv_v_intrinsic`. | AC-2 and AC-4 advanced for the compile path; AC-2/AC-3 remain blocked by residual visible/source unsupported surfaces |
| 6 | Implemented a YSX-specific default Clang/TargetParser frontend surface | `ysx64` now routes to `YSX64TargetInfo`, driver feature emission no longer calls RISCV all-extension expansion for YSX, `YSXISAInfo` is no longer a `RISCVISAInfo` alias, `-###` only emits rv64ima positive features plus relax, and YSX tests cover absence of RISCV vector macros/types/builtins | AC-2 advanced for the default driver path; AC-3 advanced for TargetParser/Clang plumbing but remains blocked by inherited target-feature hooks and backend source pruning |
| 6 review | Rejected the full Round-6 completion claim while verifying the default-driver fix | Default `ysx64` driver output and RVV macro/type/builtin probes are fixed, but `YSX64TargetInfo` still inherits RISCV target-attribute and explicit cc1 target-feature parsing. `target("arch=+v")`, `target("arch=rv64imaf")`, and `-Xclang -target-feature +v` can still expose unsupported RISCV vector/FP feature state before the backend rejects or crashes. Backend AC-3 pruning also remains deliberately out of scope. | AC-2 remains partially blocked by inherited Clang feature-surface leaks; AC-3 remains unmet |
| 7 | Implemented YSX-specific Clang target-feature hardening | `YSX64TargetInfo` now owns target-feature map initialization, target-feature handling, target attribute parsing, feature-name validation, and FP/vector asm-constraint rejection. Tests cover `target("arch=+v")`, `target("arch=rv64imaf")`, explicit cc1 `+v`, and `"f"`/`"vr"` asm constraints. | AC-2 and AC-4 advanced for inherited Clang frontend feature paths; AC-3 remains unmet because backend source pruning is still queued for the next source-deletion slice |

#### Active Tasks
<!-- Mainline tasks only: each task must directly advance the current round objective and carry routing metadata -->
| Task | Target AC | Status | Tag | Owner | Notes |
|------|-----------|--------|-----|-------|-------|
| task3: Finish pruning YSX to the actual `rv64ima` source surface | AC-2, AC-3 | active after Round-7 implementation | coding | Claude | Round 7 locally fixed the inherited Clang target-attribute, explicit cc1 target-feature, and FP/vector asm-constraint leaks. Retained vector/FP/vendor/RV32/compressed lowering, selection, metadata, pass declarations, false-return compatibility helpers, and backend vector helpers still need deletion or replacement with YSX-only rv64ima code. |
| task6: Revalidate after source pruning and Clang/MC surface fixes | AC-1, AC-2, AC-4 | active after Round-7 implementation | coding | Claude | Round 7 validated the YSX-only lit subset, YSX-only smoke compiles, combined RISCV+YSX static build, combined smoke compiles, unsupported frontend feature rejection, RISCV vector macro preservation, and RISCV backend zero-diff; keep rerunning after remaining backend pruning slices. |

### Blocking Side Issues
<!-- Only issues that directly block current mainline progress belong here -->
| Issue | Discovered Round | Blocking AC | Resolution Path |
|-------|-----------------|-------------|-----------------|
| Removed feature source remains renamed/disabled instead of deleted | 2 review | AC-3 | Continue replacing register/calling-convention leftovers, instruction-format metadata, lowering, frame, subtarget, MC, selection support, pass declarations, and TargetParser/Clang integration with a minimal `rv64ima`-only surface; resolve generated-code breakage by deleting unsupported callers rather than adding compatibility stubs. |
| Residual copied frame/instr/subtarget vector helpers remain after first Round-3 slice | 3 | AC-3 | Continue replacing YSX vector/scalable frame, instruction-combiner, feature, and subtarget helpers with rv64ima-only implementations; keep rebuilding after each deletion slice. |

### Queued Side Issues
<!-- Non-blocking issues stay queued and must NOT replace the round objective -->
| Issue | Discovered Round | Why Not Blocking | Revisit Trigger |
|-------|-----------------|------------------|-----------------|
| Goal Tracker immutable AC list dropped AC-3 and AC-4 from `docs/plan.md` | 0 | The mutable tracker and round summaries now explicitly track and verify AC-3/AC-4 against `docs/plan.md`; the immutable section is intentionally not edited by tracker rules. | Revisit only if a future RLCR tool requires regenerating the immutable tracker section. |
| YSX CodeGen tests retain large inactive copied check blocks for unsupported RISCV variants | 2 review | Active RUN lines scanned during review did not invoke unsupported YSX variants outside negative tests, so this is secondary to source pruning and MC rejection. | Revisit after task3/task6 are fixed, then trim or regenerate stale RV32/ZBB/XTHEAD/RV64IA-TSO check-prefix blocks that no longer describe supported YSX behavior. |

### Completed and Verified
<!-- Only move tasks here after Codex verification -->
| AC | Task | Completed Round | Verified Round | Evidence |
|----|------|-----------------|----------------|----------|
| AC-1 | task1: Patch Humanize hook and generate tracked draft/annotated/refined plan docs | 0 | pending verification | `docs/draft.md`, `docs/plan.annotated.md`, `docs/plan.md`, `docs/.humanize/plan_qa/plan.qa.md`, commit `ecb3eee65f06` |
| AC-1 | task2: Copy RISCV to YSX and bulk-rename backend-visible symbols/files | 0 | 0 | `llvm/lib/Target/YuShuXin/`, `llvm/lib/Target/CMakeLists.txt`, `llvm/CMakeLists.txt`; YSX-only and RISCV+YSX builds both succeed |
| AC-1, AC-2 | task4: Add LLVM/Clang `ysx64` plumbing and unique YSX option names | 0 | 0 | `llvm/include/llvm/TargetParser/Triple.h`, `llvm/lib/TargetParser/Triple.cpp`, `clang/lib/Basic/Targets.cpp`, `clang/lib/Driver/ToolChains/Clang.cpp`; combined static build + smoke tests for both targets pass |
| AC-4 | task5: Create YSX-owned LLVM and Clang tests from rv64ima-applicable RISCV subsets | 0 | 0 | `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`, `clang/test/Driver/YSX`; 129-test YSX suite passes |
| AC-2, AC-4 | Round-4 MC CSR/privileged/Zifencei visible-surface fix | 4 | 4 | Manual `llvm-mc -triple=ysx64-unknown-elf` checks now reject `fence.i`, `csrr`, counter aliases, `mret`/`sret`/`wfi`/`dret`, `sfence.vma`, and `hfence.vvma`; `llvm/test/MC/YSX/unsupported-features.s` contains negative coverage for these examples. |
| AC-2, AC-4 | Round-5 Clang compile-path fix | 5 | 5 review | `clang --target=ysx64-unknown-elf -c` and `clang --target=ysx64-unknown-elf -march=rv64ima -c` both produce ELF64 RISC-V relocatable objects in available YSX-only and combined builds; enabled unsupported `rv64imaf`/`rv64imac` and backend `+f`/`+v` remain rejected. Review could not rerun lit because the external build test root is read-only in this sandbox. |
| AC-2, AC-4 | Round-6 default-driver and RVV frontend probe fix | 6 | 6 review | `clang --target=ysx64-unknown-elf -### -c` now emits only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax`; YSX predefines no longer include `__riscv_v_intrinsic` or `__riscv_vector`; RVV builtin/type probes fail. Review rejected the broader frontend-surface completion claim because inherited target attributes and explicit cc1 target features still leak unsupported RISCV features. |
| AC-2, AC-4 | Round-7 inherited Clang target-feature hardening | 7 | pending review | `target("arch=+v")` and `target("arch=rv64imaf")` now fail before IR emission; explicit cc1 `-target-feature +v` fails without defining RVV macros; `"f"` and `"vr"` asm constraints are rejected; focused YSX lit, YSX-only smoke compiles, combined RISCV+YSX static build, combined smoke compiles, RISCV vector macro smoke, and RISCV backend zero-diff all pass locally. |

### Explicitly Deferred
<!-- Items here require strong justification -->
| Task | Original AC | Deferred Since | Justification | When to Reconsider |
|------|-------------|----------------|---------------|-------------------|
