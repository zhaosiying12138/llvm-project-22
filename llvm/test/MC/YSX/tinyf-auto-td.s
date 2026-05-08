# RUN: llvm-mc %s -triple=ysx64 -mattr=+xtinyf -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=ysx64 -mattr=+xtinyf < %s \
# RUN:     | llvm-objdump --triple=ysx64 --mattr=+xtinyf --no-print-imm-hex -d - \
# RUN:     | FileCheck -check-prefix=CHECK-ASM-AND-OBJ %s
# RUN: not llvm-mc %s -triple=ysx64 -show-encoding 2>&1 \
# RUN:     | FileCheck -check-prefix=CHECK-NO-FEATURE %s

# CHECK-NO-FEATURE: instruction requires the following: 'XTinyF'

# CHECK-ASM-AND-OBJ: flw ft0, 0(a0)
# CHECK-ASM: encoding: [0x07,0x20,0x05,0x00]
flw ft0, 0(a0)

# CHECK-ASM-AND-OBJ: fsw ft1, 4(a1)
# CHECK-ASM: encoding: [0x27,0xa2,0x15,0x00]
fsw ft1, 4(a1)

# CHECK-ASM-AND-OBJ: fadd.s ft2, ft3, ft4, 0
# CHECK-ASM: encoding: [0x53,0x81,0x41,0x00]
fadd.s ft2, ft3, ft4, 0

# CHECK-ASM-AND-OBJ: fsub.s ft5, ft6, ft7, 1
# CHECK-ASM: encoding: [0xd3,0x12,0x73,0x08]
fsub.s ft5, ft6, ft7, 1

# CHECK-ASM-AND-OBJ: fmul.s fa0, fa1, fa2, 2
# CHECK-ASM: encoding: [0x53,0xa5,0xc5,0x10]
fmul.s fa0, fa1, fa2, 2

# CHECK-ASM-AND-OBJ: feq.s a0, fa3, fa4
# CHECK-ASM: encoding: [0x53,0xa5,0xe6,0xa0]
feq.s a0, fa3, fa4

# CHECK-ASM-AND-OBJ: flt.s a1, fa5, fa6
# CHECK-ASM: encoding: [0xd3,0x95,0x07,0xa1]
flt.s a1, fa5, fa6

# CHECK-ASM-AND-OBJ: fle.s a2, fa7, fs0
# CHECK-ASM: encoding: [0x53,0x86,0x88,0xa0]
fle.s a2, fa7, fs0

# CHECK-ASM-AND-OBJ: fcvt.w.s a3, fs1, 0
# CHECK-ASM: encoding: [0xd3,0x86,0x04,0xc0]
fcvt.w.s a3, fs1, 0

# CHECK-ASM-AND-OBJ: fcvt.wu.s a4, fs2, 1
# CHECK-ASM: encoding: [0x53,0x17,0x19,0xc0]
fcvt.wu.s a4, fs2, 1

# CHECK-ASM-AND-OBJ: fcvt.s.w fs3, a5, 2
# CHECK-ASM: encoding: [0xd3,0xa9,0x07,0xd0]
fcvt.s.w fs3, a5, 2

# CHECK-ASM-AND-OBJ: fcvt.s.wu fs4, a6, 3
# CHECK-ASM: encoding: [0x53,0x3a,0x18,0xd0]
fcvt.s.wu fs4, a6, 3
