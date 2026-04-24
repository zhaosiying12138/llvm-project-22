# Round 1 Contract

## Mainline Objective

Close the reviewed AC-3 through AC-6 gaps by making the scheduler scoping,
reload safety evidence, frame diagnostics, and report reproducibility match the
original plan.

## Target ACs

- AC-3: `-riscv-v-reg-pressure-aware-sched` enables pressure-aware scheduling
  only for high-pressure RVV regions and remains default-off.
- AC-5: `-riscv-v-reg-pressure-report` prints useful RVV stack diagnostics with
  a true spill-slot count.

## Blocking Side Issues In Scope

- `rvv-spill-slots` currently counts all scalable-vector frame objects.
- Load/store clustering is suppressed for the full function under the hidden
  flag instead of being limited to pressure-heavy RVV regions.

## Queued Side Issues Out Of Scope

- None. The AC-4 reload safety tests and AC-6 report fixes are required
  companion evidence for this round's mainline objective and will be completed
  before the stop gate.

## Round Success Criteria

- An RVV alloca reports zero RVV spill slots, while actual RVV spills still
  report nonzero slots.
- The scheduler retains load/store clustering globally and only suppresses
  clustering for high-pressure RVV scheduling regions.
- Lit coverage includes default-off scheduler behavior, masked-vector validity,
  reload positive and negative safety cases, and frame diagnostics.
- `docs/report.md` contains exact reproducible per-workload commands,
  measurement procedure, spill/reload counts, diagnostics snippets, and
  before/after snippets.
