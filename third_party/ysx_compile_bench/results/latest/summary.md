# YSX Compile Benchmark Summary

- Generated: 2026-04-20T02:05:23.500591+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 14.737 ms
- RISCV median across benchmarks: 17.028 ms
- Median YSX compile-time improvement over RISCV: 12.21%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 15.012 | 17.482 | 14.13% | 1568 | 1608 |
| bicg_i32 | polybench_style | 14.036 | 16.444 | 14.64% | 1560 | 1600 |
| bitmix_sha_style | embench_style | 18.834 | 15.932 | -18.21% | 1328 | 1360 |
| convolution2d_i32 | polybench_style | 21.909 | 22.010 | 0.46% | 1816 | 1856 |
| crc32_slice | embench_style | 15.185 | 17.205 | 11.74% | 1360 | 1400 |
| dijkstra_i32 | embench_style | 16.924 | 17.789 | 4.86% | 2080 | 2112 |
| doitgen_i32 | polybench_style | 16.466 | 18.795 | 12.39% | 1808 | 1848 |
| fir_i32 | embench_style | 13.958 | 15.780 | 11.55% | 1672 | 1632 |
| fixed_butterfly_i32 | embench_style | 17.098 | 19.542 | 12.50% | 1928 | 1968 |
| floyd_warshall_i32 | polybench_style | 14.451 | 16.373 | 11.74% | 1744 | 1784 |
| gemm_i32 | polybench_style | 14.599 | 16.245 | 10.13% | 1728 | 1768 |
| insertion_sort | embench_style | 12.984 | 15.145 | 14.27% | 1496 | 1536 |
| jacobi_1d_i32 | polybench_style | 14.832 | 17.201 | 13.77% | 1504 | 1544 |
| levenshtein_i32 | embench_style | 17.065 | 19.844 | 14.00% | 1968 | 1952 |
| matmul_i32 | embench_style | 14.394 | 19.806 | 27.32% | 1352 | 1392 |
| montgomery64 | embench_style | 13.769 | 15.515 | 11.25% | 1440 | 1480 |
| mvt_i32 | polybench_style | 14.588 | 16.855 | 13.45% | 1408 | 1448 |
| seidel_2d_i32 | polybench_style | 18.261 | 20.292 | 10.01% | 1720 | 1792 |
| state_machine | embench_style | 13.665 | 16.787 | 18.60% | 2472 | 2512 |
| trisolv_i32 | polybench_style | 14.641 | 16.643 | 12.03% | 1496 | 1488 |
