# RUN: not llvm-mc %s -triple=ysx64 -M no-aliases -show-encoding 2>&1 \
# RUN:     | FileCheck %s

dret # CHECK: :[[@LINE]]:1: error: unrecognized instruction mnemonic
