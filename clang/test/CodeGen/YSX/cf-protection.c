// RUN: %clang_cc1 -triple ysx64-unknown-elf -emit-llvm -fcf-protection=branch -verify %s
// RUN: %clang_cc1 -triple ysx64-unknown-elf -emit-llvm -fcf-protection=return -verify=return %s

int f(void) {
  return 0;
}

// expected-error@* {{option 'cf-protection=branch' cannot be specified on this target}}
// return-error@* {{option 'cf-protection=return' cannot be specified on this target}}
