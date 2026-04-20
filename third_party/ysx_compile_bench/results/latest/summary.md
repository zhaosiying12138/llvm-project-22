# YSX Compile Benchmark Summary

- Generated: 2026-04-20T03:06:12.189369+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 14.515 ms
- RISCV median across benchmarks: 16.290 ms
- YSX 相对 RISCV 的编译耗时性能提升: 10.29%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 14.339 | 16.028 | 10.54% | 1608 | 1608 |
| bicg_i32 | polybench_style | 13.975 | 15.135 | 7.67% | 1600 | 1600 |
| bitmix_sha_style | embench_style | 18.516 | 20.781 | 10.90% | 1368 | 1360 |
| convolution2d_i32 | polybench_style | 20.259 | 21.269 | 4.75% | 1856 | 1856 |
| crc32_slice | embench_style | 14.407 | 17.094 | 15.71% | 1400 | 1400 |
| dijkstra_i32 | embench_style | 15.558 | 17.297 | 10.05% | 2120 | 2112 |
| doitgen_i32 | polybench_style | 14.943 | 16.970 | 11.94% | 1848 | 1848 |
| fir_i32 | embench_style | 12.903 | 14.914 | 13.48% | 1712 | 1632 |
| fixed_butterfly_i32 | embench_style | 16.947 | 19.427 | 12.76% | 1968 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.863 | 14.847 | 6.63% | 1784 | 1784 |
| gemm_i32 | polybench_style | 14.025 | 15.033 | 6.71% | 1768 | 1768 |
| insertion_sort | embench_style | 15.854 | 16.553 | 4.22% | 1536 | 1536 |
| jacobi_1d_i32 | polybench_style | 14.623 | 15.602 | 6.27% | 1544 | 1544 |
| levenshtein_i32 | embench_style | 16.642 | 17.740 | 6.19% | 2008 | 1952 |
| matmul_i32 | embench_style | 13.323 | 19.540 | 31.82% | 1392 | 1392 |
| montgomery64 | embench_style | 16.668 | 14.292 | -16.62% | 1480 | 1480 |
| mvt_i32 | polybench_style | 13.708 | 15.849 | 13.51% | 1448 | 1448 |
| seidel_2d_i32 | polybench_style | 16.353 | 18.571 | 11.94% | 1760 | 1792 |
| state_machine | embench_style | 13.296 | 15.497 | 14.20% | 2512 | 2512 |
| trisolv_i32 | polybench_style | 13.974 | 15.483 | 9.75% | 1536 | 1488 |
