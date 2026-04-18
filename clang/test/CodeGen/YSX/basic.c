// RUN: %clang_cc1 -triple ysx64 -emit-llvm %s -o - | FileCheck %s

long add(long a, long b) {
// CHECK-LABEL: define{{.*}} i64 @add
// CHECK: add nsw i64
  return a + b;
}
