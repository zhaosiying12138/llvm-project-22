// RUN: %clang_cc1 -triple ysx64 -emit-llvm %s -o - | FileCheck %s

long add(long a, long b) {
// CHECK-LABEL: define{{.*}} i64 @add
// CHECK: add nsw i64
  return a + b;
}

struct pair {
  long a;
  long b;
};

struct pair ret_pair(long a, long b) {
// CHECK-LABEL: define{{.*}} [2 x i64] @ret_pair
  struct pair p = {a, b};
// CHECK: ret [2 x i64]
  return p;
}

long take_pair(struct pair p) {
// CHECK-LABEL: define{{.*}} i64 @take_pair([2 x i64] %p.coerce)
  return p.a + p.b;
}
