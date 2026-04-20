# YSX Compile Benchmark Summary

- Generated: 2026-04-20T14:55:04.612154+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 13.992 ms
- RISCV median across benchmarks: 15.327 ms
- YSX 相对 RISCV 的编译耗时性能提升: 9.34%

| Benchmark | Suite | YSX ms | RISCV ms | YSX improvement | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 14.552 | 15.783 | 7.80% | 1608 | 1608 |
| bicg_i32 | polybench_style | 14.181 | 14.954 | 5.17% | 1600 | 1600 |
| bitmix_sha_style | embench_style | 13.662 | 15.083 | 9.42% | 1368 | 1360 |
| convolution2d_i32 | polybench_style | 20.099 | 20.028 | -0.36% | 1856 | 1856 |
| crc32_slice | embench_style | 14.316 | 16.841 | 14.99% | 1400 | 1400 |
| dijkstra_i32 | embench_style | 15.406 | 16.966 | 9.20% | 2120 | 2112 |
| doitgen_i32 | polybench_style | 14.896 | 16.792 | 11.29% | 1848 | 1848 |
| fir_i32 | embench_style | 13.316 | 14.848 | 10.32% | 1712 | 1632 |
| fixed_butterfly_i32 | embench_style | 16.788 | 17.741 | 5.37% | 1968 | 1968 |
| floyd_warshall_i32 | polybench_style | 13.153 | 14.968 | 12.12% | 1784 | 1784 |
| gemm_i32 | polybench_style | 13.867 | 15.088 | 8.09% | 1768 | 1768 |
| insertion_sort | embench_style | 12.134 | 14.247 | 14.83% | 1536 | 1536 |
| jacobi_1d_i32 | polybench_style | 13.865 | 15.289 | 9.31% | 1544 | 1544 |
| levenshtein_i32 | embench_style | 16.869 | 17.740 | 4.91% | 2008 | 1952 |
| matmul_i32 | embench_style | 13.226 | 14.686 | 9.95% | 1392 | 1392 |
| montgomery64 | embench_style | 12.230 | 13.852 | 11.71% | 1480 | 1480 |
| mvt_i32 | polybench_style | 13.998 | 15.443 | 9.36% | 1448 | 1448 |
| seidel_2d_i32 | polybench_style | 16.334 | 17.834 | 8.41% | 1760 | 1792 |
| state_machine | embench_style | 13.299 | 15.365 | 13.44% | 2512 | 2512 |
| trisolv_i32 | polybench_style | 13.986 | 15.044 | 7.03% | 1536 | 1488 |
