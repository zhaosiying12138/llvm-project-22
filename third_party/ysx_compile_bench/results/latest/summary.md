# YSX Compile Benchmark Summary

- Generated: 2026-04-20T02:02:01.122749+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 14.326 ms
- RISCV median across benchmarks: 16.519 ms
- Median YSX compile-time improvement over RISCV: 11.61%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 15.084 | 16.518 | 8.68% | 1568 | 1608 |
| bicg_i32 | polybench_style | 14.212 | 16.519 | 13.96% | 1560 | 1600 |
| bitmix_sha_style | embench_style | 13.851 | 16.687 | 16.99% | 1328 | 1360 |
| convolution2d_i32 | polybench_style | 20.236 | 20.874 | 3.05% | 1816 | 1856 |
| crc32_slice | embench_style | 14.269 | 16.263 | 12.26% | 1360 | 1400 |
| dijkstra_i32 | embench_style | 15.934 | 17.296 | 7.88% | 2080 | 2112 |
| doitgen_i32 | polybench_style | 15.460 | 17.426 | 11.28% | 1808 | 1848 |
| fir_i32 | embench_style | 12.838 | 15.626 | 17.84% | 1672 | 1632 |
| fixed_butterfly_i32 | embench_style | 16.676 | 18.596 | 10.33% | 1928 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.323 | 15.520 | 14.16% | 1744 | 1784 |
| gemm_i32 | polybench_style | 14.384 | 15.745 | 8.64% | 1728 | 1768 |
| insertion_sort | embench_style | 12.472 | 14.616 | 14.66% | 1496 | 1536 |
| jacobi_1d_i32 | polybench_style | 14.669 | 16.291 | 9.96% | 1504 | 1544 |
| levenshtein_i32 | embench_style | 16.105 | 18.577 | 13.31% | 1968 | 1952 |
| matmul_i32 | embench_style | 13.762 | 15.609 | 11.83% | 1352 | 1392 |
| montgomery64 | embench_style | 12.813 | 14.200 | 9.77% | 1440 | 1480 |
| mvt_i32 | polybench_style | 14.855 | 16.725 | 11.18% | 1408 | 1448 |
| seidel_2d_i32 | polybench_style | 16.649 | 18.786 | 11.38% | 1720 | 1792 |
| state_machine | embench_style | 13.727 | 19.466 | 29.49% | 2472 | 2512 |
| trisolv_i32 | polybench_style | 13.612 | 16.266 | 16.32% | 1496 | 1488 |
