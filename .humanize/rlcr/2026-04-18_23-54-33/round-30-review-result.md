- [P2] Preserve reserve-x features after validation — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:47-49
  When compiling YSX with `-ffixed-xN` or passing `+reserve-xN`, the driver/frontend deliberately emit `+reserve-xN` and `YSXSubtarget` has `UserReservedRegister` bits to consume them, but this whitelist drops/fatal-errors every enabled feature that is not required/relax/exact-asm. As a result `--target=ysx64 ... -ffixed-x5` fails with “YSX only supports the rv64ima ISA” instead of reserving x5; include `reserve-x*` in the retained feature set and mirror it in the MC copy.

- [P2] Keep LastArchType at the final arch enumerator — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:114-114
  With `ysx64` inserted before `sparc`, setting `LastArchType` to `ysx64` makes loops over `FirstArchType..LastArchType` skip every existing architecture from `sparc` through `ve`. Any code enumerating all architectures, including the in-tree triple tests and downstream users, no longer sees those targets; leave `LastArchType` at the last enum value or append `ysx64` at the end.
The patch introduces a new target but breaks architecture enumeration metadata and rejects the reserved-register features that the driver itself emits for YSX. These are functional issues that should be fixed before considering the patch correct.

Full review comments:

- [P2] Preserve reserve-x features after validation — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/lib/Target/YuShuXin/YSXSubtarget.cpp:47-49
  When compiling YSX with `-ffixed-xN` or passing `+reserve-xN`, the driver/frontend deliberately emit `+reserve-xN` and `YSXSubtarget` has `UserReservedRegister` bits to consume them, but this whitelist drops/fatal-errors every enabled feature that is not required/relax/exact-asm. As a result `--target=ysx64 ... -ffixed-x5` fails with “YSX only supports the rv64ima ISA” instead of reserving x5; include `reserve-x*` in the retained feature set and mirror it in the MC copy.

- [P2] Keep LastArchType at the final arch enumerator — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/llvm/include/llvm/TargetParser/Triple.h:114-114
  With `ysx64` inserted before `sparc`, setting `LastArchType` to `ysx64` makes loops over `FirstArchType..LastArchType` skip every existing architecture from `sparc` through `ve`. Any code enumerating all architectures, including the in-tree triple tests and downstream users, no longer sees those targets; leave `LastArchType` at the last enum value or append `ysx64` at the end.
