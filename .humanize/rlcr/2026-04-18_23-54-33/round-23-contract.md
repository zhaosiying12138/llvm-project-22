# Round 23 Contract

## Mainline Objective
Delete the remaining YuShuXin short-forward-branch/CCMOV tuning, dead scheduler predicate, and vector custom-ISD helper scaffolding for removed non-rv64ima paths.

## Target ACs
- AC-3: YSX implementation contains no support code for removed features.
- AC-4: YSX validation remains passing after the pruning slice.

## Blocking Issues
- Short-forward-branch feature predicates, scheduler resources, subtarget hooks, lowering hooks, and predicated-select helpers remain even though the CCMOV pseudo surface has been removed.
- Dead vector/FP scheduler predicates and scheduler reads remain with no rv64ima role.
- `YSXSelectionDAGInfo.h` still exposes vector custom-ISD helper bits and a vector multiply-add helper.

## Queued Out Of Scope
- Immutable goal-tracker AC-list drift remains queued because `docs/plan.md` is the review source of truth and the immutable tracker section must not be edited.
- CPU/tune target-attribute warn-and-ignore diagnostics remain queued because current probes show no unsupported feature leakage and this round is source pruning.

## Success Criteria
- The reviewed SFB/CCMOV feature, scheduler, lowering, instr-info, and vector custom-ISD helper surfaces are removed, not stubbed.
- `rg "ShortForwardBranch|short-forward-branch|SFB|PseudoCCMOV|PseudoCCSUB|VLDSX0Pred|SingleElementVecFP64SchedPred|ReadFMemBase|ReadFStoreData|HasPassthruOp|HasMaskOp|getMAccOpcode|vector multiply-add" llvm/lib/Target/YuShuXin` has no matches outside intentionally retained diagnostic text.
- YSX-only Ninja build of `LLVMYSXCodeGen llvm-mc clang llc` passes.
- Combined RISCV+YSX Ninja build of `LLVMYSXCodeGen LLVMRISCVCodeGen llvm-mc llc clang lld` passes.
- Focused YSX LLVM/Clang lit suites pass.
- Retained rv64ima smoke tests and unsupported-feature probes pass.
- An assertions-enabled YSX codegen build check for the touched files passes.
- `git diff --check` passes and `git diff -- llvm/lib/Target/RISCV | wc -l` returns `0`.

## BitLesson
- Selector result: `LESSON_IDS: <comma-separated lesson IDs or NONE>` / `RATIONALE: <one concise sentence>`, treated as no applicable lesson.
