# RUN: llvm-mc %s -triple=ysx64 -mattr=+xtinyv -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=ysx64 -mattr=+xtinyv < %s \
# RUN:     | llvm-objdump --triple=ysx64 --mattr=+xtinyv --no-print-imm-hex -d - \
# RUN:     | FileCheck -check-prefix=CHECK-ASM-AND-OBJ %s

# CHECK-ASM-AND-OBJ: vadd.vv v1, v2, v3
# CHECK-ASM: encoding: [0xd7,0x80,0x21,0x02]
vadd.vv v1, v2, v3

# CHECK-ASM-AND-OBJ: vadd.vv v1, v2, v3, v0.t
# CHECK-ASM: encoding: [0xd7,0x80,0x21,0x00]
vadd.vv v1, v2, v3, v0.t

# CHECK-ASM-AND-OBJ: vle32.v v7, (a0)
# CHECK-ASM: encoding: [0x87,0x63,0x05,0x02]
vle32.v v7, (a0)

# CHECK-ASM-AND-OBJ: vsetivli a4, 4, 208
# CHECK-ASM: encoding: [0x57,0x77,0x02,0xcd]
vsetivli a4, 4, 208

# CHECK-ASM-AND-OBJ: vsetvli a5, a3, 208
# CHECK-ASM: encoding: [0xd7,0xf7,0x06,0x0d]
vsetvli a5, a3, 208

# CHECK-ASM-AND-OBJ: vmv1r.v v11, v12
# CHECK-ASM: encoding: [0xd7,0x35,0xc0,0x9e]
vmv1r.v v11, v12

# CHECK-ASM-AND-OBJ: vse32.v v8, (a1), v0.t
# CHECK-ASM: encoding: [0x27,0xe4,0x05,0x00]
vse32.v v8, (a1), v0.t

# CHECK-ASM-AND-OBJ: vfredusum.vs v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x12,0x53,0x04]
vfredusum.vs v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: vfredusum.vs v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x12,0x53,0x04]
vfredsum.vs v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: yushuxin.vfexp v9, v10
# CHECK-ASM: encoding: [0x8b,0x14,0xa0,0xaa]
yushuxin.vfexp v9, v10
