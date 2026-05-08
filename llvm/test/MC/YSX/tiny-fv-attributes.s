# RUN: llvm-mc -triple=ysx64 -filetype=asm %s | FileCheck %s
# RUN: printf '.attribute arch, "rv64ima_f"\n' | not llvm-mc -triple=ysx64 -filetype=asm - 2>&1 | FileCheck %s --check-prefix=ERR
# RUN: printf '.attribute arch, "rv64ima_v"\n' | not llvm-mc -triple=ysx64 -filetype=asm - 2>&1 | FileCheck %s --check-prefix=ERR
# RUN: printf '.attribute arch, "rv64ima_c"\n' | not llvm-mc -triple=ysx64 -filetype=asm - 2>&1 | FileCheck %s --check-prefix=ERR

.attribute arch, "rv64ima_xtinyf"
# CHECK: .attribute 5, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0_xtinyf1p0"

.attribute arch, "rv64ima_xtinyv"
# CHECK: .attribute 5, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0_xtinyv1p0_zvl128b1p0"

.attribute arch, "rv64ima_xtinyv1p0_zvl128b1p0"
# CHECK: .attribute 5, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0_xtinyv1p0_zvl128b1p0"

.attribute arch, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0_xtinyf1p0_xtinyv1p0_zvl128b1p0"
# CHECK: .attribute 5, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0_xtinyf1p0_xtinyv1p0_zvl128b1p0"

# ERR: invalid arch name
