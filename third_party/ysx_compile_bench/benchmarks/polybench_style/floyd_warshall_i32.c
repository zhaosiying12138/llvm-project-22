typedef int i32;

void ysx_bench_floyd_warshall_i32(i32 *path, int n) {
  for (int k = 0; k < n; ++k) {
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        i32 through = path[i * n + k] + path[k * n + j];
        if (through < path[i * n + j])
          path[i * n + j] = through;
      }
    }
  }
}
