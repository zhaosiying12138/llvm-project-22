# Round 30 Contract

## Mainline Objective

Preserve the completed YSX rv64ima backend goal while fixing the two review
blocking side issues found after Round 29.

## Blocking Side Issues

| Issue | Classification | Resolution |
|-------|----------------|------------|
| `+reserve-xN` is rejected by YSX feature filtering | blocking | Retain valid `reserve-x1` through `reserve-x31` features in both CodeGen and MC feature filters, and add positive coverage for `-ffixed-x5` / `+reserve-x5`. |
| `Triple::LastArchType` points at `ysx64` instead of the final enum value | blocking | Restore `LastArchType` to the final architecture enumerator so existing architectures after `ysx64` remain covered by architecture enumeration loops. |

## Queued Side Issues

No new queued issues.

## Acceptance Checks

- YSX `-ffixed-x5` and backend/MC `+reserve-x5` paths compile or assemble.
- Unsupported YSX features such as `+zbb` still reject.
- TargetParser tests cover the restored architecture enumeration boundary.
- YSX-only and RISCV+YSX static builds still pass.
- Focused YSX LLVM/Clang lit tests still pass.
- RISCV source and test directories remain unchanged.
