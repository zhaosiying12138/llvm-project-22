typedef unsigned long long u64;

static u64 add_mod(u64 a, u64 b, u64 m) {
  u64 r = a + b;
  if (r < a || r >= m)
    r -= m;
  return r;
}

u64 ysx_bench_montgomery64(u64 a, u64 b, u64 m) {
  u64 result = 0;
  a %= m;
  while (b != 0) {
    if (b & 1u)
      result = add_mod(result, a, m);
    a = add_mod(a, a, m);
    b >>= 1;
  }
  return result;
}
