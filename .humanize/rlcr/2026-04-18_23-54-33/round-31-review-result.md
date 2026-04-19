- [P2] Add Linux linker handling before advertising YSX Linux — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/lib/Basic/Targets.cpp:495-499
  For `--target=ysx64-linux-gnu`, this path makes Clang accept a Linux YSX target, but the Linux toolchain still has no `ysx64` case in `Linux::getDynamicLinker`; its arch switch falls to `llvm_unreachable`, so a normal dynamic link aborts instead of producing a linker command. Please either add YSX handling in the Linux/GNU driver paths or don't advertise Linux as supported yet.

- [P2] Reject RVV vector-size options for YSX — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/lib/Driver/ToolChains/Clang.cpp:2017-2023
  For `ysx64`, this branch forces the ABI but then falls through to the generic RISC-V `-mrvv-vector-bits` handling below; e.g. `--target=ysx64 -mrvv-vector-bits=128` is accepted and adds `-mvscale-*`, which makes the frontend define `__riscv_v_fixed_vlen` even though this target intentionally removes the vector frontend and vector macros. Please diagnose or ignore RVV-only options for YSX before the generic RISC-V handling runs.
The patch introduces a new target but leaves supported driver paths inconsistent: Linux linking can abort for advertised YSX Linux triples, and RVV-only options remain active despite the target disabling vector support.

Full review comments:

- [P2] Add Linux linker handling before advertising YSX Linux — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/lib/Basic/Targets.cpp:495-499
  For `--target=ysx64-linux-gnu`, this path makes Clang accept a Linux YSX target, but the Linux toolchain still has no `ysx64` case in `Linux::getDynamicLinker`; its arch switch falls to `llvm_unreachable`, so a normal dynamic link aborts instead of producing a linker command. Please either add YSX handling in the Linux/GNU driver paths or don't advertise Linux as supported yet.

- [P2] Reject RVV vector-size options for YSX — /home/zhaosiying/codebase/compiler/llvm-project-22.1.3-ysx/clang/lib/Driver/ToolChains/Clang.cpp:2017-2023
  For `ysx64`, this branch forces the ABI but then falls through to the generic RISC-V `-mrvv-vector-bits` handling below; e.g. `--target=ysx64 -mrvv-vector-bits=128` is accepted and adds `-mvscale-*`, which makes the frontend define `__riscv_v_fixed_vlen` even though this target intentionally removes the vector frontend and vector macros. Please diagnose or ignore RVV-only options for YSX before the generic RISC-V handling runs.
