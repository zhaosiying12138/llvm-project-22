void ysx_negative_vector_inline(void) {
  __asm__ volatile("vsetvli zero, zero, e8, m1, ta, ma");
}
