typedef int i32;

void ysx_bench_mvt_i32(const i32 *a, const i32 *y1, const i32 *y2, i32 *x1,
                       i32 *x2, int n) {
  for (int i = 0; i < n; ++i) {
    i32 acc = x1[i];
    for (int j = 0; j < n; ++j)
      acc += a[i * n + j] * y1[j];
    x1[i] = acc;
  }
  for (int i = 0; i < n; ++i) {
    i32 acc = x2[i];
    for (int j = 0; j < n; ++j)
      acc += a[j * n + i] * y2[j];
    x2[i] = acc;
  }
}
