typedef int i32;

void ysx_bench_matmul_i32(const i32 *a, const i32 *b, i32 *c, int n) {
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      i32 sum = 0;
      for (int k = 0; k < n; ++k)
        sum += a[i * n + k] * b[k * n + j];
      c[i * n + j] = sum;
    }
  }
}
