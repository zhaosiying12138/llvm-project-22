// RUN: %clang --target=ysx64 -### -c %s 2>&1 | FileCheck %s
// RUN: %clang --target=ysx64-unknown-elf -dM -E -x c /dev/null | FileCheck %s --check-prefix=DEFS --implicit-check-not=__riscv_v_intrinsic --implicit-check-not=__riscv_vector
// RUN: %clang --target=ysx64-unknown-elf -c %s -o %t-default.o
// RUN: %clang --target=ysx64-unknown-elf -march=rv64ima -c %s -o %t-rv64ima.o
// RUN: not %clang --target=ysx64 -march=rv64gc -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imaf -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imac -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imav -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -mabi=lp64d -c %s 2>&1 | FileCheck %s --check-prefix=ABIERR
// RUN: printf 'typedef __rvv_int8m1_t t;\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=VTYPE
// RUN: printf 'void f(void){ (void)__builtin_rvv_vsetvli(0, 0, 0); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=VBUILTIN
// RUN: printf 'void f(void) __attribute__((target("arch=rv64ima"))); void f(void){}\n' | %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - | FileCheck %s --check-prefix=ATTRIR --implicit-check-not="+v" --implicit-check-not="+f" --implicit-check-not="+d" --implicit-check-not="+zve" --implicit-check-not="+zvl"
// RUN: printf 'void f(void) __attribute__((target("arch=+v"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: printf 'void f(void) __attribute__((target("arch=rv64imaf"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: printf 'void f(void) __attribute__((target("arch=+64bit"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: not %clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang +v -dM -E -x c /dev/null 2>&1 | FileCheck %s --check-prefix=FEATUREERR --implicit-check-not=__riscv_vector --implicit-check-not=__riscv_v
// RUN: printf 'void f(double x){ asm volatile("" :: "f"(x)); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMFP
// RUN: printf 'void f(long x){ asm volatile("" :: "vr"(x)); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMV
// RUN: printf 'void f(void){ asm volatile("" ::: "x9", "s1"); }\n' | %clang --target=ysx64-unknown-elf -x c -fsyntax-only -
// RUN: printf 'void f(void){ asm volatile("" ::: "f8"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-F8
// RUN: printf 'void f(void){ asm volatile("" ::: "fs0"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-FS0
// RUN: printf 'void f(void){ asm volatile("" ::: "v0"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-V0
// RUN: printf 'void f(void){ asm volatile("" ::: "vtype"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-VTYPE
// RUN: printf 'void f(void){ asm volatile("" ::: "vl"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-VL
// RUN: printf 'void f(void){ asm volatile("" ::: "vxsat"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-VXSAT
// RUN: printf 'void f(void){ asm volatile("" ::: "vxrm"); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=ASMCLB-VXRM

// CHECK: "-target-cpu" "generic-rv64"
// CHECK: "-target-feature" "+i"
// CHECK: "-target-feature" "+m"
// CHECK: "-target-feature" "+a"
// CHECK: "-target-feature" "+zmmul"
// CHECK: "-target-feature" "+zaamo"
// CHECK: "-target-feature" "+zalrsc"
// CHECK: "-target-feature" "+relax"
// CHECK-NOT: "-target-feature" "-v"
// CHECK-NOT: "-target-feature" "-zve64x"
// CHECK-NOT: "-target-feature" "-zvl128b"
// CHECK-NOT: "-target-feature" "-xventanacondops"
// CHECK: "-target-abi" "lp64"

// DEFS-DAG: #define __SIZEOF_POINTER__ 8
// DEFS-DAG: #define __riscv_xlen 64
// DEFS-DAG: #define __riscv_i 2001000
// DEFS-DAG: #define __riscv_m 2000000
// DEFS-DAG: #define __riscv_a 2001000
// DEFS-DAG: #define __riscv_zmmul 1000000
// DEFS-DAG: #define __riscv_zaamo 1000000
// DEFS-DAG: #define __riscv_zalrsc 1000000
// DEFS-DAG: #define __riscv_float_abi_soft 1

// ERR: YuShuXin only supports -march=rv64ima
// ABIERR: unsupported argument 'lp64d' to option '-mabi='
// VTYPE: error: unknown type name '__rvv_int8m1_t'
// VBUILTIN: error: use of unknown builtin '__builtin_rvv_vsetvli'
// ATTRIR: "target-features"="+64bit,+a,+i,+m,+relax,+zaamo,+zalrsc,+zmmul"
// ATTRERR: error: invalid feature combination: YSX only supports the rv64ima ISA
// FEATUREERR: error: invalid feature combination: YSX only supports the rv64ima ISA
// ASMFP: error: invalid input constraint 'f' in asm
// ASMV: error: invalid input constraint 'vr' in asm
// ASMCLB-F8: error: unknown register name 'f8' in asm
// ASMCLB-FS0: error: unknown register name 'fs0' in asm
// ASMCLB-V0: error: unknown register name 'v0' in asm
// ASMCLB-VTYPE: error: unknown register name 'vtype' in asm
// ASMCLB-VL: error: unknown register name 'vl' in asm
// ASMCLB-VXSAT: error: unknown register name 'vxsat' in asm
// ASMCLB-VXRM: error: unknown register name 'vxrm' in asm

int x;
