typedef int i32;

void ysx_bench_fixed_butterfly_i32(i32 *real, i32 *imag, int stages,
                                   int span) {
  for (int s = 0; s < stages; ++s) {
    int step = 1 << s;
    for (int base = 0; base < span; base += step << 1) {
      for (int j = 0; j < step; ++j) {
        int a = base + j;
        int b = a + step;
        i32 wr = 256 - ((j * 17 + s * 3) & 255);
        i32 wi = (j * 29 + s * 11) & 255;
        i32 tr = (real[b] * wr - imag[b] * wi) >> 8;
        i32 ti = (real[b] * wi + imag[b] * wr) >> 8;
        i32 ar = real[a];
        i32 ai = imag[a];
        real[a] = ar + tr;
        imag[a] = ai + ti;
        real[b] = ar - tr;
        imag[b] = ai - ti;
      }
    }
  }
}
