# Round 9 Review Result

Mainline Progress Verdict: ADVANCED

Round 9 completed its narrow contract: the YSX-owned `YSXVType` forwarding
namespace is gone, the reviewed copied tune/profile/vendor `-mattr` surface now
rejects, and MC help no longer exposes those names. The full original plan is
still incomplete because AC-3 requires deleting the remaining copied
vector/FP/RV32/compressed lowering, selection, frame, register, and metadata
surfaces rather than leaving them as direct `RISCVVType` users, false-return
helpers, or `#if 0` bodies.

## Part 1: Goal Tracker Audit

| AC | Status | Evidence | Blocker | Justification |
|----|--------|----------|---------|---------------|
| AC-1 | MET | `llc -mtriple=ysx64-unknown-elf` initializes and emits YSX assembly in the YSX-only build; combined build has both `libLLVMYSXCodeGen.a` and `libLLVMRISCVCodeGen.a`; `git diff -- llvm/lib/Target/RISCV \| wc -l` remains `0`. | None found. | Not deferred. |
| AC-2 | MET for the reviewed public surface | Default Clang smoke compile produced an ELF64 RISC-V soft-float object; default `-###` emits only `+i`, `+m`, `+a`, `+zmmul`, `+zaamo`, `+zalrsc`, and `+relax`; MC rejects `+f`, `+v`, `+zbb`, `+32bit`, disabled required features, and the Round-9 tune/profile probes. `-global-isel` reports target unsupported. | Must be revalidated after each AC-3 deletion slice. | Not deferred. |
| AC-3 | PARTIAL | Round 9 removed `namespace YSXVType` and the reviewed copied tune/profile feature names. Current YSX is 59,449 lines versus RISCV's 136,068. | Direct copied vector infrastructure remains, including `RISCVVType` in `YSXRegisterInfo.h:53`, `YSXBaseInfo.h:170`, `YSXISelLowering.cpp:2640`, and `YSXISelDAGToDAG.cpp:290`; vector TSFlags remain in `YSXBaseInfo.h:72`; vector frame/CFI paths remain in `YSXFrameLowering.cpp:523` and `YSXFrameLowering.cpp:573`; many `#if 0`/false stubs remain in `YSXInstrInfo.cpp` and `YSXISelDAGToDAG.cpp`. | Not deferred; task3 remains active. |
| AC-4 | PARTIAL | YSX-owned test directories exist; `llvm/test/MC/YSX/unsupported-features.s` now covers the Round-9 negative feature probes; focused MC lit for that file passes. | Full Clang/CodeGen lit could not be rerun in this review sandbox because Clang's configured external test exec root is read-only. Stale copied CodeGen check blocks remain queued until source pruning is done. | Not deferred. |

Forgotten items detection: no new task is absent from Active, Completed, or
Deferred. The known immutable tracker drift remains: the immutable section
still omits AC-3/AC-4 from `docs/plan.md`, and the mutable tracker continues to
track them against the plan source of truth. Claude's Round-9 completion claim
for the narrow contract is verified, but it is not a full plan completion.

Deferred items audit: the `Explicitly Deferred` table is empty. There are no
approved deferrals. Items described as "later AC-3 pruning" are active
mainline work, not accepted deferrals.

```text
Acceptance Criteria: 2/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 4-6
Critical blockers: AC-3 backend source pruning; AC-4 needs final full-suite validation after pruning
```

## Part 2: Mainline Drift Audit

The Round-9 objective was clear and singular: remove the YSXVType forwarding
surface and the copied MC-visible tune/profile features found in Round 8.
Claude advanced the mainline rather than clearing unrelated side issues. The
remaining blockers are not side quests; they are the core AC-3 source-pruning
work from `docs/plan.md`.

```text
Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 0
Queued Side Issues: 4
```

Queued side issues: immutable tracker AC drift, stale YSX CodeGen check-prefix
blocks, `target("cpu=...")`/`target("tune=...")` warning policy, and dead
disassembler helper cleanup. None should displace AC-3 deletion work.

## Part 3: Implementation Review

Verified claims:

- `rg "YSXVType|namespace YSXVType"` has no matches under YSX.
- `llvm-mc -triple=ysx64 -mattr=+vxrm-pipeline-flush`,
  `+log-vrgather`, `+single-element-vec-fp64`,
  `+prefer-vsetvli-over-read-vlenb`, and `+andes45` all reject with
  `YSX only supports the rv64ima ISA`.
- `llvm-mc -triple=ysx64 -mattr=help` lists only `64bit`, `i`, `m`, `a`,
  `zmmul`, `zaamo`, `zalrsc`, `relax`, and `exact-asm`.
- Focused `llvm-lit -q llvm/test/MC/YSX/unsupported-features.s` passes.
- YSX Clang smoke compile produces an ELF64 RISC-V soft-float relocatable.
- `git diff --check` is clean and `git diff -- llvm/lib/Target/RISCV | wc -l`
  is `0`.

Validation limitation: I could not rerun the full mixed LLVM/Clang lit command
because this sandbox cannot write to the external build tree
`build_ysx_only_host_llvm/tools/clang/test`.

Remaining implementation gaps:

1. AC-3 still has direct vector helper dependencies. The forwarding shim was
   removed, but YSX still calls `RISCVVType` directly in register metadata,
   MC base info, lowering, and DAG selection. This is copied vector support
   retained under another name.
2. AC-3 still has generated vector metadata and operand surfaces. Examples
   include VLMUL, SEW, VL, policy, VXRM, overlap, and segment-pseudo structures
   in `YSXBaseInfo.h`, plus VTYPE/SEW/VL operand cases in `YSXInstrInfo.cpp`.
3. AC-3 still has disabled bodies instead of deletion. Representative examples
   are the `#if 0` VSETVLI block in `YSXISelDAGToDAG.cpp:465`, vector
   reassociation bodies in `YSXInstrInfo.cpp:1661`, and scalable-vector frame
   helpers in `YSXFrameLowering.cpp:573`.
4. AC-4 cannot be final until AC-3 settles. The YSX tests exist and Round-9 MC
   coverage is good, but final coverage needs to match the actually pruned
   backend and avoid stale copied RISCV check blocks.

## Part 4: Goal Tracker Update

I updated the mutable tracker to Plan Version 20. The Round-9 completed item is
now verified by review, and the plan evolution log records that the narrow
Round-9 contract passed while AC-3 remains open. I did not modify the immutable
section.

## Part 5: Progress Stagnation Check

Development is not stagnating. AC-3 has recurred across reviews, but recent
rounds have removed real surfaces: exact parser broadness in Round 8, the
`YSXVType` forwarding layer in Round 9, and the copied MC tune/profile leak in
Round 9. The work is still large, but the last two rounds made measurable
mainline progress and did not repeat the same unfixed public leak.

## Action Items

Mainline Gaps:

1. Delete direct `RISCVVType` use from YSX by removing the vector lowering,
   DAG selection, register-class, MC metadata, and pseudo-table callers instead
   of adding a new YSX compatibility shim.
2. Prune `YSXBaseInfo.h`, `YSXInstrInfo.cpp`, `YSXInstrFormats.td`,
   `YSXRegisterInfo.*`, `YSXFrameLowering.cpp`, `YSXISelLowering.*`, and
   `YSXISelDAGToDAG.*` to scalar rv64ima-only code.
3. After each deletion slice, rebuild YSX-only and RISCV+YSX, run focused YSX
   LLVM/Clang tests, smoke compile both targets, and keep RISCV source diff at
   zero.

Blocking Side Issues:

None outside the mainline AC-3 blockers.

Queued Side Issues:

1. Trim stale copied YSX CodeGen check-prefix blocks after source pruning.
2. Decide whether unsupported `target("cpu=...")` and `target("tune=...")`
   should hard-error instead of warn-and-ignore.
3. Remove dead disassembler helper warnings alongside TableGen decoder cleanup.
4. Keep noting immutable tracker AC drift, but continue using `docs/plan.md` as
   the source of truth.

REQUIRES MORE WORK
