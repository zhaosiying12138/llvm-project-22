// RUN: %clang --target=ysx64 -### -c %s 2>&1 | FileCheck %s
// RUN: %clang --target=ysx64-unknown-elf -dM -E -x c /dev/null | FileCheck %s --check-prefix=DEFS --implicit-check-not=__riscv_v_intrinsic --implicit-check-not=__riscv_vector
// RUN: %clang --target=ysx64-unknown-elf -c %s -o %t-default.o
// RUN: %clang --target=ysx64-unknown-elf -march=rv64ima -c %s -o %t-rv64ima.o
// RUN: %clang --target=ysx64-unknown-elf -mno-save-restore -c %s -o %t-no-save-restore.o
// RUN: %clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang -v -c %s -o %t-disable-v.o
// RUN: %clang --target=ysx64 -ffixed-x5 -### -c %s 2>&1 | FileCheck %s --check-prefix=FIXED
// RUN: %clang --target=ysx64-unknown-elf -ffixed-x5 -c %s -o %t-fixed-x5.o
// RUN: %clang --target=ysx64-linux-gnu -### %s 2>&1 | FileCheck %s --check-prefix=LINUX
// RUN: %clang --target=ysx64-unknown-managarm-mlibc -### %s 2>&1 | FileCheck %s --check-prefix=MANAGARM
// RUN: %clang --target=ysx64-pc-hurd-gnu -### %s 2>&1 | FileCheck %s --check-prefix=HURD
// RUN: %clang -### %s --target=ysx64-unknown-linux-gnu --rtlib=platform --unwindlib=platform -fuse-ld= -no-pie --gcc-toolchain=%S/../Inputs/multilib_riscv_linux_sdk --sysroot=%S/../Inputs/multilib_riscv_linux_sdk/sysroot 2>&1 | FileCheck %s --check-prefix=LINUX-MULTI
// RUN: %clang -### %s --target=ysx64-unknown-elf 2>&1 | FileCheck %s --check-prefix=BAREMETAL --implicit-check-not="rv64imac" --implicit-check-not="rv64imafdc"
// RUN: %clang --target=ysx64 --print-supported-extensions 2>&1 | FileCheck %s --check-prefix=EXTS --implicit-check-not="RISC-V" --implicit-check-not="{{^}}    f " --implicit-check-not="{{^}}    d " --implicit-check-not="{{^}}    c " --implicit-check-not="{{^}}    v "
// RUN: %clang --target=ysx64 --print-enabled-extensions 2>&1 | FileCheck %s --check-prefix=ENABLED --implicit-check-not="RISC-V" --implicit-check-not="{{^}}    f " --implicit-check-not="{{^}}    d " --implicit-check-not="{{^}}    c " --implicit-check-not="{{^}}    v "
// RUN: not %clang --target=ysx64 -march=rv64gc -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imaf -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imac -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imav -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -mabi=lp64d -c %s 2>&1 | FileCheck %s --check-prefix=ABIERR
// RUN: not %clang --target=ysx64 -fno-integrated-as -march=rv64gc -### -x assembler -c %s 2>&1 | FileCheck %s --check-prefix=GASERR
// RUN: not %clang --target=ysx64 -fno-integrated-as -mabi=lp64d -### -x assembler -c %s 2>&1 | FileCheck %s --check-prefix=GASABIERR
// RUN: not %clang --target=ysx64 -mrvv-vector-bits=128 -### -c %s 2>&1 | FileCheck %s --check-prefix=RVVBITS --implicit-check-not="-mvscale" --implicit-check-not=__riscv_v_fixed_vlen
// RUN: printf 'typedef __rvv_int8m1_t t;\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=VTYPE
// RUN: printf 'void f(void){ (void)__builtin_rvv_vsetvli(0, 0, 0); }\n' | not %clang --target=ysx64-unknown-elf -x c -fsyntax-only - 2>&1 | FileCheck %s --check-prefix=VBUILTIN
// RUN: printf 'void f(void) __attribute__((target("arch=rv64ima"))); void f(void){}\n' | %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - | FileCheck %s --check-prefix=ATTRIR --implicit-check-not="+v" --implicit-check-not="+f" --implicit-check-not="+d" --implicit-check-not="+zve" --implicit-check-not="+zvl"
// RUN: printf 'void f(void) __attribute__((target("arch=+v"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: printf 'void f(void) __attribute__((target("arch=rv64imaf"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: printf 'void f(void) __attribute__((target("arch=+64bit"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: printf 'void f(void) __attribute__((target("+reserve-x0"))); void f(void){}\n' | not %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - 2>&1 | FileCheck %s --check-prefix=ATTRERR --implicit-check-not="target-features"
// RUN: not %clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang +v -dM -E -x c /dev/null 2>&1 | FileCheck %s --check-prefix=FEATUREERR --implicit-check-not=__riscv_vector --implicit-check-not=__riscv_v
// RUN: not %clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang +reserve-x0 -c %s 2>&1 | FileCheck %s --check-prefix=FEATUREERR
// RUN: not %clang --target=ysx64-unknown-elf -Xclang -target-feature -Xclang -i -c %s 2>&1 | FileCheck %s --check-prefix=REQFEATUREERR
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

// FIXED: "-target-feature" "+reserve-x5"

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
// RVVBITS: error: unsupported option '-mrvv-vector-bits=' for target 'ysx64'
// LINUX: "-dynamic-linker" "/lib/ld-linux-riscv64-lp64.so.1"
// MANAGARM: "-dynamic-linker" "/lib/riscv64-managarm/ld-riscv64-lp64.so"
// HURD: "-dynamic-linker" "/lib/ld-riscv64-lp64.so.1"
// LINUX-MULTI: "{{.*}}Inputs/multilib_riscv_linux_sdk/lib/gcc/riscv64-unknown-linux-gnu/7.2.0/lib64/lp64/crtbegin.o"
// LINUX-MULTI: "-L{{.*}}Inputs/multilib_riscv_linux_sdk/lib/gcc/riscv64-unknown-linux-gnu/7.2.0/lib64/lp64"
// BAREMETAL: "-triple" "ysx64-unknown-unknown-elf"
// BAREMETAL: "{{.*}}clang-runtimes{{[/\\]+}}ysx64-unknown-elf{{[/\\]+}}include"
// EXTS: All available -march extensions for YuShuXin
// EXTS-DAG: {{^}}    i
// EXTS-DAG: {{^}}    m
// EXTS-DAG: {{^}}    a
// EXTS-DAG: {{^}}    zmmul
// EXTS-DAG: {{^}}    zaamo
// EXTS-DAG: {{^}}    zalrsc
// ENABLED: Extensions enabled for the given YuShuXin target
// ENABLED-DAG: {{^}}    i
// ENABLED-DAG: {{^}}    m
// ENABLED-DAG: {{^}}    a
// ENABLED-DAG: {{^}}    zmmul
// ENABLED-DAG: {{^}}    zaamo
// ENABLED-DAG: {{^}}    zalrsc
// GASERR: error: invalid arch name 'rv64gc', YuShuXin only supports -march=rv64ima
// GASABIERR: error: unsupported argument 'lp64d' to option '-mabi='
// VTYPE: error: unknown type name '__rvv_int8m1_t'
// VBUILTIN: error: use of unknown builtin '__builtin_rvv_vsetvli'
// ATTRIR: "target-features"="+64bit,+a,+i,+m,+relax,+zaamo,+zalrsc,+zmmul"
// ATTRERR: error: invalid feature combination: YSX only supports the rv64ima ISA
// FEATUREERR: error: invalid feature combination: YSX only supports the rv64ima ISA
// REQFEATUREERR: error: invalid feature combination: YSX requires the rv64ima ISA
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
