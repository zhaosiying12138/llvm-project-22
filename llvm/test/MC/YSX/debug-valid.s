# RUN: llvm-mc %s -triple=ysx64 -M no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK,CHECK-INST %s
# RUN: llvm-mc -filetype=obj -triple ysx64 < %s \
# RUN:     | llvm-objdump --triple=ysx64 -M no-aliases -d - \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s

# CHECK-INST: dret
# CHECK: encoding: [0x73,0x00,0x20,0x7b]
dret
