# RUN: llvm-mc -triple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp -show-encoding < %s \
# RUN:   | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp -filetype=obj < %s \
# RUN:   | llvm-objdump --mattr=+v,+experimental-yushuxin-vfexp -d - \
# RUN:   | FileCheck %s --check-prefix=DIS
# RUN: not llvm-mc -triple=riscv64 -mattr=+v < %s 2>&1 \
# RUN:   | FileCheck %s --check-prefix=NOFEATURE

# ASM: yushuxin.vfexp v8, v9 # encoding:
# DIS: yushuxin.vfexp v8, v9
# NOFEATURE: instruction requires the following: 'experimental-yushuxin-vfexp'
yushuxin.vfexp v8, v9
