typedef int i32;

void ysx_bench_gemm_i32(const i32 *a, const i32 *b, i32 *c, int ni, int nj,
                        int nk, i32 alpha, i32 beta) {
  for (int i = 0; i < ni; ++i) {
    for (int j = 0; j < nj; ++j) {
      i32 sum = c[i * nj + j] * beta;
      for (int k = 0; k < nk; ++k)
        sum += alpha * a[i * nk + k] * b[k * nj + j];
      c[i * nj + j] = sum;
    }
  }
}
