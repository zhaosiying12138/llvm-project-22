# RUN: llvm-mc -triple=ysx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=ysx64 %s -filetype=obj -o - | llvm-objdump -d --triple=ysx64 - | FileCheck %s --check-prefix=DIS

add a0, a0, a1
# ENC: encoding: [0x33,0x05,0xb5,0x00]
# DIS: add a0, a0, a1

mul a0, a0, a1
# ENC: encoding: [0x33,0x05,0xb5,0x02]
# DIS: mul a0, a0, a1
