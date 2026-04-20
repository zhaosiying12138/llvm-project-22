#!/usr/bin/env python3
import argparse
import csv
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


YSX_FLAGS = [
    "--target=ysx64-unknown-elf",
    "-march=rv64ima",
    "-mabi=lp64",
    "-O2",
    "-ffreestanding",
    "-fno-builtin",
]
RISCV_FLAGS = [
    "--target=riscv64-unknown-elf",
    "-march=rv64ima",
    "-mabi=lp64",
    "-O2",
    "-ffreestanding",
    "-fno-builtin",
]
BAD_ASM_RE = re.compile(
    r"(^|\s)(c\.|vset|vle|vse|vadd|vsub|vmul|vdiv|vf|fadd|fsub|fmul|fdiv|"
    r"fld|fsd|flw|fsw|fmv|fcvt|fsqrt|fsgnj|csrr|csrw|mret|sret|wfi|sfence|"
    r"fence\.i)\b"
)
BAD_REG_RE = re.compile(r"\b(fa[0-7]|fs[0-9]+|ft[0-9]+|f[0-9]+|v[0-9]+)\b")


def run(cmd, cwd=None, env=None, check=True, capture=True):
    kwargs = {
        "cwd": cwd,
        "env": env,
        "text": True,
        "check": False,
    }
    if capture:
        kwargs.update({"stdout": subprocess.PIPE, "stderr": subprocess.PIPE})
    proc = subprocess.run(cmd, **kwargs)
    if check and proc.returncode != 0:
        stdout = proc.stdout or ""
        stderr = proc.stderr or ""
        raise RuntimeError(
            "command failed: {}\nstdout:\n{}\nstderr:\n{}".format(
                " ".join(map(str, cmd)), stdout, stderr
            )
        )
    return proc


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--bench-root", required=True)
    parser.add_argument("--mode", choices=["quick", "full"], default="full")
    return parser.parse_args()


def real_size(path):
    resolved = Path(path).resolve()
    return resolved.stat().st_size


def tool_version(cmd):
    proc = run(cmd, check=False)
    text = ((proc.stdout or "") + (proc.stderr or "")).strip().splitlines()
    return text[0] if text else ""


def cmake_cache_subset(build_dir):
    cache_path = Path(build_dir) / "CMakeCache.txt"
    keys = {
        "CMAKE_BUILD_TYPE",
        "CMAKE_C_COMPILER",
        "CMAKE_CXX_COMPILER",
        "LLVM_CCACHE_BUILD",
        "LLVM_ENABLE_PROJECTS",
        "LLVM_TARGETS_TO_BUILD",
    }
    values = {}
    if not cache_path.exists():
        return values
    for line in cache_path.read_text(errors="ignore").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        name_type, value = line.split("=", 1)
        name = name_type.split(":", 1)[0]
        if name in keys:
            values[name] = value
    return values


def ldd_footprint(path):
    if not shutil.which("ldd"):
        return {"bytes": 0, "libraries": []}
    proc = run(["ldd", str(path)], check=False)
    libs = []
    total = 0
    for line in (proc.stdout or "").splitlines():
        parts = line.strip().split("=>")
        candidate = None
        if len(parts) == 2:
            candidate = parts[1].strip().split()[0]
        elif line.strip().startswith("/"):
            candidate = line.strip().split()[0]
        if candidate and Path(candidate).exists():
            size = Path(candidate).stat().st_size
            libs.append({"path": candidate, "bytes": size})
            total += size
    return {"bytes": total, "libraries": libs}


def compiler_info(label, clang, required, forbidden):
    if not Path(clang).exists():
        raise RuntimeError(f"{label} clang does not exist: {clang}")
    version = run([clang, "--version"]).stdout.strip().splitlines()
    targets = run([clang, "--print-targets"]).stdout
    for target in required:
        if target not in targets:
            raise RuntimeError(f"{label} clang does not advertise {target}")
    for target in forbidden:
        if target in targets:
            raise RuntimeError(f"{label} clang unexpectedly advertises {target}")
    return {
        "path": str(Path(clang).resolve()),
        "version": version,
        "targets_excerpt": [line for line in targets.splitlines() if "riscv" in line or "ysx" in line],
        "executable_bytes": real_size(clang),
        "linked_shared_libraries": ldd_footprint(clang),
    }


def ensure_riscv_build(repo_root):
    default_build = Path("/home/zhaosiying/codebase/compiler/build_riscv_only_22_1_3_host_llvm")
    build_dir = Path(os.environ.get("RISCV_BUILD_DIR", default_build))
    clang = Path(os.environ.get("RISCV_CLANG", build_dir / "bin" / "clang"))
    if clang.exists():
        return clang

    source_dir = Path(os.environ.get("LLVM_SOURCE_DIR", repo_root / "llvm"))
    cc = os.environ.get(
        "CMAKE_C_COMPILER",
        "/home/zhaosiying/codebase/software/LLVM-19.1.3-Linux-X64/bin/clang",
    )
    cxx = os.environ.get(
        "CMAKE_CXX_COMPILER",
        "/home/zhaosiying/codebase/software/LLVM-19.1.3-Linux-X64/bin/clang++",
    )
    ccache = "ON" if shutil.which("ccache") else "OFF"
    build_dir.mkdir(parents=True, exist_ok=True)
    run(
        [
            "cmake",
            "-S",
            str(source_dir),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DLLVM_ENABLE_PROJECTS=clang;lld",
            "-DLLVM_TARGETS_TO_BUILD=RISCV",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DLLVM_CCACHE_BUILD={ccache}",
        ],
        capture=False,
    )
    run(["ninja", "-C", str(build_dir), "clang", "lld", "llvm-objdump", "llvm-size"], capture=False)
    return clang


def load_benchmarks(bench_root, mode):
    manifest = json.loads((bench_root / "benchmarks" / "manifest.json").read_text())
    benches = manifest["benchmarks"]
    if mode == "quick":
        keep = {"crc32_slice", "matmul_i32", "atax_i32", "jacobi_1d_i32"}
        benches = [b for b in benches if b["name"] in keep]
    return benches


def check_asm(compiler, flags, source, out_dir):
    asm_path = out_dir / (source.stem + ".s")
    cmd = [str(compiler)] + flags + ["-S", str(source), "-o", str(asm_path)]
    run(cmd)
    bad = []
    for line in asm_path.read_text(errors="ignore").splitlines():
        text = line.strip().lower()
        if not text or text.startswith(".") or text.endswith(":") or text.startswith("#"):
            continue
        if BAD_ASM_RE.search(text) or BAD_REG_RE.search(text):
            bad.append(line.strip())
    if bad:
        raise RuntimeError(f"non-rv64ima assembly in {source}: {bad[:8]}")
    return asm_path


def timed_compile(compiler, flags, source, obj_path):
    metrics_path = obj_path.with_suffix(".time")
    cmd = [
        "/usr/bin/time",
        "-f",
        "wall=%e\nuser=%U\nsys=%S\nrss_kb=%M",
        "-o",
        str(metrics_path),
        str(compiler),
    ] + flags + ["-c", str(source), "-o", str(obj_path)]
    start = time.perf_counter()
    run(cmd)
    perf_wall = time.perf_counter() - start
    parsed = {}
    for line in metrics_path.read_text().splitlines():
        key, value = line.split("=", 1)
        parsed[key] = float(value)
    parsed["wall_perf"] = perf_wall
    parsed["object_bytes"] = obj_path.stat().st_size
    return parsed


def summarize(values):
    return {
        "median": statistics.median(values),
        "mean": statistics.fmean(values),
        "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
        "min": min(values),
        "max": max(values),
    }


def write_csv(path, rows):
    if not rows:
        return
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def markdown_table(headers, rows):
    lines = ["| " + " | ".join(headers) + " |"]
    lines.append("|" + "|".join(["---"] * len(headers)) + "|")
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def generate_reports(results_dir, bench_root, env_info, sample_rows, summary_rows):
    raw_csv = results_dir / "raw_samples.csv"
    summary_csv = results_dir / "summary.csv"
    summary_json = results_dir / "summary.json"
    environment_json = results_dir / "environment.json"
    write_csv(raw_csv, sample_rows)
    write_csv(summary_csv, summary_rows)
    summary_json.write_text(json.dumps({"benchmarks": summary_rows}, indent=2, ensure_ascii=False) + "\n")
    environment_json.write_text(json.dumps(env_info, indent=2, ensure_ascii=False) + "\n")

    aggregate = {}
    for compiler in ["YSX", "RISCV"]:
        vals = [float(r["wall_median_s"]) for r in summary_rows if r["compiler"] == compiler]
        aggregate[compiler] = summarize(vals)
    ratios = []
    paired = {}
    for row in summary_rows:
        paired.setdefault(row["benchmark"], {})[row["compiler"]] = row
    for name, compilers in paired.items():
        if "YSX" in compilers and "RISCV" in compilers:
            y = float(compilers["YSX"]["wall_median_s"])
            r = float(compilers["RISCV"]["wall_median_s"])
            ratios.append(y / r if r else 0.0)
    ratio_summary = summarize(ratios)
    improvement_summary = {
        key: 100.0 * (1.0 - value) for key, value in ratio_summary.items()
    }
    benchmark_count = len(paired)

    rows = []
    for name in sorted(paired):
        y = paired[name]["YSX"]
        r = paired[name]["RISCV"]
        ratio = float(y["wall_median_s"]) / float(r["wall_median_s"])
        improvement = 100.0 * (1.0 - ratio)
        rows.append([
            name,
            y["suite"],
            f'{float(y["wall_median_s"]) * 1000:.3f}',
            f'{float(r["wall_median_s"]) * 1000:.3f}',
            f"{improvement:.2f}%",
            y["object_bytes"],
            r["object_bytes"],
        ])

    summary_md = results_dir / "summary.md"
    summary_md.write_text(
        "# YSX Compile Benchmark Summary\n\n"
        f"- Generated: {env_info['generated_at']}\n"
        f"- Benchmarks: {benchmark_count}\n"
        f"- Iterations per benchmark/compiler: {env_info['iterations']}\n"
        f"- YSX median across benchmarks: {aggregate['YSX']['median'] * 1000:.3f} ms\n"
        f"- RISCV median across benchmarks: {aggregate['RISCV']['median'] * 1000:.3f} ms\n"
        f"- Median YSX compile-time improvement over RISCV: {improvement_summary['median']:.2f}%\n\n"
        + markdown_table(
            ["Benchmark", "Suite", "YSX ms", "RISCV ms", "YSX improvement", "YSX .o", "RISCV .o"],
            rows,
        )
        + "\n",
        encoding="utf-8",
    )

    ysx_bin = env_info["compilers"]["YSX"]["executable_bytes"]
    riscv_bin = env_info["compilers"]["RISCV"]["executable_bytes"]
    bin_delta = 100.0 * (1.0 - (ysx_bin / riscv_bin)) if riscv_bin else 0.0
    if bin_delta >= 0.0:
        bin_sentence = f"YSX-only clang 可执行文件比 RISCV-only clang 小约 `{bin_delta:.2f}%`。"
    else:
        bin_sentence = f"YSX-only clang 可执行文件比 RISCV-only clang 大约 `{-bin_delta:.2f}%`。"
    speed_delta = 100.0 * (1.0 - ratio_summary["median"])
    if speed_delta >= 0.0:
        speed_sentence = (
            f"按这个口径 YSX-only clang 的端到端编译时间约减少 `{speed_delta:.2f}%`。"
        )
    else:
        speed_sentence = (
            f"按这个口径 YSX-only clang 的端到端编译时间约增加 `{-speed_delta:.2f}%`。"
        )
    build_rows = []
    for compiler in ["YSX", "RISCV"]:
        cache = env_info["builds"].get(compiler, {})
        build_rows.append(
            [
                f"{compiler}-only",
                cache.get("LLVM_TARGETS_TO_BUILD", ""),
                cache.get("LLVM_ENABLE_PROJECTS", ""),
                cache.get("CMAKE_BUILD_TYPE", ""),
                cache.get("LLVM_CCACHE_BUILD", ""),
            ]
        )

    blog = bench_root / "blog.md"
    blog.write_text(
        "# YSX 后端裁剪后的 clang 编译耗时对比\n\n"
        "这组数据比较两个同为 LLVM 22.1.3 的 clang：一个只启用裁剪后的 YSX 后端，"
        "另一个只启用 RISCV 后端。每个测试都是一个独立 C 源文件，每次测量都单独启动"
        "一个 clang 进程并编译到 `.o`，因此结果包含进程启动、动态加载、前端、中端和目标"
        "后端代码生成的端到端成本。\n\n"
        "## 测试集\n\n"
        f"测试集放在 `third_party/ysx_compile_bench/benchmarks`，共 `{benchmark_count}` 个 C kernel。"
        "它由两类 clean-room C kernel 组成："
        "一类覆盖 Embench IoT 常见的嵌入式整数工作负载形态，另一类覆盖 PolyBench/C 常见的"
        "静态控制流数组/矩阵 kernel 形态。为保证目标范围清晰，这些程序都不依赖 libc、OS、"
        "线程、文件或浮点运行时，也不会使用向量 intrinsic。\n\n"
        "这些文件不是 SPEC CPU 源码，也不声称代表 SPEC CPU 成绩；它们只用于 C 编译过程的"
        "可复现实验。\n\n"
        "## 方法\n\n"
        f"- 生成时间：`{env_info['generated_at']}`\n"
        f"- 主机：`{env_info['host']}`\n"
        f"- 重复次数：每个测试、每个编译器 `{env_info['iterations']}` 次，先预热再测量\n"
        "- 缓存策略：暖缓存，减少 I/O 抖动，保留真实进程启动成本\n"
        "- 优化参数：`-O2 -ffreestanding -fno-builtin -c`\n"
        "- YSX target：`--target=ysx64-unknown-elf -march=rv64ima -mabi=lp64`\n"
        "- RISCV target：`--target=riscv64-unknown-elf -march=rv64ima -mabi=lp64`\n"
        "- 指令范围检查：生成 assembly 后扫描并拒绝 FP、V、C、特权/system 等非 rv64ima 指令\n\n"
        "## 构建配置\n\n"
        + markdown_table(
            ["Compiler", "LLVM targets", "Projects", "Build type", "CCache"],
            build_rows,
        )
        + "\n\n"
        "## 编译器体积\n\n"
        + markdown_table(
            ["Compiler", "clang bytes", "linked shared-library bytes"],
            [
                [
                    "YSX-only",
                    str(ysx_bin),
                    str(env_info["compilers"]["YSX"]["linked_shared_libraries"]["bytes"]),
                ],
                [
                    "RISCV-only",
                    str(riscv_bin),
                    str(env_info["compilers"]["RISCV"]["linked_shared_libraries"]["bytes"]),
                ],
            ],
        )
        + f"\n\n{bin_sentence}"
        "这不会单独解释全部耗时差异，但它会影响进程启动、代码页加载和指令缓存压力。\n\n"
        "## 编译耗时结果\n\n"
        + markdown_table(
            ["Benchmark", "Suite", "YSX median ms", "RISCV median ms", "YSX 提升", "YSX .o", "RISCV .o"],
            rows,
        )
        + "\n\n"
        f"跨测试的 per-benchmark median 口径下，YSX 相对 RISCV 的编译耗时性能提升为 "
        f"`{improvement_summary['median']:.2f}%`。{speed_sentence}"
        "更完整的原始样本在 `results/latest/raw_samples.csv`，汇总在 "
        "`results/latest/summary.csv` 和 `results/latest/summary.json`。\n\n"
        "## 差异来源分析\n\n"
        "这次对比的前端和大部分中端是共享的，所以耗时差异主要来自目标相关部分。"
        "如果实测显示 YSX 更快，合理解释包括：\n\n"
        "- YSX-only 构建只注册一个 rv64ima 目标，目标枚举、target lookup 和后端初始化面更小。\n"
        "- 裁剪后的 YSX 后端删除了 RV32、浮点、压缩、向量、vendor 扩展、GlobalISel、复杂调度"
        "和大量不支持 feature 的表/分支。\n"
        "- driver 和 target feature 解析只接受固定 rv64ima/lp64 组合，避免 RISCV 后端需要保留的"
        "多 ABI、多扩展、多 profile 兼容路径。\n"
        "- 指令选择、lowering、MC 和伪指令定义更少，编译小型 C 文件时会减少代码路径和数据表访问。\n\n"
        "因此，YSX 可能获得的收益不是来自生成代码运行得更快，而是来自编译器自身要加载和执行的目标后端逻辑更少。"
        "如果某次机器上没有观察到明显收益，应优先看共享前端/中端成本、系统调度噪声和二进制布局差异。\n\n"
        "## 限制\n\n"
        "- 数据只代表这台机器、这个构建配置和这组 C 编译负载。\n"
        "- 暖缓存测量不能完全隔离冷启动页面加载成本。\n"
        "- 单个 C 文件编译时间很短，系统调度和 WSL2 环境会带来噪声，所以报告使用中位数。\n"
        "- 这不是运行时 benchmark，也不是 SPEC CPU benchmark。\n"
        "- 两个 clang 都仍共享大量 Clang/LLVM 前端和中端代码，因此目标后端裁剪的收益会被共享成本稀释。\n",
        encoding="utf-8",
    )


def main():
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    bench_root = Path(args.bench_root).resolve()
    iterations = int(os.environ.get("YSX_BENCH_ITERATIONS", "30"))
    if args.mode == "quick":
        iterations = int(os.environ.get("YSX_BENCH_QUICK_ITERATIONS", "3"))
    if iterations <= 0:
        raise RuntimeError("iteration count must be positive")

    default_ysx = Path("/home/zhaosiying/codebase/compiler/build_ysx_only_host_llvm/bin/clang")
    ysx_clang = Path(os.environ.get("YSX_CLANG", default_ysx))
    riscv_clang = ensure_riscv_build(repo_root)
    objdump = Path(os.environ.get("LLVM_OBJDUMP", ysx_clang.parent / "llvm-objdump"))
    if not objdump.exists():
        objdump = Path(shutil.which("llvm-objdump") or "")

    ysx_info = compiler_info("YSX", str(ysx_clang), ["ysx64"], ["riscv64"])
    riscv_info = compiler_info("RISCV", str(riscv_clang), ["riscv64"], ["ysx64"])
    if ysx_info["version"][0].split()[2] != riscv_info["version"][0].split()[2]:
        raise RuntimeError("compiler version mismatch")

    benches = load_benchmarks(bench_root, args.mode)
    results_dir = bench_root / "results" / "latest"
    work_dir = bench_root / "results" / "work"
    if results_dir.exists():
        shutil.rmtree(results_dir)
    if work_dir.exists():
        shutil.rmtree(work_dir)
    results_dir.mkdir(parents=True)
    work_dir.mkdir(parents=True)

    env_info = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "mode": args.mode,
        "iterations": iterations,
        "host": platform.platform(),
        "python": sys.version.split()[0],
        "repo_root": str(repo_root),
        "bench_root": str(bench_root),
        "flags": {"YSX": YSX_FLAGS, "RISCV": RISCV_FLAGS},
        "compilers": {"YSX": ysx_info, "RISCV": riscv_info},
        "builds": {
            "YSX": cmake_cache_subset(ysx_clang.parent.parent),
            "RISCV": cmake_cache_subset(riscv_clang.parent.parent),
        },
        "tools": {
            "cmake": tool_version(["cmake", "--version"]),
            "ninja": tool_version(["ninja", "--version"]),
            "time": "/usr/bin/time",
            "llvm_objdump": str(objdump) if objdump else "",
        },
    }

    sample_rows = []
    summary_rows = []
    compilers = [
        ("YSX", ysx_clang, YSX_FLAGS),
        ("RISCV", riscv_clang, RISCV_FLAGS),
    ]

    for bench in benches:
        source = bench_root / "benchmarks" / bench["path"]
        if not source.exists():
            raise RuntimeError(f"missing benchmark source: {source}")
        for compiler_name, compiler, flags in compilers:
            out_dir = work_dir / compiler_name.lower() / bench["name"]
            out_dir.mkdir(parents=True, exist_ok=True)
            check_asm(compiler, flags, source, out_dir)
            warm_obj = out_dir / "warmup.o"
            timed_compile(compiler, flags, source, warm_obj)
            samples = []
            for i in range(iterations):
                obj_path = out_dir / f"sample-{i}.o"
                metrics = timed_compile(compiler, flags, source, obj_path)
                row = {
                    "benchmark": bench["name"],
                    "suite": bench["suite"],
                    "compiler": compiler_name,
                    "iteration": i,
                    "wall_s": f'{metrics["wall_perf"]:.9f}',
                    "time_wall_s": f'{metrics["wall"]:.6f}',
                    "user_s": f'{metrics["user"]:.6f}',
                    "sys_s": f'{metrics["sys"]:.6f}',
                    "rss_kb": int(metrics["rss_kb"]),
                    "object_bytes": int(metrics["object_bytes"]),
                }
                sample_rows.append(row)
                samples.append(row)

            walls = [float(r["wall_s"]) for r in samples]
            users = [float(r["user_s"]) for r in samples]
            syss = [float(r["sys_s"]) for r in samples]
            rss = [int(r["rss_kb"]) for r in samples]
            objs = [int(r["object_bytes"]) for r in samples]
            s = summarize(walls)
            summary_rows.append(
                {
                    "benchmark": bench["name"],
                    "suite": bench["suite"],
                    "compiler": compiler_name,
                    "wall_median_s": f'{s["median"]:.9f}',
                    "wall_mean_s": f'{s["mean"]:.9f}',
                    "wall_stdev_s": f'{s["stdev"]:.9f}',
                    "wall_min_s": f'{s["min"]:.9f}',
                    "wall_max_s": f'{s["max"]:.9f}',
                    "user_median_s": f"{statistics.median(users):.6f}",
                    "sys_median_s": f"{statistics.median(syss):.6f}",
                    "rss_median_kb": str(int(statistics.median(rss))),
                    "object_bytes": str(int(statistics.median(objs))),
                    "source": str(source.relative_to(bench_root)),
                }
            )

    generate_reports(results_dir, bench_root, env_info, sample_rows, summary_rows)
    print(f"wrote results to {results_dir}")
    print(f"updated {bench_root / 'blog.md'}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
