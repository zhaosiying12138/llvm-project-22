typedef unsigned int u32;
typedef unsigned char u8;

u32 ysx_bench_state_machine(const u8 *input, unsigned len) {
  u32 state = 0x12345678u;
  u32 score = 0;
  for (unsigned i = 0; i < len; ++i) {
    u8 ch = input[i];
    switch ((state ^ ch) & 7u) {
    case 0: state += ch + 17u; break;
    case 1: state ^= (u32)ch << 5; break;
    case 2: state = (state << 3) | (state >> 29); break;
    case 3: state -= ch * 13u; break;
    case 4: state += state >> 7; break;
    case 5: state ^= 0x9e3779b9u; break;
    case 6: state += (state << 2) ^ ch; break;
    default: state = (state >> 1) + ch; break;
    }
    score += state ^ (u32)i;
  }
  return score;
}
