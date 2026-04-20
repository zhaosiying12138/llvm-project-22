typedef int i32;

void ysx_bench_insertion_sort(i32 *data, int n) {
  for (int i = 1; i < n; ++i) {
    i32 key = data[i];
    int j = i - 1;
    while (j >= 0 && data[j] > key) {
      data[j + 1] = data[j];
      --j;
    }
    data[j + 1] = key;
  }
}
