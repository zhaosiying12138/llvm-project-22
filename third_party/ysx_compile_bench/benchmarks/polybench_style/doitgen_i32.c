typedef int i32;

void ysx_bench_doitgen_i32(i32 *a, const i32 *c4, i32 *sum, int nr, int nq,
                           int np) {
  for (int r = 0; r < nr; ++r) {
    for (int q = 0; q < nq; ++q) {
      for (int p = 0; p < np; ++p) {
        i32 acc = 0;
        for (int s = 0; s < np; ++s)
          acc += a[(r * nq + q) * np + s] * c4[s * np + p];
        sum[p] = acc;
      }
      for (int p = 0; p < np; ++p)
        a[(r * nq + q) * np + p] = sum[p];
    }
  }
}
