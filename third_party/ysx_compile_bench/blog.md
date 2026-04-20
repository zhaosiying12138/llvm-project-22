# YSX 后端裁剪后的 clang 编译耗时对比

这组数据比较两个同为 LLVM 22.1.3 的 clang：一个只启用裁剪后的 YSX 后端，另一个只启用 RISCV 后端。每个测试都是一个独立 C 源文件，每次测量都单独启动一个 clang 进程并编译到 `.o`，因此结果包含进程启动、动态加载、前端、中端和目标后端代码生成的端到端成本。

## 测试集

测试集放在 `third_party/ysx_compile_bench/benchmarks`，共 `20` 个 C kernel。它由两类 clean-room C kernel 组成：一类覆盖 Embench IoT 常见的嵌入式整数工作负载形态，另一类覆盖 PolyBench/C 常见的静态控制流数组/矩阵 kernel 形态。为保证目标范围清晰，这些程序都不依赖 libc、OS、线程、文件或浮点运行时，也不会使用向量 intrinsic。

这些文件不是 SPEC CPU 源码，也不声称代表 SPEC CPU 成绩；它们只用于 C 编译过程的可复现实验。

## 方法

- 生成时间：`2026-04-20T02:02:01.122749+00:00`
- 主机：`Linux-6.5.0-microsoft-standard-WSL2+-x86_64-with-glibc2.38`
- 重复次数：每个测试、每个编译器 `30` 次，先预热再测量
- 缓存策略：暖缓存，减少 I/O 抖动，保留真实进程启动成本
- 优化参数：`-O2 -ffreestanding -fno-builtin -c`
- YSX target：`--target=ysx64-unknown-elf -march=rv64ima -mabi=lp64`
- RISCV target：`--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64`
- 指令范围检查：扫描 assembly，检查 object symbol table，并在工具可反汇编时补充 object disassembly；拒绝 FP、V、C、特权/system 等非 rv64ima 指令
- 负向自测：`7` 个 fixture 覆盖 include、libc、inline asm、FP、V/C 指令和 undefined symbol

## 构建配置

| Compiler | LLVM targets | Projects | Build type | CCache | Source rev |
|---|---|---|---|---|---|
| YSX-only | YSX | clang;lld | Release | OFF | a32416433c7d |
| RISCV-only | RISCV | clang;lld | Release | OFF | a32416433c7d |

## 编译器体积

| Compiler | clang bytes | linked shared-library bytes |
|---|---|---|
| YSX-only | 120640104 | 17712112 |
| RISCV-only | 132348016 | 17712112 |

YSX-only clang 可执行文件比 RISCV-only clang 小约 `8.85%`。这不会单独解释全部耗时差异，但它会影响进程启动、代码页加载和指令缓存压力。

## 编译耗时结果

| Benchmark | Suite | YSX median ms | RISCV median ms | YSX 提升 | YSX .o | RISCV .o |
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

跨测试的 per-benchmark median 口径下，YSX 相对 RISCV 的编译耗时性能提升为 `11.61%`。按这个口径 YSX-only clang 的端到端编译时间约减少 `11.61%`。更完整的原始样本在 `results/latest/raw_samples.csv`，汇总在 `results/latest/summary.csv` 和 `results/latest/summary.json`。

## 差异来源分析

这次对比的前端和大部分中端是共享的，所以耗时差异主要来自目标相关部分。如果实测显示 YSX 更快，合理解释包括：

- YSX-only 构建只注册一个 rv64ima 目标，目标枚举、target lookup 和后端初始化面更小。
- 裁剪后的 YSX 后端删除了 RV32、浮点、压缩、向量、vendor 扩展、GlobalISel、复杂调度和大量不支持 feature 的表/分支。
- driver 和 target feature 解析只接受固定 rv64ima/lp64 组合，避免 RISCV 后端需要保留的多 ABI、多扩展、多 profile 兼容路径。
- 指令选择、lowering、MC 和伪指令定义更少，编译小型 C 文件时会减少代码路径和数据表访问。

因此，YSX 可能获得的收益不是来自生成代码运行得更快，而是来自编译器自身要加载和执行的目标后端逻辑更少。如果某次机器上没有观察到明显收益，应优先看共享前端/中端成本、系统调度噪声和二进制布局差异。

## 限制

- 数据只代表这台机器、这个构建配置和这组 C 编译负载。
- 暖缓存测量不能完全隔离冷启动页面加载成本。
- 单个 C 文件编译时间很短，系统调度和 WSL2 环境会带来噪声，所以报告使用中位数。
- 这不是运行时 benchmark，也不是 SPEC CPU benchmark。
- 两个 clang 都仍共享大量 Clang/LLVM 前端和中端代码，因此目标后端裁剪的收益会被共享成本稀释。
