# YSX Compile Benchmark Summary

- Generated: 2026-04-20T01:44:56.552480+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 14.725 ms
- RISCV median across benchmarks: 18.001 ms
- Median YSX compile-time improvement over RISCV: 17.31%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 15.062 | 18.213 | 17.30% | 1568 | 1608 |
| bicg_i32 | polybench_style | 14.492 | 18.634 | 22.23% | 1560 | 1600 |
| bitmix_sha_style | embench_style | 13.984 | 17.004 | 17.76% | 1328 | 1360 |
| convolution2d_i32 | polybench_style | 20.519 | 23.304 | 11.95% | 1816 | 1856 |
| crc32_slice | embench_style | 14.755 | 18.137 | 18.65% | 1360 | 1400 |
| dijkstra_i32 | embench_style | 16.000 | 20.097 | 20.38% | 2080 | 2112 |
| doitgen_i32 | polybench_style | 15.854 | 18.609 | 14.80% | 1808 | 1848 |
| fir_i32 | embench_style | 14.361 | 17.349 | 17.22% | 1672 | 1632 |
| fixed_butterfly_i32 | embench_style | 18.662 | 20.788 | 10.23% | 1928 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.404 | 16.832 | 20.36% | 1744 | 1784 |
| gemm_i32 | polybench_style | 14.501 | 17.518 | 17.22% | 1728 | 1768 |
| insertion_sort | embench_style | 13.052 | 16.466 | 20.73% | 1496 | 1536 |
| jacobi_1d_i32 | polybench_style | 14.695 | 18.536 | 20.72% | 1504 | 1544 |
| levenshtein_i32 | embench_style | 18.729 | 20.848 | 10.17% | 1968 | 1952 |
| matmul_i32 | embench_style | 15.058 | 17.223 | 12.57% | 1352 | 1392 |
| montgomery64 | embench_style | 12.264 | 16.617 | 26.19% | 1440 | 1480 |
| mvt_i32 | polybench_style | 14.906 | 17.729 | 15.92% | 1408 | 1448 |
| seidel_2d_i32 | polybench_style | 18.010 | 20.073 | 10.28% | 1720 | 1792 |
| state_machine | embench_style | 14.061 | 17.865 | 21.29% | 2472 | 2512 |
| trisolv_i32 | polybench_style | 14.315 | 17.315 | 17.33% | 1496 | 1488 |
