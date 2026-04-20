typedef int i32;

void ysx_bench_fir_i32(const i32 *input, const i32 *coeff, i32 *output,
                       int samples, int taps) {
  for (int i = 0; i < samples; ++i) {
    i32 acc = 0;
    for (int t = 0; t < taps; ++t) {
      int pos = i - t;
      if (pos >= 0)
        acc += input[pos] * coeff[t];
    }
    output[i] = acc >> 8;
  }
}
