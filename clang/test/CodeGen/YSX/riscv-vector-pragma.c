// RUN: %clang_cc1 -triple ysx64-unknown-elf -fsyntax-only -Wunknown-pragmas -verify %s

#pragma clang riscv intrinsic vector
// expected-warning@-1 {{unknown pragma ignored}}
