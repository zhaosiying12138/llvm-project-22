#define __global__ __attribute__((global))
#define __shared__ __attribute__((shared))
extern "C" __global__
void mcasi_digit_counters_kernel(unsigned int *out,
                                 const unsigned int *in,
                                 unsigned int selector) {
  __shared__ unsigned int shared_storage[256];

  unsigned int tid;
  asm volatile("mov.u32 %0, %%tid.x;" : "=r"(tid));

  if (tid < 256)
    shared_storage[tid] = in[tid];

  asm volatile("bar.sync 0;" ::: "memory");

  unsigned int *digit_counters[4];
  const unsigned int base = (tid & 31) * 4;

  digit_counters[0] = &shared_storage[(base + 0) & 255];
  digit_counters[1] = &shared_storage[(base + 1) & 255];
  digit_counters[2] = &shared_storage[(base + 2) & 255];
  digit_counters[3] = &shared_storage[(base + 3) & 255];

  const unsigned int i = selector & 3;
  unsigned int *p = digit_counters[i];
  const unsigned int value = *p;

  out[tid] = value + i;
}
