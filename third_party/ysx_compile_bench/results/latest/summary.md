# YSX Compile Benchmark Summary

- Generated: 2026-04-20T15:14:30.014521+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 14.464 ms
- RISCV median across benchmarks: 15.860 ms
- YSX 相对 RISCV 的编译耗时性能提升: 10.39%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 14.776 | 16.132 | 8.40% | 1608 | 1608 |
| bicg_i32 | polybench_style | 14.523 | 15.703 | 7.51% | 1600 | 1600 |
| bitmix_sha_style | embench_style | 16.176 | 15.012 | -7.76% | 1368 | 1360 |
| convolution2d_i32 | polybench_style | 19.986 | 20.443 | 2.24% | 1856 | 1856 |
| crc32_slice | embench_style | 14.404 | 16.597 | 13.21% | 1400 | 1400 |
| dijkstra_i32 | embench_style | 15.479 | 17.669 | 12.39% | 2120 | 2112 |
| doitgen_i32 | polybench_style | 15.939 | 17.078 | 6.67% | 1848 | 1848 |
| fir_i32 | embench_style | 13.151 | 14.835 | 11.35% | 1712 | 1632 |
| fixed_butterfly_i32 | embench_style | 16.648 | 18.677 | 10.86% | 1968 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.712 | 15.222 | 9.92% | 1784 | 1784 |
| gemm_i32 | polybench_style | 13.738 | 15.424 | 10.93% | 1768 | 1768 |
| insertion_sort | embench_style | 12.170 | 14.379 | 15.37% | 1536 | 1536 |
| jacobi_1d_i32 | polybench_style | 14.759 | 15.717 | 6.10% | 1544 | 1544 |
| levenshtein_i32 | embench_style | 16.883 | 18.377 | 8.13% | 2008 | 1952 |
| matmul_i32 | embench_style | 12.847 | 14.412 | 10.86% | 1392 | 1392 |
| montgomery64 | embench_style | 11.950 | 14.086 | 15.16% | 1480 | 1480 |
| mvt_i32 | polybench_style | 14.206 | 16.142 | 11.99% | 1448 | 1448 |
| seidel_2d_i32 | polybench_style | 16.195 | 17.904 | 9.54% | 1760 | 1792 |
| state_machine | embench_style | 13.045 | 16.002 | 18.48% | 2512 | 2512 |
| trisolv_i32 | polybench_style | 14.301 | 15.254 | 6.25% | 1536 | 1488 |
