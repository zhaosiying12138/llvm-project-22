typedef int i32;

void ysx_bench_atax_i32(const i32 *a, const i32 *x, i32 *y, i32 *tmp,
                        int rows, int cols) {
  for (int i = 0; i < cols; ++i)
    y[i] = 0;
  for (int i = 0; i < rows; ++i) {
    i32 acc = 0;
    for (int j = 0; j < cols; ++j)
      acc += a[i * cols + j] * x[j];
    tmp[i] = acc;
    for (int j = 0; j < cols; ++j)
      y[j] += a[i * cols + j] * acc;
  }
}
