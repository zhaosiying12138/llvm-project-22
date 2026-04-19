# Round 19 Goal Alignment Review

Mainline Progress Verdict: ADVANCED

Round 19 advanced the mainline cleanup: the Round 18 stale `PseudoFloatLoad`,
FP mask, GlobalISel include/CSE hook, stale vector-stack comment, and inactive
unsupported YSX check-prefix blocks are gone. The visible target surface is now
much tighter: the YSX-only `llc -mtriple=ysx64-unknown-elf` smoke emits
`.attribute 5, "rv64ima"`, `clang --target=ysx64-unknown-elf` emits an ELF64
RISC-V soft-float object, unsupported `+v`, vendor `+xventanacondops`,
`rv64imaf`, and `.option arch, rv64ima_zbb` reject, numeric CSR/Zifencei/
privileged probes reject, `-mattr=help` lists only the curated rv64ima-related
features, `git diff -- llvm/lib/Target/RISCV | wc -l` is `0`, and
`git diff --check` is clean.

The original `docs/plan.md` goal is still not complete. AC-3 requires no support
code for removed features, and YSX still carries dead copied removed-feature
scaffolding even though front-door probes reject the corresponding inputs.

## Acceptance Criteria Status

| AC | Status | Evidence / Blocker |
|----|--------|--------------------|
| AC-1 | MET | YSX-only built binaries initialize `llc -mtriple=ysx64-unknown-elf`; smoke codegen succeeds and RISCV source diff is `0`. Claude's combined static-build claim could not be rerun in this sandbox because the external build dir is read-only, but existing combined smoke evidence has not regressed. |
| AC-2 | MET | Default Clang emits only `+i,+m,+a,+zmmul,+zaamo,+zalrsc,+relax` and `lp64`; unsupported RV32/FP/vector/vendor/compressed/CSR/Zifencei probes reject deterministically. |
| AC-3 | PARTIAL | Major pruning is real, but residual dead removed-feature source remains: `MCTargetDesc/YSXBaseInfo.h:127` and `YSXInstrInfo.cpp:1313` keep Zibi operand metadata; `YSXBaseInfo.h:144` through `YSXBaseInfo.h:149` and `YSXInstrInfo.cpp:1352` through `YSXInstrInfo.cpp:1361` keep VTYPE/RVKR metadata; `AsmParser/YSXAsmParser.cpp:652` keeps a compressed `CLUI` matcher; `YSXInsertReadWriteCSR.cpp:15` through `YSXInsertReadWriteCSR.cpp:45` plus `YSXTargetMachine.cpp:96` and `YSXTargetMachine.cpp:380` keep a no-op CSR pass; `YSXMachineFunctionInfo.h:111` through `YSXMachineFunctionInfo.h:134` and `YSXFrameLowering.cpp:170` through `YSXFrameLowering.cpp:200` keep copied SiFive CLIC interrupt state/stubs despite `YSXISelLowering.cpp:2280` rejecting interrupt handlers. |
| AC-4 | PARTIAL | YSX-owned tests exist and the reviewed stale inactive unsupported prefixes are gone; targeted MC/CodeGen negative tests passed. Full focused Clang lit could not be rerun because `clang/test/lit.cfg.py` needs to write under the read-only external build test root. Full completion still depends on AC-3 cleanup plus final full revalidation. |

## Forgotten Items Detection

No original plan task is missing from Active/Completed/Deferred tracking after
the tracker update. The tracker did drift before this review: Round 19 treated
the reviewed cleanup as review-pending completion but did not track the newly
found dead operand/CSR/SiFive scaffolding. I updated the mutable section to keep
task3/task6 active and add the current AC-3 blocker.

No task should be considered fully complete solely from the Round 19 summary:
the reported build/lit results are implementation evidence, but this review
could not rerun Ninja or full Clang lit because both configured external build
directories are read-only in the review sandbox.

## Deferred Items Audit

There are no rows in Explicitly Deferred. The CPU/tune target-attribute
warn-and-ignore policy remains a queued side issue, not a deferral. That remains
valid for now because a manual `target("cpu=sifive-p670")` probe produced a
warning, ignored the attribute, and emitted only generic-rv64 rv64ima features.

## Goal Completion Summary

Acceptance Criteria: 2/4 met (0 deferred)
Active Tasks: 2 remaining
Estimated remaining rounds: 1
Critical blockers: residual dead removed-feature source scaffolding blocks AC-3

## Drift Summary

Mainline Progress Verdict: ADVANCED
Blocking Side Issues: 1
Queued Side Issues: 2

Claude is still serving the original plan. The current round had a clear
mainline objective and removed the exact Round 18 blockers. The remaining issue
is a blocking side issue only because it is still AC-3 source-pruning work, not
because the visible target surface regressed. Queued side issues remain the
immutable tracker AC-list drift and CPU/tune warn-and-ignore policy.

## Implementation Findings

1. AC-3 remains incomplete: dead removed-extension operand metadata and
   validators are still present. `YSXBaseInfo.h` still enumerates VTYPE, Zibi,
   CLUI, and RVKR operand kinds, and `YSXInstrInfo.cpp` still validates them,
   even though no retained rv64ima instruction should need vector, compressed,
   Zibi, or crypto operands. Delete the unused enum values, validator switch
   cases, and any parser helpers made dead by that deletion.

2. AC-3 remains incomplete: the CSR insertion pass is still registered and run
   even though it is a no-op and Zicsr/CSR instructions are intentionally
   rejected. Remove `YSXInsertReadWriteCSR.cpp` and its declarations/
   registration/pipeline hook unless a retained rv64ima path demonstrably needs
   it.

3. AC-3 remains incomplete: copied SiFive CLIC interrupt frame state and stub
   calls remain after interrupt lowering was changed to reject handlers. Remove
   the `InterruptStackKind` SiFive cases, helper accessors, no-op frame-lowering
   stubs, and comments/call sites such as `emitSiFiveCLICStackSwap` and
   `emitSiFiveCLICPreemptibleSaves`.

## Required Action Items

Mainline Gaps:
1. Delete the residual dead removed-feature scaffolding above and simplify any
   generated or switch users to the rv64ima-only set.
2. Rebuild YSX-only and combined RISCV+YSX static targets after the deletion.
3. Rerun focused YSX lit suites, smoke compiles, and negative probes.

Blocking Side Issues:
1. AC-3 residual dead unsupported-source scaffolding after Round 19.

Queued Side Issues:
1. Immutable tracker AC-list drift; continue reviewing against `docs/plan.md`.
2. CPU/tune target-attribute diagnostics; keep queued unless feature leakage is
   observed or the desired policy becomes hard-error-only.

## Goal Tracker Update

I updated the mutable tracker to Plan Version 40, added a Round 19 review
evolution entry, kept task3/task6 active, collapsed stale resolved blocking
rows into the current AC-3 blocker, and marked the Round 19 completed row as
`19 review partial`. I did not modify the immutable section.

## Stagnation Check

Development is not stagnating. Rounds 15 through 19 repeatedly removed distinct
source/test surfaces and reduced YSX from roughly 33k backend lines to 27,237.
The repeated theme is AC-3 pruning, but the concrete blockers have changed and
Claude has addressed the prior reviews rather than cycling on the same exact
defect. Do not trigger the STOP circuit breaker.

REQUIRES MORE WORK
