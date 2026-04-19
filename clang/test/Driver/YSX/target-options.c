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

int x;
