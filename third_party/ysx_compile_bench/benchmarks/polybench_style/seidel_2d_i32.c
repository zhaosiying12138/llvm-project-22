typedef int i32;

void ysx_bench_seidel_2d_i32(i32 *a, int n, int steps) {
  for (int t = 0; t < steps; ++t) {
    for (int i = 1; i < n - 1; ++i) {
      for (int j = 1; j < n - 1; ++j) {
        i32 sum = a[(i - 1) * n + (j - 1)] + a[(i - 1) * n + j] +
                  a[(i - 1) * n + (j + 1)] + a[i * n + (j - 1)] +
                  a[i * n + j] + a[i * n + (j + 1)] +
                  a[(i + 1) * n + (j - 1)] + a[(i + 1) * n + j] +
                  a[(i + 1) * n + (j + 1)];
        a[i * n + j] = sum / 9;
      }
    }
  }
}
