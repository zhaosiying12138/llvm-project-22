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

# CHECK-ASM-AND-OBJ: vlse32.v v9, (a0), a1
# CHECK-ASM: encoding: [0x87,0x64,0xb5,0x0a]
vlse32.v v9, (a0), a1

# CHECK-ASM-AND-OBJ: vluxei32.v v11, (a4), v12
# CHECK-ASM: encoding: [0x87,0x65,0xc7,0x06]
vluxei32.v v11, (a4), v12

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

# CHECK-ASM-AND-OBJ: vsse32.v v10, (a2), a3, v0.t
# CHECK-ASM: encoding: [0x27,0x65,0xd6,0x08]
vsse32.v v10, (a2), a3, v0.t

# CHECK-ASM-AND-OBJ: vsuxei32.v v13, (a5), v14, v0.t
# CHECK-ASM: encoding: [0xa7,0xe6,0xe7,0x04]
vsuxei32.v v13, (a5), v14, v0.t

# CHECK-ASM-AND-OBJ: vsub.vv v1, v2, v3
# CHECK-ASM: encoding: [0xd7,0x80,0x21,0x0a]
vsub.vv v1, v2, v3

# CHECK-ASM-AND-OBJ: vmul.vv v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x22,0x53,0x94]
vmul.vv v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: vmin.vv v7, v8, v9
# CHECK-ASM: encoding: [0xd7,0x83,0x84,0x16]
vmin.vv v7, v8, v9

# CHECK-ASM-AND-OBJ: vmax.vv v10, v11, v12, v0.t
# CHECK-ASM: encoding: [0x57,0x05,0xb6,0x1c]
vmax.vv v10, v11, v12, v0.t

# CHECK-ASM-AND-OBJ: vand.vv v13, v14, v15
# CHECK-ASM: encoding: [0xd7,0x86,0xe7,0x26]
vand.vv v13, v14, v15

# CHECK-ASM-AND-OBJ: vor.vv v16, v17, v18, v0.t
# CHECK-ASM: encoding: [0x57,0x08,0x19,0x29]
vor.vv v16, v17, v18, v0.t

# CHECK-ASM-AND-OBJ: vxor.vv v19, v20, v21
# CHECK-ASM: encoding: [0xd7,0x89,0x4a,0x2f]
vxor.vv v19, v20, v21

# CHECK-ASM-AND-OBJ: vmseq.vv v22, v23, v24, v0.t
# CHECK-ASM: encoding: [0x57,0x0b,0x7c,0x61]
vmseq.vv v22, v23, v24, v0.t

# CHECK-ASM-AND-OBJ: vmslt.vv v25, v26, v27
# CHECK-ASM: encoding: [0xd7,0x8c,0xad,0x6f]
vmslt.vv v25, v26, v27

# CHECK-ASM-AND-OBJ: vmerge.vvm v28, v29, v30, v0
# CHECK-ASM: encoding: [0x57,0x0e,0xdf,0x5d]
vmerge.vvm v28, v29, v30, v0

# CHECK-ASM-AND-OBJ: vredsum.vs v1, v2, v3
# CHECK-ASM: encoding: [0xd7,0xa0,0x21,0x02]
vredsum.vs v1, v2, v3

# CHECK-ASM-AND-OBJ: vredmin.vs v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x22,0x53,0x14]
vredmin.vs v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: vredmax.vs v7, v8, v9
# CHECK-ASM: encoding: [0xd7,0xa3,0x84,0x1e]
vredmax.vs v7, v8, v9

# CHECK-ASM-AND-OBJ: vredand.vs v10, v11, v12, v0.t
# CHECK-ASM: encoding: [0x57,0x25,0xb6,0x04]
vredand.vs v10, v11, v12, v0.t

# CHECK-ASM-AND-OBJ: vredor.vs v13, v14, v15
# CHECK-ASM: encoding: [0xd7,0xa6,0xe7,0x0a]
vredor.vs v13, v14, v15

# CHECK-ASM-AND-OBJ: vredxor.vs v16, v17, v18, v0.t
# CHECK-ASM: encoding: [0x57,0x28,0x19,0x0d]
vredxor.vs v16, v17, v18, v0.t

# CHECK-ASM-AND-OBJ: vfredusum.vs v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x12,0x53,0x04]
vfredusum.vs v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: vfredusum.vs v4, v5, v6, v0.t
# CHECK-ASM: encoding: [0x57,0x12,0x53,0x04]
vfredsum.vs v4, v5, v6, v0.t

# CHECK-ASM-AND-OBJ: vfredmin.vs v19, v20, v21
# CHECK-ASM: encoding: [0xd7,0x99,0x4a,0x17]
vfredmin.vs v19, v20, v21

# CHECK-ASM-AND-OBJ: vfredmax.vs v22, v23, v24, v0.t
# CHECK-ASM: encoding: [0x57,0x1b,0x7c,0x1d]
vfredmax.vs v22, v23, v24, v0.t

# CHECK-ASM-AND-OBJ: vslideup.vx v25, v26, a0
# CHECK-ASM: encoding: [0xd7,0x4c,0xa5,0x3b]
vslideup.vx v25, v26, a0

# CHECK-ASM-AND-OBJ: vslidedown.vx v27, v28, a1, v0.t
# CHECK-ASM: encoding: [0xd7,0xcd,0xc5,0x3d]
vslidedown.vx v27, v28, a1, v0.t

# CHECK-ASM-AND-OBJ: vrgather.vv v29, v30, v31
# CHECK-ASM: encoding: [0xd7,0x8e,0xef,0x33]
vrgather.vv v29, v30, v31

# CHECK-ASM-AND-OBJ: vmv.v.x v2, a2
# CHECK-ASM: encoding: [0x57,0x41,0x06,0x5e]
vmv.v.x v2, a2

# CHECK-ASM-AND-OBJ: vmv.v.v v3, v4
# CHECK-ASM: encoding: [0xd7,0x01,0x02,0x5e]
vmv.v.v v3, v4

# CHECK-ASM-AND-OBJ: yushuxin.vfexp v9, v10
# CHECK-ASM: encoding: [0x8b,0x14,0xa0,0xaa]
yushuxin.vfexp v9, v10
