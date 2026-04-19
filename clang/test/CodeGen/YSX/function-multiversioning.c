// RUN: %clang_cc1 -triple ysx64-linux-gnu -emit-llvm -verify %s

__attribute__((target_version("default"))) int tv_default(void) {
  // expected-error@-1 {{function multiversioning is not supported on the current target}}
  return 1;
}

__attribute__((target_clones("default", "arch=+m"))) int tc(void) {
  // expected-error@-1 {{function multiversioning is not supported on the current target}}
  return 2;
}
