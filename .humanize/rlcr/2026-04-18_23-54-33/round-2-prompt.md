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
- Write the current round contract to @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-2-contract.md

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
# Round 1 Review

Mainline Progress Verdict: ADVANCED

Goal Alignment Summary
`ACs: 2/4 addressed | Forgotten items: 0 | Unjustified deferrals: 1`

Round 1 made substantial mainline progress by deleting many unsupported TD and pass files, but the work is not complete. The summary's "Remaining Items" defers generated-reference compatibility stubs and dead helper code as a follow-up; that deferral is not justified because the original plan and the round contract require the YSX source itself to stop carrying removed-feature support.

## Mainline Gaps

1. AC-2 is still violated: incremental assembler target parsing accepts unsupported extensions. The new negative test only checks full `.option arch, rv64gc`, but the actual `.option arch, +...` path still enables removed feature bits without running the YSX `rv64ima` validation. I confirmed these commands succeed instead of reporting an error:
   - `printf '.option arch, +f\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -`
   - `printf '.option arch, +c\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -`
   - `printf '.option arch, +zbb\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -`
   - `printf '.option arch, +v\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -`
   - `printf '.option rvc\n' | ../build_ysx_only_host_llvm/bin/llvm-mc -triple=ysx64-unknown-elf -`

   The bug is in `llvm/lib/Target/YuShuXin/AsmParser/YSXAsmParser.cpp:2952-2968`: the `+extension` branch calls `setFeatureBits()` and only checks `YSXFeatures::parseFeatureBits()`, not `YSXFeatures::isValidYSXISAInfo()` or the same rejection used by full arch strings at `YSXAsmParser.cpp:2846-2848`. The legacy `.option rvc` path at `YSXAsmParser.cpp:3014-3020` directly sets `FeatureStdExtC`. This conflicts with `docs/plan.md:31-40` and the round contract's negative-coverage requirement at `round-1-contract.md:39-41`.

2. AC-3 is still not complete: unsupported feature definitions remain in TableGen. `llvm/lib/Target/YuShuXin/YSXFeatures.td` still defines large unsupported surfaces, including FP (`FeatureStdExtF/D/Q/Zfh/Zfa/Zfinx` at lines 287-382), compressed (`FeatureStdExtZca/C/Zcb/Zcd/Zcf/Zcmp/Zcmt/Zce` at lines 384-477), bitmanip/crypto (`FeatureStdExtZba/Zbb/Zbc/Zbs/Zb*` and Zk groups at lines 479-636), vector (`FeatureStdExtZvl*`, `Zve*`, `V`, `Zv*` at lines 638-915), hypervisor/supervisor/pointer masking (`FeatureStdExtH`, `S*`, `Sv*`, `Sm*` at lines 916-1106), packed SIMD (`FeatureStdExtP` at lines 1108-1120), and vendor extensions starting at line 1122. The plan explicitly requires pruning feature parsing and non-IMA feature/profile code (`docs/plan.md:41-47`, `docs/plan.md:107-109`), not preserving those definitions behind later validation.

3. AC-3 is also not complete in generated register and instruction description inputs. `llvm/lib/Target/YuShuXin/YSXRegisterInfo.td:503-522` still defines FP-in-GPR register classes, `YSXRegisterInfo.td:525-879` still defines RVV value types and vector register classes, and `YSXRegisterInfo.td:884-909` still defines FP/vector/vendor special registers and XSfmmbase tile registers. `llvm/lib/Target/YuShuXin/YSXInstrInfo.td:625-627` still includes `YSXInstrFormatsC.td` and `YSXInstrFormatsV.td`, so compressed/vector format infrastructure remains part of the YSX TableGen surface. This violates the round success criterion that TableGen sources no longer carry FP/C/V/RV32/bitmanip/vendor support code.

4. AC-3 remains incomplete in compiled C++ helper code. Examples:
   - `llvm/lib/Target/YuShuXin/MCA/CMakeLists.txt:1-16` still builds `YSXCustomBehaviour.cpp`, and `MCA/YSXCustomBehaviour.cpp:266-316` still resolves RVV pseudo information through `YSX::getVXMemOpInfo`, `getVSXPseudo`, `getVLXPseudo`, and `YSXVInversePseudosTable`.
   - `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:42-128` still preprocesses vector splats, vector FP extends, and `Intrinsic::riscv_vlse`; `YSXISelDAGToDAG.cpp:3976-4555` still contains RVV masked pseudo and vector splat selection helpers.
   - `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp:1787-1839` still selects `LD_RV32`/`SD_RV32`, and the file retains vendor-specific selection paths.
   - `llvm/lib/Target/YuShuXin/YSX.h:53-115` still declares compressed/vector/removed optimization passes such as `createYSXMakeCompressibleOptPass`, `createYSXGatherScatterLoweringPass`, `createYSXVectorPeepholePass`, `createYSXInsertVSETVLIPass`, and `createYSXVLOptimizerPass`.

5. The compatibility-stub approach is now itself a mainline gap, not a harmless follow-up. `llvm/lib/Target/YuShuXin/MCTargetDesc/YSXUnsupportedOpcodes.h:9-11` says the backend still has dead helper code inherited from RISCV, then defines more than 190 unsupported opcode constants for FP, compressed, vector, RV32, bitmanip, and vendor names. The round contract required stale references to be fixed after deletion (`round-1-contract.md:20-21`, `round-1-contract.md:34-37`). Keeping a large unsupported-opcode shim means those stale references were papered over rather than removed.

## Blocking Side Issues

No separate side issue is needed beyond the mainline blockers above. The `.option arch` acceptance bug and the retained unsupported backend source both directly block AC-2/AC-3 and must drive the next round.

## Queued Side Issues

1. The existing queued item remains valid: the immutable goal tracker section still omits AC-3 and AC-4 from `docs/plan.md`. I did not edit the immutable section, but I updated the mutable section to keep AC-3/AC-4 tracked against `docs/plan.md`.

2. Frontend/parser minimization beyond the current backend-pruning scope is still queued. `llvm/include/llvm/TargetParser/YSXISAInfo.h:12-18` aliases the RISCV ISA parser and vector type namespace. This is not the next blocker by itself, but after the backend source is actually pruned, this should be revisited if unsupported parser surface remains exposed.

## Required Implementation Plan

1. Replace `YSXFeatures.td` with a minimal YSX feature model. Keep only `Feature64Bit`, the rv64ima ISA features (`I`, `M`, `A`) and the helper extensions required by the retained lowering surface (`Zmmul`, `Zaamo`, `Zalrsc`), plus non-ISA tuning features that are actually referenced by retained rv64ima code. Remove FP, compressed, bitmanip, crypto, vector, hypervisor/supervisor, packed-SIMD, vendor, RV32-only, and unrelated Z/S/H/X feature definitions. Then fix every generated-subtarget compile error by deleting the stale caller, not by adding replacement stubs for unsupported features.

2. Prune TableGen register/instruction infrastructure to match that feature set. Remove FP-in-GPR classes, RVV value types, vector registers/classes, FP/vector/vendor CSRs and XSfmmbase tiles from `YSXRegisterInfo.td`; delete or stop including `YSXInstrFormatsC.td` and `YSXInstrFormatsV.td`; remove compressed/vector-only operand and format classes from `YSXInstrInfo.td` and `YSXInstrFormats.td` unless an rv64ima instruction still requires them.

3. Delete unsupported generated-reference compatibility instead of extending it. Remove `YSXUnsupportedOpcodes.h` and all includes of it, then remove the C++ branches that referenced those unsupported opcodes. This pass must cover `YSXISelDAGToDAG.cpp`, `YSXISelLowering.*`, `YSXInstrInfo.*`, `YSXAsmPrinter.cpp`, `YSXFrameLowering.cpp`, `YSXRegisterInfo.cpp`, `MCTargetDesc/*`, and any remaining pass declarations in `YSX.h`. If a helper exists only for FP/C/V/RV32/bitmanip/vendor behavior, delete it.

4. Remove unsupported build products and generators. Drop `YSXGenCompressInstEmitter.inc` generation if no compressed instruction support remains, remove the compression include from `YSXBaseInfo.cpp`, and remove the MCA RVV behavior library or replace it with an rv64ima-only MCA implementation. Keep `CMakeLists.txt` aligned so YSX-only builds do not compile vector/compressed/vendor support by accident.

5. Fix assembler option validation. In `YSXAsmParser.cpp`, after every `.option arch, +...` or `-...` update, reconstruct the feature-bit ISA and reject anything outside `rv64ima`; restore the old feature bits on failure. For YSX, reject `.option rvc` directly with the same unsupported-ISA diagnostic, and ensure `.option norvc` does not depend on now-removed compressed feature constants. Add MC tests for `.option arch, +f`, `+c`, `+zbb`, `+v`, `.option rvc`, and a push/pop sequence that proves a failed unsupported toggle does not leave feature bits enabled.

6. Rebuild and revalidate after the pruning is complete. Required commands are the YSX-only `ninja LLVMYSXCodeGen llvm-mc llc clang opt lld`, the RISCV+YSX combined static build with the same targets, the YSX lit subset covering `llvm/test/CodeGen/YSX`, `llvm/test/MC/YSX`, `clang/test/CodeGen/YSX`, and `clang/test/Driver/YSX`, and a RISCV untouched check. Also run `rg` checks over `llvm/lib/Target/YuShuXin` for removed surfaces (`FeatureStdExtF`, `FeatureStdExtC`, `FeatureStdExtV`, `FeatureVendor`, `RVV`, `XSf`, `XTHead`, `Xqci`, `LD_RV32`, `YSXUnsupportedOpcodes`) and treat matches as blockers unless they are in a negative test or an explicit diagnostic.

## Goal Tracker Update

I updated the mutable section of `goal-tracker.md`:

- Plan version is now `5 (Updated: Round 1 Review)`.
- Added a Round 1 review evolution entry rejecting the completion claim.
- Reopened `task3` and `task6` under Active Tasks.
- Added blocking issues for incremental assembler feature toggles and retained removed-feature source.
- Removed the Round-1 `task3`/`task6` rows from Completed and Verified.

## Validation Notes

Confirmed locally:

- `llc -mtriple=ysx64-unknown-elf -mattr=+zbb` reports `LLVM ERROR: YSX only supports the rv64ima ISA`.
- `llc -mtriple=ysx64-unknown-elf -target-abi=lp64d` reports `LLVM ERROR: YSX only supports the lp64 ABI`.
- `llvm-mc` accepts valid rv64ima assembly such as `add a0, a0, a1`.
- `llvm-mc` incorrectly accepts `.option arch, +f`, `.option arch, +c`, `.option arch, +zbb`, `.option arch, +v`, and `.option rvc`.

I did not rerun the full lit suite because the source inspection and targeted MC checks already expose blocking AC-2/AC-3 failures.
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

- Keep the mainline objective from @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-2-contract.md stable for this round
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
2. Write your work summary into @/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/.humanize/rlcr/2026-04-18_23-54-33/round-2-summary.md
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
