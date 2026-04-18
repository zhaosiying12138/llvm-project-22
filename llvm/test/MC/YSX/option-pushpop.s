
# RUN: llvm-mc -triple ysx64 -mattr=-relax -M no-aliases < %s \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s
# RUN: llvm-mc -filetype=obj -triple ysx64 < %s \
# RUN:     | llvm-readobj -r - | FileCheck -check-prefix=CHECK-RELOC %s
# RUN: llvm-mc -triple ysx64 -filetype=obj < %s \
# RUN:     | llvm-objdump --no-print-imm-hex --triple=ysx64 -d - \
# RUN:     | FileCheck -check-prefix=CHECK-ALIAS %s

# Test the operation of the push and pop assembler directives when
# using .option relax. Checks that using .option pop
# correctly restores all target features to their state at the point
# where .option pop was last used.

# CHECK-INST: call foo
# CHECK-RELOC: R_RISCV_CALL_PLT foo 0x0
# CHECK-RELOC-NOT: R_RISCV_RELAX - 0x0
call foo

# CHECK-INST: addi s0, sp, 1020
# CHECK-BYTES: 3fc10413
# CHECK-ALIAS: addi s0, sp, 1020
addi s0, sp, 1020

.option push    # Push relax=false, rvc=false
# CHECK-INST: .option push

.option relax
# CHECK-INST: .option relax
# CHECK-INST: call bar
# CHECK-RELOC-NEXT: R_RISCV_CALL_PLT bar 0x0
# CHECK-RELOC-NEXT: R_RISCV_RELAX - 0x0
call bar

# CHECK-INST: call bar
# CHECK-RELOC-NEXT: R_RISCV_CALL_PLT bar 0x0
# CHECK-RELOC-NEXT: R_RISCV_RELAX - 0x0
call bar

.option pop     # Pop relax=false, rvc=false
# CHECK-INST: .option pop
# CHECK-INST: call baz
# CHECK-RELOC: R_RISCV_CALL_PLT baz 0x0
# CHECK-RELOC-NOT: R_RISCV_RELAX - 0x0
call baz

# CHECK-INST: addi s0, sp, 1020
# CHECK-BYTES: 3fc10413
# CHECK-ALIAS: addi s0, sp, 1020
addi s0, sp, 1020

.option push    # Push pic=false
# CHECK-INST: .option push

.option pic
# CHECK-INST: .option pic

la s0, symbol
# CHECK-INST: auipc	s0, %got_pcrel_hi(symbol)
# CHECK-INST: l{{[wd]}}	s0, %pcrel_lo(.Lpcrel_hi0)(s0)
# CHECK-RELOC: R_RISCV_GOT_HI20 symbol 0x0
# CHECK-RELOC: R_RISCV_PCREL_LO12_I .Lpcrel_hi0 0x0

.option push    # Push pic=true
# CHECK-INST: .option push

.option nopic
# CHECK-INST: .option nopic

la s0, symbol
# CHECK-INST: auipc	s0, %pcrel_hi(symbol)
# CHECK-INST: addi	s0, s0, %pcrel_lo(.Lpcrel_hi1)
# CHECK-RELOC: R_RISCV_PCREL_HI20 symbol 0x0
# CHECK-RELOC: R_RISCV_PCREL_LO12_I .Lpcrel_hi1 0x0

.option pop    # Push pic=true
# CHECK-INST: .option pop

la s0, symbol
# CHECK-INST: auipc	s0, %got_pcrel_hi(symbol)
# CHECK-INST: l{{[wd]}}	s0, %pcrel_lo(.Lpcrel_hi2)(s0)
# CHECK-RELOC: R_RISCV_GOT_HI20 symbol 0x0
# CHECK-RELOC: R_RISCV_PCREL_LO12_I .Lpcrel_hi2 0x0

.option pop    # Push pic=false
# CHECK-INST: .option pop

la s0, symbol
# CHECK-INST: auipc	s0, %pcrel_hi(symbol)
# CHECK-INST: addi	s0, s0, %pcrel_lo(.Lpcrel_hi3)
# CHECK-RELOC: R_RISCV_PCREL_HI20 symbol 0x0
# CHECK-RELOC: R_RISCV_PCREL_LO12_I .Lpcrel_hi3 0x0
