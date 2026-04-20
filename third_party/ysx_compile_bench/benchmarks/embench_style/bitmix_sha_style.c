typedef unsigned int u32;

static u32 rotl32(u32 x, unsigned n) {
  return (x << n) | (x >> (32u - n));
}

u32 ysx_bench_bitmix_sha_style(const u32 *input, unsigned blocks) {
  u32 a = 0x6a09e667u;
  u32 b = 0xbb67ae85u;
  u32 c = 0x3c6ef372u;
  u32 d = 0xa54ff53au;
  for (unsigned i = 0; i < blocks; ++i) {
    u32 w = input[i] + (i * 0x9e3779b9u);
    u32 t0 = rotl32(a ^ w, 5) + (b & c);
    u32 t1 = rotl32(d + w, 11) ^ (a | c);
    d = c + t0;
    c = b ^ t1;
    b = a + rotl32(t0, 17);
    a = t1 + 0x7f4a7c15u;
  }
  return a ^ b ^ c ^ d;
}
