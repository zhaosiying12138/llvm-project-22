# RUN: llvm-mc %s -filetype=obj -triple=ysx64 | llvm-readobj -h - \
# RUN:     | FileCheck -check-prefix=RV64 %s


# RV64: Format: elf64-littleriscv
# RV64: Arch: riscv64
# RV64: AddressSize: 64bit
# RV64: ElfHeader {
# RV64:   Ident {
# RV64:     Magic: (7F 45 4C 46)
# RV64:     Class: 64-bit (0x2)
# RV64:     DataEncoding: LittleEndian (0x1)
# RV64:     FileVersion: 1
# RV64:     OS/ABI: SystemV (0x0)
# RV64:     ABIVersion: 0
# RV64:   }
# RV64:   Type: Relocatable (0x1)
# RV64:   Machine: EM_RISCV (0xF3)
# RV64:   Version: 1
# RV64:   Flags [ (0x0)
# RV64:   ]
# RV64: }
