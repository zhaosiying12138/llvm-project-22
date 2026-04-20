# YSX Compile Benchmark Summary

- Generated: 2026-04-20T02:48:32.794941+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 13.698 ms
- RISCV median across benchmarks: 15.736 ms
- YSX 相对 RISCV 的编译耗时性能提升: 13.87%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 13.916 | 16.207 | 14.14% | 1608 | 1608 |
| bicg_i32 | polybench_style | 13.792 | 15.175 | 9.12% | 1600 | 1600 |
| bitmix_sha_style | embench_style | 12.805 | 15.762 | 18.76% | 1368 | 1360 |
| convolution2d_i32 | polybench_style | 19.627 | 21.066 | 6.83% | 1856 | 1856 |
| crc32_slice | embench_style | 13.934 | 17.204 | 19.01% | 1400 | 1400 |
| dijkstra_i32 | embench_style | 15.048 | 16.918 | 11.05% | 2120 | 2112 |
| doitgen_i32 | polybench_style | 14.847 | 16.791 | 11.58% | 1848 | 1848 |
| fir_i32 | embench_style | 13.623 | 15.058 | 9.53% | 1712 | 1632 |
| fixed_butterfly_i32 | embench_style | 16.911 | 18.068 | 6.41% | 1968 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.007 | 15.259 | 14.76% | 1784 | 1784 |
| gemm_i32 | polybench_style | 13.572 | 15.709 | 13.60% | 1768 | 1768 |
| insertion_sort | embench_style | 11.822 | 14.054 | 15.88% | 1536 | 1536 |
| jacobi_1d_i32 | polybench_style | 13.768 | 16.190 | 14.96% | 1544 | 1544 |
| levenshtein_i32 | embench_style | 15.740 | 17.600 | 10.57% | 2008 | 1952 |
| matmul_i32 | embench_style | 12.867 | 15.060 | 14.56% | 1392 | 1392 |
| montgomery64 | embench_style | 11.675 | 13.729 | 14.96% | 1480 | 1480 |
| mvt_i32 | polybench_style | 13.629 | 15.491 | 12.02% | 1448 | 1448 |
| seidel_2d_i32 | polybench_style | 15.935 | 18.211 | 12.50% | 1760 | 1792 |
| state_machine | embench_style | 13.159 | 15.422 | 14.67% | 2512 | 2512 |
| trisolv_i32 | polybench_style | 13.081 | 15.527 | 15.75% | 1536 | 1488 |
