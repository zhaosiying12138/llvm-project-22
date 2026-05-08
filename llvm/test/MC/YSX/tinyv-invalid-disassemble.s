# RUN: llvm-mc -triple=ysx64 -mattr=+xtinyv --disassemble < %s 2>&1 \
# RUN:     | FileCheck %s

# CHECK: warning: invalid instruction encoding
# CHECK-NOT: vmerge.vvm
0x57 0x0e 0xdf 0x5f
