typedef unsigned int u32;
typedef unsigned char u8;

static u32 crc_step(u32 crc, u8 byte) {
  crc ^= byte;
  for (int bit = 0; bit < 8; ++bit) {
    u32 mask = 0u - (crc & 1u);
    crc = (crc >> 1) ^ (0xedb88320u & mask);
  }
  return crc;
}

u32 ysx_bench_crc32_slice(const u8 *data, unsigned len) {
  u32 crc = 0xffffffffu;
  for (unsigned i = 0; i < len; ++i)
    crc = crc_step(crc, data[i]);
  return crc ^ 0xffffffffu;
}
