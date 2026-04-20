typedef unsigned int u32;
typedef unsigned char u8;

static u32 ysx_min3(u32 a, u32 b, u32 c) {
  u32 m = a < b ? a : b;
  return m < c ? m : c;
}

u32 ysx_bench_levenshtein_i32(const u8 *a, const u8 *b, u32 *row0, u32 *row1,
                              unsigned n, unsigned m) {
  for (unsigned j = 0; j <= m; ++j)
    row0[j] = j;
  for (unsigned i = 1; i <= n; ++i) {
    row1[0] = i;
    for (unsigned j = 1; j <= m; ++j) {
      u32 cost = a[i - 1] == b[j - 1] ? 0u : 1u;
      row1[j] = ysx_min3(row1[j - 1] + 1u, row0[j] + 1u,
                         row0[j - 1] + cost);
    }
    for (unsigned j = 0; j <= m; ++j)
      row0[j] = row1[j];
  }
  return row0[m];
}
