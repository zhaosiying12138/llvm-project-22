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
VERSION_RE = re.compile(r"clang version\s+(\S+)")
GIT_REV_RE = re.compile(r"\((?P<repo>.+?)\s+(?P<revision>[0-9a-f]{7,40})(?:[^\)]*)?\)")
INCLUDE_RE = re.compile(r"^\s*#\s*include\b", re.MULTILINE)
INLINE_ASM_RE = re.compile(r"\b(__asm__|__asm|asm)\b")
FLOAT_TYPE_RE = re.compile(r"\b(float|double|long\s+double|_Float[0-9A-Za-z]*|__fp16)\b")
FORBIDDEN_SOURCE_RE = re.compile(
    r"\b("
    r"printf|fprintf|sprintf|snprintf|puts|putchar|scanf|fscanf|sscanf|getchar|gets|"
    r"fread|fwrite|fopen|fclose|fseek|ftell|fflush|malloc|calloc|realloc|free|"
    r"exit|abort|system|getenv|time|clock|open|close|read|write|lseek|mmap|munmap|"
    r"pthread_create|pthread_join|pthread_mutex|fork|exec|FILE|stdin|stdout|stderr|argc|argv"
    r")\b"
)
RV64IMA_ALLOWED_MNEMONICS = {
    "add",
    "addi",
    "addiw",
    "addw",
    "amoadd.d",
    "amoadd.w",
    "amoand.d",
    "amoand.w",
    "amomax.d",
    "amomax.w",
    "amomaxu.d",
    "amomaxu.w",
    "amomin.d",
    "amomin.w",
    "amominu.d",
    "amominu.w",
    "amoor.d",
    "amoor.w",
    "amoswap.d",
    "amoswap.w",
    "amoxor.d",
    "amoxor.w",
    "and",
    "andi",
    "auipc",
    "beq",
    "beqz",
    "bge",
    "bgez",
    "bgeu",
    "bgt",
    "bgtu",
    "bgtz",
    "ble",
    "bleu",
    "blez",
    "blt",
    "bltu",
    "bltz",
    "bne",
    "bnez",
    "call",
    "div",
    "divu",
    "divuw",
    "divw",
    "fence",
    "j",
    "jal",
    "jalr",
    "jr",
    "la",
    "lb",
    "lbu",
    "ld",
    "lh",
    "lhu",
    "li",
    "lla",
    "lr.d",
    "lr.w",
    "lui",
    "lw",
    "lwu",
    "mul",
    "mulh",
    "mulhsu",
    "mulhu",
    "mulw",
    "mv",
    "neg",
    "negw",
    "nop",
    "not",
    "or",
    "ori",
    "rem",
    "remu",
    "remuw",
    "remw",
    "ret",
    "sb",
    "sc.d",
    "sc.w",
    "sd",
    "seqz",
    "sext.b",
    "sext.h",
    "sext.w",
    "sgtz",
    "sh",
    "sll",
    "slli",
    "slliw",
    "sllw",
    "slt",
    "slti",
    "sltiu",
    "sltu",
    "sltz",
    "snez",
    "sra",
    "srai",
    "sraiw",
    "sraw",
    "srl",
    "srli",
    "srliw",
    "srlw",
    "sub",
    "subw",
    "sw",
    "tail",
    "xor",
    "xori",
    "zext.b",
    "zext.h",
    "zext.w",
}
RV64IMA_ATOMIC_BASES = {
    "amoadd",
    "amoand",
    "amomax",
    "amomaxu",
    "amomin",
    "amominu",
    "amoor",
    "amoswap",
    "amoxor",
    "lr",
    "sc",
}
EXPECTED_NEGATIVE_FIXTURES = {
    "compressed_inline.c": "inline assembly",
    "floating_point.c": "floating-point type",
    "include_stdio.c": "preprocessor include",
    "inline_ecall.c": "inline assembly",
    "libc_printf.c": "host/runtime dependency spelling",
    "undefined_external.c": "undefined external symbols",
    "vector_inline.c": "inline assembly",
}
BAD_ISA_SNIPPETS = {
    "zbb_clz": "clz a0, a0",
    "zbb_ctz": "ctz a0, a0",
    "zbb_cpop": "cpop a0, a0",
    "zbb_andn": "andn a0, a0, a1",
    "zbb_orn": "orn a0, a0, a1",
    "zbb_xnor": "xnor a0, a0, a1",
    "zba_sh1add": "sh1add a0, a0, a1",
    "zbc_clmul": "clmul a0, a0, a1",
    "fp_fadd": "fadd.d fa0, fa0, fa1",
    "vector_vsetvli": "vsetvli zero, zero, e8, m1, ta, ma",
    "compressed_nop": "c.nop",
    "system_ecall": "ecall",
    "system_ebreak": "ebreak",
    "csr_read": "csrr a0, cycle",
}
GOOD_ISA_SNIPPETS = {
    "addi": "addi a0, a0, 1",
    "mul": "mul a0, a0, a1",
    "branch_alias": "blez a0, .Ldone",
    "ret_alias": "ret",
    "atomic": "amoadd.w.aqrl a0, a1, (a2)",
    "zext_byte_alias": "zext.b a0, a0",
    "zext_alias": "zext.w a0, a0",
}
BAD_REG_RE = re.compile(r"\b(fa[0-7]|fs[0-9]+|ft[0-9]+|f[0-9]+|v[0-9]+)\b")
DISASM_RE = re.compile(r"^\s*[0-9a-fA-F]+:\s*(?:[0-9a-fA-F]{2}\s+)*\s*([A-Za-z0-9_.]+)\b(.*)$")


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
    parser.add_argument("--self-test", action="store_true")
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
        "CMAKE_HOME_DIRECTORY",
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


def parse_compiler_version(version_lines):
    first = version_lines[0] if version_lines else ""
    version_match = VERSION_RE.search(first)
    git_match = GIT_REV_RE.search(first)
    return {
        "version_token": version_match.group(1) if version_match else "",
        "source_repository": git_match.group("repo") if git_match else "",
        "git_revision": git_match.group("revision") if git_match else "",
    }


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
        **parse_compiler_version(version),
        "targets_excerpt": [line for line in targets.splitlines() if "riscv" in line or "ysx" in line],
        "executable_bytes": real_size(clang),
        "linked_shared_libraries": ldd_footprint(clang),
    }


def find_llvm_tool(compiler, tool, env_name):
    env_path = os.environ.get(env_name)
    if env_path:
        path = Path(env_path)
        if path.exists():
            return path
        raise RuntimeError(f"{env_name} points to a missing tool: {path}")
    generic_env = os.environ.get(tool.upper().replace("-", "_"))
    if generic_env:
        path = Path(generic_env)
        if path.exists():
            return path
        raise RuntimeError(f"{tool} override points to a missing tool: {path}")
    candidate = Path(compiler).parent / tool
    if candidate.exists():
        return candidate
    fallback = shutil.which(tool)
    if fallback:
        return Path(fallback)
    raise RuntimeError(f"missing {tool} for {compiler}")


def compare_compiler_identity(ysx_info, riscv_info):
    if ysx_info["version_token"] != riscv_info["version_token"]:
        raise RuntimeError(
            "compiler version mismatch: "
            f"YSX={ysx_info['version_token']} RISCV={riscv_info['version_token']}"
        )
    if not ysx_info["git_revision"] or not riscv_info["git_revision"]:
        raise RuntimeError("compiler git revision missing from clang --version")
    if ysx_info["git_revision"] != riscv_info["git_revision"]:
        raise RuntimeError(
            "compiler source revision mismatch: "
            f"YSX={ysx_info['git_revision']} RISCV={riscv_info['git_revision']}"
        )


def resolve_host_compiler(env_name, fallback_env_name, tool):
    for candidate in (os.environ.get(env_name), os.environ.get(fallback_env_name)):
        if not candidate:
            continue
        path = Path(candidate)
        if path.exists():
            return path
        raise RuntimeError(f"{env_name}/{fallback_env_name} points to a missing tool: {path}")
    fallback = shutil.which(tool)
    if fallback:
        return Path(fallback)
    raise RuntimeError(f"missing host compiler {tool}; set {env_name} or {fallback_env_name}")


def ensure_target_build(repo_root, label, llvm_target, build_env, clang_env, default_dir_name):
    default_build = repo_root.parent / default_dir_name
    build_dir = Path(os.environ.get(build_env, default_build))
    clang = Path(os.environ.get(clang_env, build_dir / "bin" / "clang"))
    if clang.exists():
        return clang
    if os.environ.get(clang_env):
        raise RuntimeError(f"{clang_env} points to a missing clang: {clang}")

    source_dir = Path(os.environ.get("LLVM_SOURCE_DIR", repo_root / "llvm"))
    cc = resolve_host_compiler("CMAKE_C_COMPILER", "CC", "clang")
    cxx = resolve_host_compiler("CMAKE_CXX_COMPILER", "CXX", "clang++")
    ccache = "ON" if shutil.which("ccache") else "OFF"
    build_dir.mkdir(parents=True, exist_ok=True)
    print(f"configuring {label}-only LLVM build in {build_dir}", file=sys.stderr)
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
            f"-DLLVM_TARGETS_TO_BUILD={llvm_target}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DLLVM_CCACHE_BUILD={ccache}",
        ],
        capture=False,
    )
    run(["ninja", "-C", str(build_dir), "clang", "lld", "llvm-objdump", "llvm-size"], capture=False)
    return clang


def ensure_ysx_build(repo_root):
    return ensure_target_build(
        repo_root,
        "YSX",
        "YuShuXin",
        "YSX_BUILD_DIR",
        "YSX_CLANG",
        "build_ysx_only_host_llvm",
    )


def ensure_riscv_build(repo_root):
    return ensure_target_build(
        repo_root,
        "RISCV",
        "RISCV",
        "RISCV_BUILD_DIR",
        "RISCV_CLANG",
        "build_riscv_only_22_1_3_host_llvm",
    )


def load_benchmarks(bench_root, mode):
    manifest = json.loads((bench_root / "benchmarks" / "manifest.json").read_text())
    benches = manifest["benchmarks"]
    if mode == "quick":
        keep = {"crc32_slice", "matmul_i32", "atax_i32", "jacobi_1d_i32"}
        benches = [b for b in benches if b["name"] in keep]
    return benches


def git_revision(repo_root):
    proc = run(["git", "-C", str(repo_root), "rev-parse", "HEAD"], check=False)
    return (proc.stdout or "").strip()


def git_dirty(repo_root):
    proc = run(["git", "-C", str(repo_root), "status", "--porcelain"], check=False)
    return bool((proc.stdout or "").strip())


def strip_c_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def validate_source(source):
    raw = source.read_text(errors="ignore")
    text = strip_c_comments(raw)
    checks = [
        (INCLUDE_RE, "preprocessor include"),
        (INLINE_ASM_RE, "inline assembly"),
        (FLOAT_TYPE_RE, "floating-point type"),
        (FORBIDDEN_SOURCE_RE, "host/runtime dependency spelling"),
        (re.compile(r"\b__builtin_[A-Za-z0-9_]+\b"), "compiler builtin"),
    ]
    for pattern, reason in checks:
        match = pattern.search(text)
        if match:
            token = match.group(0).replace("\n", " ")
            raise RuntimeError(f"invalid benchmark source {source}: {reason}: {token}")


def canonical_mnemonic(mnemonic):
    mnemonic = mnemonic.lower()
    for suffix in (".aqrl", ".aq", ".rl"):
        if mnemonic.endswith(suffix):
            mnemonic = mnemonic[: -len(suffix)]
            break
    return mnemonic


def is_allowed_rv64ima_mnemonic(mnemonic):
    canonical = canonical_mnemonic(mnemonic)
    if canonical in RV64IMA_ALLOWED_MNEMONICS:
        return True
    parts = canonical.split(".")
    if len(parts) == 2 and parts[0] in RV64IMA_ATOMIC_BASES and parts[1] in {"w", "d"}:
        return True
    return False


def parse_asm_instruction(line):
    code = line.split("#", 1)[0].strip()
    if not code or code.startswith("."):
        return None, ""
    if code.endswith(":"):
        return None, ""
    if ":" in code:
        _, code = code.split(":", 1)
        code = code.strip()
        if not code:
            return None, ""
    parts = code.split(None, 1)
    if not parts:
        return None, ""
    return parts[0].rstrip(",").lower(), parts[1].lower() if len(parts) > 1 else ""


def check_instruction_text(text):
    bad = []
    for line in text.splitlines():
        mnemonic, operands = parse_asm_instruction(line)
        if not mnemonic:
            continue
        if not is_allowed_rv64ima_mnemonic(mnemonic) or BAD_REG_RE.search(operands):
            bad.append(line.strip())
    return bad


def check_asm(compiler, flags, source, out_dir):
    asm_path = out_dir / (source.stem + ".s")
    cmd = [str(compiler)] + flags + ["-S", str(source), "-o", str(asm_path)]
    run(cmd)
    bad = check_instruction_text(asm_path.read_text(errors="ignore"))
    if bad:
        raise RuntimeError(f"non-rv64ima assembly in {source}: {bad[:8]}")
    return asm_path


def compile_object(compiler, flags, source, obj_path):
    run([str(compiler)] + flags + ["-c", str(source), "-o", str(obj_path)])
    return obj_path


def check_undefined_symbols(objdump, obj_path):
    proc = run([str(objdump), "-t", str(obj_path)])
    undefined = []
    for line in proc.stdout.splitlines():
        if "*UND*" not in line and " UND " not in line:
            continue
        parts = line.split()
        if parts:
            undefined.append(parts[-1])
    if undefined:
        raise RuntimeError(f"undefined external symbols in {obj_path}: {undefined[:8]}")


def check_disassembly(objdump, obj_path, triple):
    proc = run(
        [str(objdump), f"--triple={triple}", "-d", "--no-show-raw-insn", str(obj_path)],
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            "llvm-objdump failed while validating {} with {}:\nstdout:\n{}\nstderr:\n{}".format(
                obj_path, triple, proc.stdout or "", proc.stderr or ""
            )
        )
    bad = []
    for line in proc.stdout.splitlines():
        match = DISASM_RE.match(line)
        if not match:
            continue
        mnemonic = match.group(1).lower()
        operands = match.group(2).lower()
        if not is_allowed_rv64ima_mnemonic(mnemonic) or BAD_REG_RE.search(operands):
            bad.append(line.strip())
    if bad:
        raise RuntimeError(f"non-rv64ima disassembly in {obj_path}: {bad[:8]}")


def validate_benchmark(compiler, flags, objdump, objdump_triple, source, out_dir):
    validate_source(source)
    check_asm(compiler, flags, source, out_dir)
    obj_path = out_dir / (source.stem + ".validation.o")
    compile_object(compiler, flags, source, obj_path)
    check_undefined_symbols(objdump, obj_path)
    check_disassembly(objdump, obj_path, objdump_triple)


def run_negative_self_tests(bench_root, compilers, work_dir):
    negative_dir = bench_root / "tests" / "negative"
    fixtures = sorted(negative_dir.glob("*.c"))
    if not fixtures:
        raise RuntimeError(f"missing negative fixtures under {negative_dir}")
    problems = []
    for fixture in fixtures:
        expected = EXPECTED_NEGATIVE_FIXTURES.get(fixture.name)
        if not expected:
            problems.append(f"{fixture.name}: missing expected rejection category")
            continue
        for compiler_name, compiler, flags, objdump, objdump_triple in compilers:
            out_dir = work_dir / "self-test" / compiler_name.lower() / fixture.stem
            out_dir.mkdir(parents=True, exist_ok=True)
            try:
                validate_benchmark(compiler, flags, objdump, objdump_triple, fixture, out_dir)
            except RuntimeError as exc:
                if expected not in str(exc):
                    problems.append(f"{compiler_name}:{fixture.name}: unexpected rejection: {exc}")
                continue
            problems.append(f"{compiler_name}:{fixture.name}: accepted")
    for name, snippet in BAD_ISA_SNIPPETS.items():
        if not check_instruction_text(f"\t{snippet}\n"):
            problems.append(f"bad ISA snippet accepted: {name}: {snippet}")
    for name, snippet in GOOD_ISA_SNIPPETS.items():
        bad = check_instruction_text(f"\t{snippet}\n")
        if bad:
            problems.append(f"good ISA snippet rejected: {name}: {bad}")
    if problems:
        raise RuntimeError(f"negative self-test failures: {problems}")


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
        f"- YSX 相对 RISCV 的编译耗时性能提升: {improvement_summary['median']:.2f}%\n\n"
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
        revision = env_info["compilers"][compiler].get("git_revision", "")
        build_rows.append(
            [
                f"{compiler}-only",
                cache.get("LLVM_TARGETS_TO_BUILD", ""),
                cache.get("LLVM_ENABLE_PROJECTS", ""),
                cache.get("CMAKE_BUILD_TYPE", ""),
                cache.get("LLVM_CCACHE_BUILD", ""),
                revision[:12],
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
        "- 指令范围检查：扫描 assembly，检查 object symbol table，并强制反汇编 object；"
        "拒绝 FP、V、C、特权/system 等非 rv64ima 指令\n"
        f"- 负向自测：`{env_info['negative_fixture_count']}` 个源码 fixture 加 "
        f"`{env_info['bad_isa_snippet_count']}` 个非法 ISA snippet，覆盖 include、libc、inline asm、FP、"
        "V/C/system/Zb/Zbc 指令和 undefined symbol\n\n"
        "## 构建配置\n\n"
        + markdown_table(
            ["Compiler", "LLVM targets", "Projects", "Build type", "CCache", "Source rev"],
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

    ysx_clang = ensure_ysx_build(repo_root)
    riscv_clang = ensure_riscv_build(repo_root)
    ysx_objdump = find_llvm_tool(ysx_clang, "llvm-objdump", "YSX_LLVM_OBJDUMP")
    riscv_objdump = find_llvm_tool(riscv_clang, "llvm-objdump", "RISCV_LLVM_OBJDUMP")

    ysx_info = compiler_info("YSX", str(ysx_clang), ["ysx64"], ["riscv64"])
    riscv_info = compiler_info("RISCV", str(riscv_clang), ["riscv64"], ["ysx64"])
    compare_compiler_identity(ysx_info, riscv_info)

    benches = load_benchmarks(bench_root, args.mode)
    results_dir = bench_root / "results" / "latest"
    work_dir = bench_root / "results" / "work"
    self_test_dir = bench_root / "results" / "self-test"
    if self_test_dir.exists():
        shutil.rmtree(self_test_dir)
    self_test_dir.mkdir(parents=True)
    negative_fixture_count = len(list((bench_root / "tests" / "negative").glob("*.c")))
    bad_isa_snippet_count = len(BAD_ISA_SNIPPETS)
    good_isa_snippet_count = len(GOOD_ISA_SNIPPETS)

    compilers = [
        ("YSX", ysx_clang, YSX_FLAGS, ysx_objdump, "ysx64"),
        ("RISCV", riscv_clang, RISCV_FLAGS, riscv_objdump, "riscv64"),
    ]
    run_negative_self_tests(bench_root, compilers, self_test_dir)
    if args.self_test:
        print(
            "negative self-tests passed using "
            f"{negative_fixture_count} fixtures and {bad_isa_snippet_count} bad ISA snippets"
        )
        return

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
        "repo_revision": git_revision(repo_root),
        "repo_dirty": git_dirty(repo_root),
        "bench_root": str(bench_root),
        "negative_fixture_count": negative_fixture_count,
        "bad_isa_snippet_count": bad_isa_snippet_count,
        "good_isa_snippet_count": good_isa_snippet_count,
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
            "ysx_llvm_objdump": str(ysx_objdump),
            "riscv_llvm_objdump": str(riscv_objdump),
        },
    }

    sample_rows = []
    summary_rows = []

    for bench in benches:
        source = bench_root / "benchmarks" / bench["path"]
        if not source.exists():
            raise RuntimeError(f"missing benchmark source: {source}")
        for compiler_name, compiler, flags, objdump, objdump_triple in compilers:
            out_dir = work_dir / compiler_name.lower() / bench["name"]
            out_dir.mkdir(parents=True, exist_ok=True)
            validate_benchmark(compiler, flags, objdump, objdump_triple, source, out_dir)
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
