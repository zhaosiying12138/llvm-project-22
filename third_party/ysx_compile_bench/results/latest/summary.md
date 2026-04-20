# YSX Compile Benchmark Summary

- Generated: 2026-04-20T01:42:22.933230+00:00
- Benchmarks: 20
- Iterations per benchmark/compiler: 30
- YSX median across benchmarks: 15.047 ms
- RISCV median across benchmarks: 18.493 ms
- Median YSX/RISCV per-benchmark ratio: 0.829x

| Benchmark | Suite | YSX ms | RISCV ms | YSX/RISCV | YSX .o | RISCV .o |
|---|---|---|---|---|---|---|
| atax_i32 | polybench_style | 14.808 | 18.612 | 0.796x | 1568 | 1608 |
| bicg_i32 | polybench_style | 15.117 | 17.452 | 0.866x | 1560 | 1600 |
| bitmix_sha_style | embench_style | 14.256 | 18.455 | 0.772x | 1328 | 1360 |
| convolution2d_i32 | polybench_style | 21.957 | 24.484 | 0.897x | 1816 | 1856 |
| crc32_slice | embench_style | 14.769 | 18.760 | 0.787x | 1360 | 1400 |
| dijkstra_i32 | embench_style | 17.067 | 20.086 | 0.850x | 2080 | 2112 |
| doitgen_i32 | polybench_style | 16.284 | 19.385 | 0.840x | 1808 | 1848 |
| fir_i32 | embench_style | 15.595 | 16.941 | 0.921x | 1672 | 1632 |
| fixed_butterfly_i32 | embench_style | 18.010 | 20.041 | 0.899x | 1928 | 1968 |
| floyd_warshall_i32 | polybench_style | 14.052 | 17.081 | 0.823x | 1744 | 1784 |
| gemm_i32 | polybench_style | 14.815 | 17.715 | 0.836x | 1728 | 1768 |
| insertion_sort | embench_style | 12.665 | 16.675 | 0.759x | 1496 | 1536 |
| jacobi_1d_i32 | polybench_style | 15.673 | 17.941 | 0.874x | 1504 | 1544 |
| levenshtein_i32 | embench_style | 17.086 | 20.449 | 0.836x | 1968 | 1952 |
| matmul_i32 | embench_style | 13.866 | 16.861 | 0.822x | 1352 | 1392 |
| montgomery64 | embench_style | 12.280 | 15.826 | 0.776x | 1440 | 1480 |
| mvt_i32 | polybench_style | 14.977 | 19.402 | 0.772x | 1408 | 1448 |
| seidel_2d_i32 | polybench_style | 17.168 | 20.967 | 0.819x | 1720 | 1792 |
| state_machine | embench_style | 15.407 | 17.950 | 0.858x | 2472 | 2512 |
| trisolv_i32 | polybench_style | 14.415 | 18.531 | 0.778x | 1496 | 1488 |
