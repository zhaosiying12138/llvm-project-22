; RUN: %python %S/Inputs/rvv-pressure-scale.py add 896 | llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -verify-machineinstrs | FileCheck %s --check-prefix=ADD-BASE
; RUN: %python %S/Inputs/rvv-pressure-scale.py add 896 | llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -verify-machineinstrs | FileCheck %s --check-prefix=ADD-SCHED
; RUN: %python %S/Inputs/rvv-pressure-scale.py softmax 4 | FileCheck %s --check-prefix=IR-SOFTMAX
; RUN: %python %S/Inputs/rvv-pressure-scale.py softmax 512 | llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -verify-machineinstrs | FileCheck %s --check-prefix=SOFTMAX-BASE
; RUN: %python %S/Inputs/rvv-pressure-scale.py softmax 512 | llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -verify-machineinstrs | FileCheck %s --check-prefix=SOFTMAX-STAGE1
; RUN: %python %S/Inputs/rvv-pressure-scale.py softmax 512 | llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -verify-machineinstrs | FileCheck %s --check-prefix=SOFTMAX-SCHED

; ADD-BASE-LABEL: scale_add:
; ADD-BASE: vs{{[1248]}}r.v

; ADD-SCHED-LABEL: scale_add:
; ADD-SCHED-NOT: vs{{[1248]}}r.v
; ADD-SCHED-NOT: vl{{[1248]}}r.v
; ADD-SCHED: ret

; IR-SOFTMAX-LABEL: define void @scale_softmax
; IR-SOFTMAX: %sumv = shufflevector <128 x float> %sum.ins
; IR-SOFTMAX: %norm0 = fdiv fast <128 x float> %exp0, %sumv
; IR-SOFTMAX: store <128 x float> %norm0
; IR-SOFTMAX: %norm3 = fdiv fast <128 x float> %exp3, %sumv
; IR-SOFTMAX: store <128 x float> %norm3

; SOFTMAX-BASE-LABEL: scale_softmax:
; SOFTMAX-BASE: vs{{[1248]}}r.v
; SOFTMAX-BASE: yushuxin.vfexp
; SOFTMAX-BASE-NOT: exp2f
; SOFTMAX-BASE: ret

; SOFTMAX-STAGE1-LABEL: scale_softmax:
; SOFTMAX-STAGE1: vfredmax.vs
; SOFTMAX-STAGE1: yushuxin.vfexp
; SOFTMAX-STAGE1: vfredusum.vs
; SOFTMAX-STAGE1-NOT: exp2f
; SOFTMAX-STAGE1: ret

; SOFTMAX-SCHED-LABEL: scale_softmax:
; SOFTMAX-SCHED: vfredmax.vs
; SOFTMAX-SCHED: yushuxin.vfexp
; SOFTMAX-SCHED: vfredusum.vs
; SOFTMAX-SCHED-NOT: exp2f
; SOFTMAX-SCHED-NOT: vs{{[1248]}}r.v
; SOFTMAX-SCHED-NOT: vl{{[1248]}}r.v
; SOFTMAX-SCHED: ret
