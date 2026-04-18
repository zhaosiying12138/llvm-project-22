# YuShuXin rv64ima Backend Extraction

## Goal

Create a new `YuShuXin` LLVM backend from `llvmorg-22.1.3` by copying the
RISC-V backend into `llvm/lib/Target/YuShuXin`, renaming backend-visible
identifiers from `RISCV` to `YSX`, and then pruning the implementation to a
minimal standalone `rv64ima`-only target.

The backend must:

- build without the RISCV backend being enabled
- coexist with the RISCV backend in the same static-library build without
  duplicate symbols, duplicate command-line options, or target registration
  conflicts
- leave the existing RISCV backend sources untouched
- expose a distinct public target identity `ysx64`
- remain RISC-V ELF/object compatible in v1

## Required Scope

- Copy `llvm/lib/Target/RISCV` to `llvm/lib/Target/YuShuXin`
- Rename files, classes, functions, tablegen records, generated include names,
  pass names, hidden command-line flags, and MC/target init entrypoints from
  `RISCV` to `YSX`
- Remove support for everything except `rv64ima`
- Remove RV32, floating-point, compressed, vector/RVV, bitmanip, crypto,
  vendor extensions, profiles, and other non-IMA features
- Remove GlobalISel entirely from YSX
- Remove unsupported custom ISD nodes and unsupported lowering logic from the
  DAG lowering and DAG-to-DAG selector
- Keep only the instructions, pseudos, scheduling data, processor data, and
  MC/asm/disassembler support needed for `rv64ima`
- Keep YSX independent from RISCV target libraries and generated headers

## Public Interface Decisions

- The target triple spelling is `ysx64`
- The only supported ISA is `rv64ima`
- The only supported ABI is `lp64`
- Unsupported `-march`, `-mcpu`, `-mattr`, or backend-specific flags must be
  rejected instead of being silently accepted
- Backend-local hidden flags that would collide with RISCV flags must be
  renamed from `riscv-*` to `ysx-*`

## Testing Requirements

- Copy all RISCV backend tests that are applicable to `rv64ima` into YSX-owned
  test directories
- Cover LLVM CodeGen tests, LLVM MC tests, and the relevant Clang CodeGen and
  Driver tests
- Rewrite target names and options for the new `ysx64` surface
- Do not modify existing RISCV tests

## Build And Validation Requirements

- Use a fresh build directory outside the worktree
- Configure with Ninja, ccache, and clang
- Enable `clang`, `lld`, and `llvm`
- Validate:
  - YSX-only build
  - combined RISCV+YSX build with static libraries
  - targeted LLVM and Clang test subsets for YSX

## Workflow Requirements

- Disable the Humanize RLCR 2000-line stop-hook limit for this task
- Produce `docs/draft.md`
- Produce an annotated generated plan, then refine it into `docs/plan.md`
- Run the Humanize RLCR workflow from `docs/plan.md`
