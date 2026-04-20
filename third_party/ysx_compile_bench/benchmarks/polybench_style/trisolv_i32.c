typedef int i32;

void ysx_bench_trisolv_i32(const i32 *lower, const i32 *b, i32 *x, int n) {
  for (int i = 0; i < n; ++i) {
    i32 acc = b[i];
    for (int j = 0; j < i; ++j)
      acc -= lower[i * n + j] * x[j];
    i32 diag = lower[i * n + i];
    if (diag == 0)
      diag = 1;
    x[i] = acc / diag;
  }
}
