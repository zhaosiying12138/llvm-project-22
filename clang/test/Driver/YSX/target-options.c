// RUN: %clang --target=ysx64 -### -c %s 2>&1 | FileCheck %s
// RUN: %clang --target=ysx64-unknown-elf -c %s -o %t-default.o
// RUN: %clang --target=ysx64-unknown-elf -march=rv64ima -c %s -o %t-rv64ima.o
// RUN: not %clang --target=ysx64 -march=rv64gc -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imaf -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imac -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -mabi=lp64d -c %s 2>&1 | FileCheck %s --check-prefix=ABIERR

// CHECK: "-target-cpu" "generic-rv64"
// CHECK: "-target-feature" "+i"
// CHECK: "-target-feature" "+m"
// CHECK: "-target-feature" "+a"
// CHECK: "-target-feature" "+zmmul"
// CHECK: "-target-feature" "+zaamo"
// CHECK: "-target-feature" "+zalrsc"
// CHECK: "-target-abi" "lp64"

// ERR: YuShuXin only supports -march=rv64ima
// ABIERR: unsupported argument 'lp64d' to option '-mabi='

int x;
