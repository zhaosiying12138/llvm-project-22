typedef int i32;

void ysx_bench_convolution2d_i32(const i32 *input, const i32 *kernel,
                                 i32 *output, int rows, int cols) {
  for (int i = 1; i + 1 < rows; ++i) {
    for (int j = 1; j + 1 < cols; ++j) {
      i32 acc = 0;
      for (int ki = -1; ki <= 1; ++ki)
        for (int kj = -1; kj <= 1; ++kj)
          acc += input[(i + ki) * cols + (j + kj)] *
                 kernel[(ki + 1) * 3 + (kj + 1)];
      output[i * cols + j] = acc;
    }
  }
}
