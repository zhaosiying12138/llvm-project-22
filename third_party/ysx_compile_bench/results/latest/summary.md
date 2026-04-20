# YSX Compile Benchmark Summary

- Generated: 2026-04-20T02:18:13.468137+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 12.921 ms
- RISCV median across benchmarks: 14.735 ms
- Median YSX compile-time improvement over RISCV: 12.06%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 13.562 | 15.120 | 10.30% | 1568 | 1608 |
| bicg_i32 | polybench_style | 12.678 | 14.555 | 12.90% | 1560 | 1600 |
| bitmix_sha_style | embench_style | 12.293 | 14.322 | 14.17% | 1328 | 1360 |
| convolution2d_i32 | polybench_style | 18.838 | 19.940 | 5.53% | 1816 | 1856 |
| crc32_slice | embench_style | 13.556 | 14.938 | 9.25% | 1360 | 1400 |
| dijkstra_i32 | embench_style | 14.321 | 15.942 | 10.17% | 2080 | 2112 |
| doitgen_i32 | polybench_style | 14.478 | 16.017 | 9.61% | 1808 | 1848 |
| fir_i32 | embench_style | 11.993 | 13.913 | 13.80% | 1672 | 1632 |
| fixed_butterfly_i32 | embench_style | 15.751 | 17.730 | 11.16% | 1928 | 1968 |
| floyd_warshall_i32 | polybench_style | 12.551 | 14.355 | 12.57% | 1744 | 1784 |
| gemm_i32 | polybench_style | 13.049 | 14.215 | 8.20% | 1728 | 1768 |
| insertion_sort | embench_style | 11.140 | 13.867 | 19.66% | 1496 | 1536 |
| jacobi_1d_i32 | polybench_style | 12.793 | 14.732 | 13.16% | 1504 | 1544 |
| levenshtein_i32 | embench_style | 15.408 | 16.857 | 8.60% | 1968 | 1952 |
| matmul_i32 | embench_style | 11.746 | 13.669 | 14.07% | 1352 | 1392 |
| montgomery64 | embench_style | 11.125 | 12.839 | 13.35% | 1440 | 1480 |
| mvt_i32 | polybench_style | 13.573 | 15.344 | 11.54% | 1408 | 1448 |
| seidel_2d_i32 | polybench_style | 15.714 | 17.213 | 8.71% | 1720 | 1792 |
| state_machine | embench_style | 12.402 | 14.596 | 15.03% | 2472 | 2512 |
| trisolv_i32 | polybench_style | 12.424 | 14.737 | 15.70% | 1496 | 1488 |
