# YSX rv64ima Backend Extraction

## Goal Description

Create a new standalone `YSX` backend in `llvmorg-22.1.3` by copying the
RISC-V backend into `llvm/lib/Target/YuShuXin`, renaming backend-visible
symbols and files from `RISCV` to `YSX`, and pruning the implementation to a
minimal `rv64ima`-only target exposed as `ysx64`.

The implementation must preserve the existing RISCV backend unchanged, allow
YSX to build without RISCV, and allow RISCV plus YSX to co-build with static
libraries and no collisions in symbols, options, or target registration.

## Acceptance Criteria

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
    - YSX does not expose GlobalISel-only functionality after GISel removal.
- AC-3: YSX implementation is materially smaller than RISCV and contains no
  support code for removed features.
  - Positive Tests (expected to PASS):
    - The copied backend removes GISel, RV32, RVV, FP, C, and non-IMA
      feature/scheduling/profile code.
    - DAG lowering, DAG-to-DAG selection, custom ISD nodes, pseudos, and
      scheduling data are reduced to the retained `rv64ima` subset.
  - Negative Tests (expected to FAIL):
    - YSX build or tests must fail if removed instruction definitions or custom
      ISD nodes are still referenced.
    - YSX test coverage must not rely on RISCV-only test directories or
      modify existing RISCV tests.
- AC-4: YSX carries its own LLVM and Clang test coverage derived from the
  `rv64ima`-applicable RISCV subset.
  - Positive Tests (expected to PASS):
    - `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`,
      and `clang/test/Driver/YSX` cover the retained target surface.
    - RUN lines and target spellings are rewritten to YSX-owned names.
  - Negative Tests (expected to FAIL):
    - Any YSX test copied from RISCV but still requiring removed ISA features
      must be excluded or rewritten.
    - Existing RISCV tests and sources must remain unchanged.

## Path Boundaries

Path boundaries define the acceptable range of implementation quality and
choices.

### Upper Bound (Maximum Acceptable Scope)

The implementation introduces a complete standalone `YSX` backend with
independent LLVM and Clang plumbing for `ysx64`, a minimal `rv64ima` ISA
surface, independent MC/asm/disassembly/codegen support, YSX-owned test
coverage across LLVM and Clang, and successful YSX-only plus RISCV+YSX static
builds and targeted test runs.

### Lower Bound (Minimum Acceptable Scope)

The implementation introduces a buildable and testable `YSX` backend for
`ysx64` that supports only `rv64ima`, removes GISel and other excluded
features, has no dependency on RISCV target libraries, and passes a focused set
of LLVM and Clang YSX regression tests in both YSX-only and combined
RISCV+YSX builds.

### Allowed Choices

- Can use: bulk file copy/rename operations, explicit target-plumbing changes in
  LLVM and Clang, minimal placeholder/stub code where layout preservation would
  otherwise keep dead references, explicit rejection of unsupported ISA
  surfaces, fresh out-of-tree build directories, and environment-gated Humanize
  hook configuration.
- Cannot use: edits under `llvm/lib/Target/RISCV` or existing RISCV test
  directories, any runtime dependency from YSX on RISCV target libraries, a
  custom ELF machine type in v1, or preservation of unsupported features merely
  to reduce implementation effort.

## Feasibility Hints and Suggestions

> **Note**: This section is for reference and understanding only. These are
> conceptual suggestions, not prescriptive requirements.

### Conceptual Approach

1. Copy `llvm/lib/Target/RISCV` to `llvm/lib/Target/YuShuXin`.
2. Rename backend symbols, tablegen records, generated includes, and
   command-line flags from `RISCV` to `YSX`.
3. Remove GISel and reduce the backend to the `rv64ima` subset by pruning
   feature parsing, scheduling data, instruction definitions, custom ISD nodes,
   and lowering code until YSX is self-contained.
4. Add new `ysx64` target parsing and Clang target/driver plumbing while
   keeping object semantics RISC-V ELF compatible.
5. Copy only the `rv64ima`-applicable RISCV test subset into YSX-owned LLVM and
   Clang test directories and rewrite RUN lines accordingly.
6. Build and test in a fresh Ninja+ccache+clang build directory for both YSX
   alone and RISCV+YSX together.

### Relevant References

- `llvm/lib/Target/RISCV` - source backend to copy and prune
- `llvm/include/llvm/TargetParser/Triple.h` - target arch spelling and helpers
- `llvm/lib/TargetParser/Triple.cpp` - triple parsing and normalization
- `clang/lib/Basic/Targets.cpp` - target info selection
- `clang/lib/Driver/ToolChains/Clang.cpp` - backend/driver flag routing
- `clang/lib/Driver/Driver.cpp` - architecture selection and toolchain paths

CMT:
research_request
There is no repository-local `build_host_llvm` script or preset in this tree.
The plan should explicitly use a fresh external build directory with explicit
CMake flags instead of assuming a repo helper exists.
ENDCMT

CMT:
change_request
The Humanize stop hook hardcodes a 2000-line limit. The implementation should
disable this through an environment-gated hook change, and the build/test
commands used for RLCR should export that environment override.
ENDCMT

## Dependencies and Sequence

### Milestones

1. Workflow setup: patch the Humanize hook, create `docs/draft.md`, produce the
   annotated plan and refined plan, and initialize RLCR assets.
   - Phase A: generate the draft and plan documents
   - Phase B: patch the hook and prepare RLCR execution
2. Backend extraction: copy RISCV to YSX, rename backend-visible identifiers,
   and remove unsupported subsystems until YSX is self-contained.
   - Phase A: copy/rename filesystem layout and target entrypoints
   - Phase B: prune instruction definitions, features, DAG lowering, and MC
3. LLVM/Clang integration: add `ysx64` plumbing to LLVM target parsing and
   Clang target/driver support.
   - Phase A: add LLVM target identity and build-system integration
   - Phase B: add Clang target info and minimal driver/codegen surface
4. Validation: migrate YSX tests, build YSX-only and RISCV+YSX variants, then
   run focused regression suites and fix remaining issues.
   - Phase A: create YSX-owned LLVM and Clang tests
   - Phase B: build, test, and iterate on failures

## Task Breakdown

Each task must include exactly one routing tag:
- `coding`: implemented by Claude
- `analyze`: executed via Codex (`/humanize:ask-codex`)

| Task ID | Description | Target AC | Tag (`coding`/`analyze`) | Depends On |
|---------|-------------|-----------|----------------------------|------------|
| task1 | Patch the Humanize stop hook, create `docs/draft.md`, and produce `docs/plan.md` plus QA artifacts | AC-1 | coding | - |
| task2 | Copy the RISCV backend to `llvm/lib/Target/YuShuXin` and bulk-rename backend-visible `RISCV` identifiers and files to `YSX` | AC-1 | coding | task1 |
| task3 | Remove GISel and prune the backend to a self-contained `rv64ima` subset, including features, TD files, ISel lowering, DAG-to-DAG selection, custom ISD nodes, and schedulers | AC-2, AC-3 | coding | task2 |
| task4 | Add LLVM and Clang target plumbing for `ysx64` with a minimal accepted ISA/ABI surface and no RISCV symbol or option conflicts | AC-1, AC-2 | coding | task3 |
| task5 | Create YSX-owned LLVM and Clang tests by copying the `rv64ima`-applicable RISCV subset and rewriting target names/options | AC-4 | coding | task4 |
| task6 | Configure fresh Ninja+ccache+clang build directories, validate YSX-only and combined RISCV+YSX builds, run targeted tests, and fix failures | AC-1, AC-2, AC-4 | coding | task5 |

## Claude-Codex Deliberation

### Agreements

- `ysx64` is the public target identity for v1.
- YSX remains RISC-V ELF/object compatible in v1.
- GlobalISel should be removed to keep the backend minimal.
- The implementation must not modify RISCV sources or tests.

### Resolved Disagreements

- Layout preservation: A literal file-for-file mirror of RISCV is unnecessary.
  The chosen direction keeps only functionally relevant YSX files and
  directories because strict mirroring would preserve dead code and conflict
  with the minimization goal.

### Convergence Status

- Final Status: `converged`

## Pending User Decisions

- None. The required target identity, test scope, object-identity policy,
  ISA/ABI surface, GISel strategy, and layout direction have already been
  fixed for this implementation.

## Implementation Notes

### Code Style Requirements

- Implementation code and comments must NOT contain plan-specific terminology
  such as `AC-`, `Milestone`, `Step`, or `Phase`.
- Use backend-domain naming in code and tests.
- Keep hidden option names, pass names, and public target spellings unique to
  YSX to avoid collisions in combined static builds.
