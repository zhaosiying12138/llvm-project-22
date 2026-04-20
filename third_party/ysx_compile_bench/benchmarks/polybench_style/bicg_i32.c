typedef int i32;

void ysx_bench_bicg_i32(const i32 *a, const i32 *p, const i32 *r, i32 *s,
                        i32 *q, int rows, int cols) {
  for (int j = 0; j < cols; ++j)
    s[j] = 0;
  for (int i = 0; i < rows; ++i) {
    i32 qi = 0;
    for (int j = 0; j < cols; ++j) {
      i32 v = a[i * cols + j];
      s[j] += r[i] * v;
      qi += v * p[j];
    }
    q[i] = qi;
  }
}
