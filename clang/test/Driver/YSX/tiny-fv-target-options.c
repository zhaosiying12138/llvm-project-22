// RUN: %clang --target=ysx64 -### -c %s 2>&1 | FileCheck %s --check-prefix=BASE --implicit-check-not="+xtinyf" --implicit-check-not="+xtinyv" --implicit-check-not="+zvl128b"
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyf -### -c %s 2>&1 | FileCheck %s --check-prefix=TINYF --implicit-check-not="+f" --implicit-check-not="+d"
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyv_zvl128b -### -c %s 2>&1 | FileCheck %s --check-prefix=TINYV --implicit-check-not="+v" --implicit-check-not="+zve"
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyv -### -c %s 2>&1 | FileCheck %s --check-prefix=TINYV-IMPLIED --implicit-check-not="+v" --implicit-check-not="+zve"
// RUN: not %clang --target=ysx64 -march=rv64imaf -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imav -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64imac -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: not %clang --target=ysx64 -march=rv64gc -c %s 2>&1 | FileCheck %s --check-prefix=ERR
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyf -dM -E -x c /dev/null | FileCheck %s --check-prefix=DEFS-TINYF --implicit-check-not=__riscv_f --implicit-check-not=__riscv_d --implicit-check-not=__riscv_vector --implicit-check-not=__riscv_v_intrinsic
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyv_zvl128b -dM -E -x c /dev/null | FileCheck %s --check-prefix=DEFS-TINYV --implicit-check-not=__riscv_vector --implicit-check-not=__riscv_v_intrinsic
// RUN: printf 'void f(void) __attribute__((target("arch=rv64ima_xtinyf"))); void f(void){}\n' | %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - | FileCheck %s --check-prefix=ATTR-TINYF --implicit-check-not="+f" --implicit-check-not="+d" --implicit-check-not="+v" --implicit-check-not="+zve"
// RUN: printf 'void f(void) __attribute__((target("arch=rv64ima_xtinyv_zvl128b"))); void f(void){}\n' | %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - | FileCheck %s --check-prefix=ATTR-TINYV --implicit-check-not="+f" --implicit-check-not="+d" --implicit-check-not="+v" --implicit-check-not="+zve"
// RUN: printf 'void f(void) __attribute__((target("arch=+xtinyv"))); void f(void){}\n' | %clang --target=ysx64-unknown-elf -S -emit-llvm -x c - -o - | FileCheck %s --check-prefix=ATTR-TINYV --implicit-check-not="+f" --implicit-check-not="+d" --implicit-check-not="+v" --implicit-check-not="+zve"
// RUN: %clang --target=ysx64 --print-supported-extensions 2>&1 | FileCheck %s --check-prefix=EXTS --implicit-check-not="{{^}}    f " --implicit-check-not="{{^}}    v " --implicit-check-not="{{^}}    c "
// RUN: %clang --target=ysx64 -march=rv64ima_xtinyv --print-enabled-extensions 2>&1 | FileCheck %s --check-prefix=ENABLED-TINYV --implicit-check-not="{{^}}    f " --implicit-check-not="{{^}}    v " --implicit-check-not="{{^}}    c "

// BASE: "-target-feature" "+i"
// BASE: "-target-feature" "+m"
// BASE: "-target-feature" "+a"
// BASE: "-target-abi" "lp64"

// TINYF: "-target-feature" "+xtinyf"

// TINYV-DAG: "-target-feature" "+xtinyv"
// TINYV-DAG: "-target-feature" "+zvl128b"

// TINYV-IMPLIED-DAG: "-target-feature" "+xtinyv"
// TINYV-IMPLIED-DAG: "-target-feature" "+zvl128b"

// ERR: YuShuXin only supports -march=rv64ima

// DEFS-TINYF-DAG: #define __riscv_xtinyf 1000000

// DEFS-TINYV-DAG: #define __riscv_xtinyv 1000000
// DEFS-TINYV-DAG: #define __riscv_zvl128b 1000000

// ATTR-TINYF: "target-features"="+64bit,+a,+i,+m,+relax,+xtinyf,+zaamo,+zalrsc,+zmmul"

// ATTR-TINYV: "target-features"="+64bit,+a,+i,+m,+relax,+xtinyv,+zaamo,+zalrsc,+zmmul,+zvl128b"

// EXTS: All available -march extensions for YuShuXin
// EXTS-DAG: {{^}}    xtinyf
// EXTS-DAG: {{^}}    xtinyv
// EXTS-DAG: {{^}}    zvl128b

// ENABLED-TINYV: Extensions enabled for the given YuShuXin target
// ENABLED-TINYV-DAG: {{^}}    xtinyv
// ENABLED-TINYV-DAG: {{^}}    zvl128b

int x;
