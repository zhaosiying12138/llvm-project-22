typedef int i32;

void ysx_bench_jacobi_1d_i32(i32 *a, i32 *b, int n, int steps) {
  for (int t = 0; t < steps; ++t) {
    for (int i = 1; i < n - 1; ++i)
      b[i] = (a[i - 1] + a[i] + a[i + 1]) / 3;
    for (int i = 1; i < n - 1; ++i)
      a[i] = (b[i - 1] + b[i] + b[i + 1]) / 3;
  }
}
