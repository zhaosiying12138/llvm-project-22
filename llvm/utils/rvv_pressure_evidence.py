#!/usr/bin/env python3
"""Collect RISCV RVV softmax pressure evidence and optional qemu checks.

The script is independent from lit. It writes generated IR, assembly, pass
dumps, objects, and ELFs into a temporary directory unless --work-dir is given,
and every checked item is reported as PASS, FAIL, or SKIP.
"""

from __future__ import annotations

import argparse
import contextlib
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from typing import Iterable, NamedTuple


REPO_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPO_ROOT / "llvm/test/CodeGen/RISCV/rvv/Inputs/rvv-pressure-scale.py"
REALCASE = (
    REPO_ROOT
    / "llvm/test/CodeGen/RISCV/rvv/v-reg-pressure-cleaned-softmax-realcase.ll"
)

SOFTMAX_VLEN_FLAGS = [
    "-mtriple=riscv64",
    "-mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b",
    "-riscv-v-vector-bits-min=1024",
]
ADD_VLEN_FLAGS = SOFTMAX_VLEN_FLAGS
ADD_RUNTIME_FLAGS = [
    "-mtriple=riscv64",
    "-mattr=+v,+zvl1024b",
    "-riscv-v-vector-bits-min=1024",
]
SOFTMAX_RUNTIME_FLAGS = ADD_RUNTIME_FLAGS
HARNESS_MC_MATTR = "+v,+zvl1024b"
RUNTIME_OBJECT_FLAGS = [
    "-riscv-add-build-attributes=false",
    "-riscv-abi-attributes=false",
]
SOFTMAX_RUNTIME_INPUT = tuple(((i % 17) - 8) * 0.25 for i in range(128))
EXP2F_PROBE_INPUT = (-5.5, -2.3, -0.75, -0.125, 0.5)
EXP2F_POLY_DEGREE = 10

SPILL_RE = re.compile(r"\b[vu][sl][1248]r\.v\b")
VFEXP_RE = re.compile(r"\b(?:PseudoYUSHUXIN_VFEXP[A-Z0-9_]*|yushuxin\.vfexp)\b")
EXP2F_RE = re.compile(r"\bexp2f\b")
LOAD_RE = re.compile(r"\bPseudoVLE\d+_[A-Z0-9_]*\b")
REDUCTION_RE = re.compile(
    r"\b(?:PseudoV(?:F|FW)?RED[A-Z0-9_]*|v(?:f|fw)?red[a-z0-9_.]*)\b"
)


class Variant(NamedTuple):
    name: str
    flags: tuple[str, ...]


VARIANTS = [
    Variant("baseline", ()),
    Variant("stage1", ("-riscv-rvv-pressure-dag-sched",)),
    Variant(
        "stage1+2",
        ("-riscv-rvv-pressure-dag-sched", "-riscv-rvv-pressure-remat"),
    ),
]
RUNTIME_VARIANTS = [VARIANTS[0], VARIANTS[2]]


class Toolchain(NamedTuple):
    llc: Path
    llvm_mc: Path
    ld_lld: Path
    qemu: str | None


class Recorder:
    def __init__(self) -> None:
        self.counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}

    def emit(self, status: str, name: str, detail: str) -> None:
        self.counts[status] += 1
        print(f"{status}: {name}: {detail}")

    def pass_(self, name: str, detail: str) -> None:
        self.emit("PASS", name, detail)

    def fail(self, name: str, detail: str) -> None:
        self.emit("FAIL", name, detail)

    def skip(self, name: str, detail: str) -> None:
        self.emit("SKIP", name, detail)

    def summary(self) -> None:
        print(
            "SUMMARY: "
            f"PASS={self.counts['PASS']} "
            f"FAIL={self.counts['FAIL']} "
            f"SKIP={self.counts['SKIP']}"
        )

    def exit_code(self) -> int:
        return 1 if self.counts["FAIL"] else 0


def run(
    argv: Iterable[os.PathLike[str] | str],
    *,
    cwd: Path = REPO_ROOT,
    timeout: int | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(arg) for arg in argv],
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )


def run_binary(
    argv: Iterable[os.PathLike[str] | str],
    *,
    cwd: Path = REPO_ROOT,
    timeout: int | None = None,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(arg) for arg in argv],
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )


def tool_path(name: str, build_dir: Path, env_name: str) -> Path:
    if os.environ.get(env_name):
        return Path(os.environ[env_name])
    return build_dir / "bin" / name


def find_toolchain(args: argparse.Namespace) -> Toolchain:
    build_dir = Path(args.llvm_build)
    return Toolchain(
        llc=tool_path("llc", build_dir, "LLC"),
        llvm_mc=tool_path("llvm-mc", build_dir, "LLVM_MC"),
        ld_lld=tool_path("ld.lld", build_dir, "LD_LLD"),
        qemu=args.qemu or shutil.which("qemu-riscv64"),
    )


def missing(paths: Iterable[Path]) -> list[str]:
    return [str(path) for path in paths if not path.exists()]


def llc_args(
    tools: Toolchain,
    opt: str,
    variant: Variant,
    vlen_flags: list[str] = SOFTMAX_VLEN_FLAGS,
) -> list[str]:
    return [str(tools.llc), f"-{opt}", *vlen_flags, *variant.flags]


def generate_ir(rec: Recorder, work_dir: Path, kind: str, count: int) -> Path | None:
    out = work_dir / f"generated-{kind}-{count}.ll"
    proc = run([sys.executable, GENERATOR, kind, str(count)])
    if proc.returncode:
        rec.fail(
            f"generate-{kind}-{count}",
            f"generator exit {proc.returncode}: {proc.stderr.strip()}",
        )
        return None
    out.write_text(proc.stdout)
    rec.pass_(f"generate-{kind}-{count}", f"wrote {out}")
    return out


def count_reductions(text: str) -> int:
    return len(REDUCTION_RE.findall(text))


def count_vfexp(text: str) -> int:
    return len(VFEXP_RE.findall(text))


def count_spills(text: str) -> int:
    return len(SPILL_RE.findall(text))


def count_loads(text: str) -> int:
    return len(LOAD_RE.findall(text))


def pass_sections(dump: str, pass_id: str) -> dict[str, list[str]]:
    header = re.compile(
        r"(?m)^# \*\*\* IR Dump (Before|After) .* \("
        + re.escape(pass_id)
        + r"\) \*\*\*:\n"
    )
    any_header = re.compile(r"(?m)^# \*\*\* IR Dump (?:Before|After) .*\n")
    matches = list(header.finditer(dump))
    sections: dict[str, list[str]] = {"Before": [], "After": []}
    for match in matches:
        next_header = any_header.search(dump, match.end())
        end = next_header.start() if next_header else len(dump)
        sections[match.group(1)].append(dump[match.start() : end])
    return sections


def check_denominator(rec: Recorder, name: str, ir: str, expected: int) -> None:
    divs = re.findall(
        r"%norm\w*\s*=\s*fdiv fast <128 x float>\s+%exp\w*,\s*%sumv",
        ir,
        re.S,
    )
    stores = re.findall(
        r"store <128 x float>\s+%norm\w*,\s+ptr\s+%(?:op\d+|out)", ir, re.S
    )
    if len(divs) >= expected and len(stores) >= expected:
        rec.pass_(f"{name}-denominator", f"{len(stores)} store(s) depend on %sumv")
    else:
        rec.fail(
            f"{name}-denominator",
            f"expected {expected}; saw divs={len(divs)} stores={len(stores)}",
        )


def compile_asm(
    rec: Recorder,
    tools: Toolchain,
    name: str,
    ir: Path,
    opt: str,
    variant: Variant,
    work_dir: Path,
) -> None:
    asm_path = work_dir / f"{name}-{opt}-{variant.name}.s"
    proc = run(
        [
            *llc_args(tools, opt, variant),
            "-verify-machineinstrs",
            "-o",
            "-",
            ir,
        ]
    )
    if proc.returncode:
        rec.fail(
            f"{name}-{opt}-{variant.name}-asm",
            f"llc exit {proc.returncode}: {proc.stderr.strip()[:500]}",
        )
        return
    asm_path.write_text(proc.stdout)
    spills = count_spills(proc.stdout)
    vfexp = count_vfexp(proc.stdout)
    exp2f = len(EXP2F_RE.findall(proc.stdout))
    if exp2f:
        rec.fail(
            f"{name}-{opt}-{variant.name}-vfexp",
            f"found scalar exp2f fallback despite {SOFTMAX_VLEN_FLAGS[1]}",
        )
    elif vfexp == 0:
        rec.fail(f"{name}-{opt}-{variant.name}-vfexp", "missing yushuxin.vfexp")
    else:
        rec.pass_(
            f"{name}-{opt}-{variant.name}-asm",
            f"spills/reloads={spills}, yushuxin.vfexp={vfexp}, wrote {asm_path}",
        )


def run_dump(
    rec: Recorder,
    tools: Toolchain,
    name: str,
    ir: Path,
    opt: str,
    work_dir: Path,
) -> None:
    dump_path = work_dir / f"{name}-{opt}-stage1+2-pass-dump.txt"
    func = "scale_softmax" if name.startswith("generated") else "cleaned_softmax_tiny_realcase"
    proc = run(
        [
            *llc_args(tools, opt, VARIANTS[2]),
            "-verify-machineinstrs",
            f"-filter-print-funcs={func}",
            "-print-before=machine-scheduler",
            "-print-after=machine-scheduler",
            "-print-before=riscv-v-reg-pressure-remat",
            "-print-after=riscv-v-reg-pressure-remat",
            "-print-before=greedy",
            "-print-after=greedy",
            ir,
            "-o",
            os.devnull,
        ]
    )
    dump = proc.stdout + proc.stderr
    dump_path.write_text(dump)
    if proc.returncode:
        rec.fail(f"{name}-{opt}-pass-dump", f"llc exit {proc.returncode}")
        return

    missing_passes = [
        pass_id
        for pass_id in ("machine-scheduler", "riscv-v-reg-pressure-remat", "greedy")
        if not any(pass_sections(dump, pass_id).values())
    ]
    if missing_passes:
        rec.fail(f"{name}-{opt}-pass-dump", "missing " + ", ".join(missing_passes))
        return
    rec.pass_(
        f"{name}-{opt}-pass-dump",
        f"captured remat, MachineScheduler, and greedy sections in {dump_path}",
    )

    remat = pass_sections(dump, "riscv-v-reg-pressure-remat")
    before = remat["Before"][0] if remat["Before"] else ""
    after = remat["After"][0] if remat["After"] else ""
    before_reductions = count_reductions(before)
    after_reductions = count_reductions(after)
    if before_reductions == 0:
        rec.fail(f"{name}-{opt}-no-reduction-recompute", "no reductions in remat input")
    elif before_reductions == after_reductions:
        rec.pass_(
            f"{name}-{opt}-no-reduction-recompute",
            "remat reduction count unchanged "
            f"({before_reductions}); vfexp {count_vfexp(before)}->{count_vfexp(after)}, "
            f"loads {count_loads(before)}->{count_loads(after)}",
        )
    else:
        rec.fail(
            f"{name}-{opt}-no-reduction-recompute",
            f"remat changed reduction count {before_reductions}->{after_reductions}",
        )


def add_harness() -> str:
    return """\
    .text
    .globl _start
_start:
    la a0, input_a
    la a1, input_b
    la a2, output
    call scale_add
    li a0, 1
    la a1, output
    li a2, 512
    li a7, 64
    ecall
    li a0, 0
    li a7, 93
    ecall

    .section .rodata
    .balign 64
input_a:
    .rept 128
    .word 0x3f800000
    .endr
input_b:
    .rept 128
    .word 0x40000000
    .endr

    .bss
    .balign 64
output:
    .zero 512
"""


def float_data_words(values: Iterable[float]) -> str:
    return "\n".join(
        f"    .word 0x{float_word(value):08x}"
        for value in values
    )


def float_word(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def runtime_exp2f_poly_words() -> str:
    ln2 = math.log(2.0)
    coeffs = [
        ln2**power / math.factorial(power)
        for power in range(EXP2F_POLY_DEGREE, -1, -1)
    ]
    return float_data_words(coeffs)


def softmax_harness() -> str:
    input_words = float_data_words(SOFTMAX_RUNTIME_INPUT)
    exp2f_probe_input = float_data_words(EXP2F_PROBE_INPUT)
    exp2f_poly = runtime_exp2f_poly_words()
    return f"""\
    .text
    .globl _start
_start:
    la a0, input
    la a1, output
    la a2, sum_out
    call scale_softmax
    li a0, 1
    la a1, output
    li a2, 512
    li a7, 64
    ecall
    li a0, 1
    la a1, sum_out
    li a2, 4
    li a7, 64
    ecall
    la s5, exp2f_probe_input
    la s6, exp2f_probe_output
    li s7, {len(EXP2F_PROBE_INPUT)}
3:
    flw fa0, 0(s5)
    call exp2f
    fsw fa0, 0(s6)
    addi s5, s5, 4
    addi s6, s6, 4
    addi s7, s7, -1
    bnez s7, 3b
    li a0, 1
    la a1, exp2f_probe_output
    li a2, {len(EXP2F_PROBE_INPUT) * 4}
    li a7, 64
    ecall
    li a0, 0
    li a7, 93
    ecall

    .globl exp2f
    .type exp2f,@function
exp2f:
    fcvt.w.s t0, fa0, rdn
    fcvt.s.w ft0, t0
    fsub.s ft1, fa0, ft0
    la t1, exp2f_poly
    flw fa0, 0(t1)
    flw ft2, 4(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 8(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 12(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 16(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 20(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 24(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 28(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 32(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 36(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    flw ft2, 40(t1)
    fmul.s fa0, fa0, ft1
    fadd.s fa0, fa0, ft2
    addi t0, t0, 127
    slli t0, t0, 23
    fmv.w.x ft2, t0
    fmul.s fa0, fa0, ft2
    ret

    .section .rodata
    .balign 64
input:
{input_words}
    .balign 4
exp2f_probe_input:
{exp2f_probe_input}
exp2f_poly:
{exp2f_poly}

    .bss
    .balign 64
output:
    .zero 512
sum_out:
    .word 0
exp2f_probe_output:
    .zero {len(EXP2F_PROBE_INPUT) * 4}
"""


def compile_runtime_elf(
    rec: Recorder,
    tools: Toolchain,
    name: str,
    ir: Path,
    harness: str,
    variant: Variant,
    work_dir: Path,
) -> Path | None:
    runtime_dir = work_dir / "runtime"
    runtime_dir.mkdir(exist_ok=True)
    kernel_o = runtime_dir / f"{name}-{variant.name}.o"
    harness_s = runtime_dir / f"{name}-{variant.name}-harness.s"
    harness_o = runtime_dir / f"{name}-{variant.name}-harness.o"
    elf = runtime_dir / f"{name}-{variant.name}.elf"
    harness_s.write_text(harness)

    compile_proc = run(
        [
            *llc_args(
                tools,
                "O2",
                variant,
                ADD_RUNTIME_FLAGS if name == "add" else SOFTMAX_RUNTIME_FLAGS,
            ),
            *RUNTIME_OBJECT_FLAGS,
            "-filetype=obj",
            "-relocation-model=static",
            ir,
            "-o",
            kernel_o,
        ]
    )
    if compile_proc.returncode:
        rec.fail(
            f"runtime-{name}-{variant.name}-compile",
            f"llc exit {compile_proc.returncode}: {compile_proc.stderr.strip()[:500]}",
        )
        return None

    asm_proc = run(
        [
            tools.llvm_mc,
            "-triple=riscv64",
            f"-mattr={HARNESS_MC_MATTR}",
            "-filetype=obj",
            harness_s,
            "-o",
            harness_o,
        ]
    )
    if asm_proc.returncode:
        rec.fail(
            f"runtime-{name}-{variant.name}-assemble",
            f"llvm-mc exit {asm_proc.returncode}: {asm_proc.stderr.strip()[:500]}",
        )
        return None

    link_proc = run(
        [
            tools.ld_lld,
            "-m",
            "elf64lriscv",
            "-static",
            "-e",
            "_start",
            harness_o,
            kernel_o,
            "-o",
            elf,
        ]
    )
    if link_proc.returncode:
        detail = link_proc.stderr.strip()
        if "yushuxin-vfexp" in detail and "string may only contain" in detail:
            rec.skip(
                f"runtime-{name}-{variant.name}-link",
                "ld.lld rejected the experimental Yushuxin RISC-V attribute",
            )
        else:
            rec.fail(
                f"runtime-{name}-{variant.name}-link",
                f"ld.lld exit {link_proc.returncode}: {detail[:500]}",
            )
        return None
    return elf


def qemu_skip_reason(proc: subprocess.CompletedProcess[bytes]) -> str | None:
    combined = (proc.stdout + proc.stderr).decode(errors="replace").lower()
    if proc.returncode in (-4, 132) or "illegal instruction" in combined:
        return "qemu reported illegal instruction for required RVV/custom ISA"
    if (
        "can't apply global" in combined
        or "property" in combined
        or "invalid parameter" in combined
        or "expected key=value" in combined
    ):
        return "qemu rejected the requested CPU/vlen configuration"
    return None


def expected_runtime_bytes(name: str) -> bytes:
    if name == "add":
        return struct.pack("<128f", *([3.0] * 128))
    if name == "softmax":
        max_value = max(SOFTMAX_RUNTIME_INPUT)
        exp_values = [math.exp(value - max_value) for value in SOFTMAX_RUNTIME_INPUT]
        exp_sum = sum(exp_values)
        normalized = [value / exp_sum for value in exp_values]
        exp2f_probe = [2.0**value for value in EXP2F_PROBE_INPUT]
        return (
            struct.pack("<128f", *normalized)
            + struct.pack("<f", exp_sum)
            + struct.pack(f"<{len(exp2f_probe)}f", *exp2f_probe)
        )
    raise ValueError(name)


def check_runtime_output(rec: Recorder, name: str, variant: Variant, data: bytes) -> None:
    expected = expected_runtime_bytes(name)
    if len(data) != len(expected):
        rec.fail(
            f"runtime-{name}-{variant.name}",
            f"stdout bytes {len(data)} != expected {len(expected)}",
        )
        return

    if name == "add":
        if data == expected:
            rec.pass_(f"runtime-{name}-{variant.name}", "qemu output matched exactly")
        else:
            rec.fail(f"runtime-{name}-{variant.name}", "qemu output mismatch")
        return

    got_vals = struct.unpack("<128f", data[:512])
    got_sum = struct.unpack("<f", data[512:516])[0]
    probe_count = len(EXP2F_PROBE_INPUT)
    got_probe = struct.unpack(f"<{probe_count}f", data[516:])
    ref_vals = struct.unpack("<128f", expected[:512])
    ref_sum = struct.unpack("<f", expected[512:516])[0]
    ref_probe = struct.unpack(f"<{probe_count}f", expected[516:])
    max_abs = max(abs(a - b) for a, b in zip(got_vals, ref_vals))
    sum_abs = abs(got_sum - ref_sum)
    exp2f_abs = max(abs(a - b) for a, b in zip(got_probe, ref_probe))
    if max_abs <= 1.0e-6 and sum_abs <= 1.0e-4 and exp2f_abs <= 1.0e-6:
        rec.pass_(
            f"runtime-{name}-{variant.name}",
            "qemu output matched host reference, "
            f"max_abs={max_abs:g}, sum_abs={sum_abs:g}, exp2f_abs={exp2f_abs:g}",
        )
    else:
        rec.fail(
            f"runtime-{name}-{variant.name}",
            "softmax mismatch, "
            f"max_abs={max_abs:g}, sum_abs={sum_abs:g}, exp2f_abs={exp2f_abs:g}",
        )


def run_runtime(
    rec: Recorder,
    tools: Toolchain,
    args: argparse.Namespace,
    name: str,
    ir: Path,
    harness: str,
    work_dir: Path,
) -> None:
    if not args.runtime:
        rec.skip(f"runtime-{name}", "runtime validation disabled; pass --runtime")
        return
    if not tools.qemu:
        rec.skip(f"runtime-{name}", "qemu-riscv64 not found")
        return
    tool_missing = missing([tools.llc, tools.llvm_mc, tools.ld_lld])
    if tool_missing:
        rec.skip(f"runtime-{name}", "missing tool(s): " + ", ".join(tool_missing))
        return

    for variant in RUNTIME_VARIANTS:
        elf = compile_runtime_elf(rec, tools, name, ir, harness, variant, work_dir)
        if elf is None:
            continue
        qemu_cmd = [tools.qemu]
        if args.qemu_cpu:
            qemu_cmd.extend(["-cpu", args.qemu_cpu])
        qemu_cmd.append(str(elf))
        proc = run_binary(qemu_cmd, timeout=args.qemu_timeout)
        if proc.returncode == 0:
            check_runtime_output(rec, name, variant, proc.stdout)
            continue
        reason = qemu_skip_reason(proc)
        if reason:
            rec.skip(f"runtime-{name}-{variant.name}", reason)
        else:
            rec.fail(
                f"runtime-{name}-{variant.name}",
                f"qemu exit {proc.returncode}; stderr={proc.stderr.decode(errors='replace').strip()!r}",
            )


def compiler_evidence(
    rec: Recorder, tools: Toolchain, args: argparse.Namespace, work_dir: Path
) -> None:
    if not tools.llc.exists():
        rec.skip("softmax-evidence", f"llc not found at {tools.llc}")
        return

    generated = generate_ir(rec, work_dir, "softmax", args.generated_count)
    cases: list[tuple[str, Path, int]] = []
    if generated:
        cases.append((f"generated-softmax-{args.generated_count}", generated, args.generated_count))
    if REALCASE.exists():
        cases.append(("cleaned-realcase-softmax", REALCASE, 1))
    else:
        rec.skip("cleaned-realcase-softmax", f"missing {REALCASE}")

    for name, ir, expected_stores in cases:
        check_denominator(rec, name, ir.read_text(), expected_stores)
        for opt in args.opt:
            for variant in VARIANTS:
                compile_asm(rec, tools, name, ir, opt, variant, work_dir)
            run_dump(rec, tools, name, ir, opt, work_dir)


def runtime_evidence(
    rec: Recorder, tools: Toolchain, args: argparse.Namespace, work_dir: Path
) -> None:
    add_ir = generate_ir(rec, work_dir, "add", 1)
    softmax_ir = generate_ir(rec, work_dir, "softmax", 1)
    if add_ir:
        run_runtime(rec, tools, args, "add", add_ir, add_harness(), work_dir)
    if softmax_ir:
        run_runtime(rec, tools, args, "softmax", softmax_ir, softmax_harness(), work_dir)


def self_test() -> None:
    sample = """\
# *** IR Dump Before RISC-V RVV register pressure rematerialization (riscv-v-reg-pressure-remat) ***:
  undef %1:vrm4 = PseudoVFREDMAX_VS_M4_E32 %0
  undef %2:vrm4 = PseudoVFREDUSUM_VS_M4_E32 %1
  %3:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 %0
# *** IR Dump After RISC-V RVV register pressure rematerialization (riscv-v-reg-pressure-remat) ***:
  undef %1:vrm4 = PseudoVFREDMAX_VS_M4_E32 %0
  undef %2:vrm4 = PseudoVFREDUSUM_VS_M4_E32 %1
  %3:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 %0
  %4:vrm4 = PseudoYUSHUXIN_VFEXP_V_M4_E32 %0
# *** IR Dump Before Greedy Register Allocator (greedy) ***:
  vs4r.v v8, (sp)
"""
    sections = pass_sections(sample, "riscv-v-reg-pressure-remat")
    assert len(sections["Before"]) == 1
    assert len(sections["After"]) == 1
    assert count_reductions(sections["Before"][0]) == 2
    assert count_reductions(sections["After"][0]) == 2
    assert count_vfexp(sections["After"][0]) == 2
    assert count_spills("vs4r.v\nvl1r.v\nvle32.v\n") == 2


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate RISCV RVV softmax pressure evidence and optional qemu validation."
    )
    parser.add_argument("--self-test", action="store_true", help="run parser self-tests")
    parser.add_argument(
        "--llvm-build",
        default=str(REPO_ROOT / "build-riscv"),
        help="LLVM build directory containing bin/llc, bin/llvm-mc, and bin/ld.lld",
    )
    parser.add_argument("--work-dir", type=Path, help="keep generated artifacts here")
    parser.add_argument(
        "--generated-count",
        type=int,
        default=512,
        help="generated softmax slice count for compiler evidence",
    )
    parser.add_argument(
        "--opt",
        choices=("O2", "O3"),
        action="append",
        default=None,
        help="optimization level to measure; repeat for O2 and O3",
    )
    parser.add_argument("--runtime", action="store_true", help="run qemu ELF checks")
    parser.add_argument("--qemu", help="path to qemu-riscv64")
    parser.add_argument(
        "--qemu-cpu",
        default="max,v=true,vlen=1024,elen=64",
        help="qemu -cpu argument for fixed-VLEN runtime checks",
    )
    parser.add_argument("--qemu-timeout", type=int, default=20)
    args = parser.parse_args(argv)
    if args.opt is None:
        args.opt = ["O2"]
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        self_test()
        print("PASS: self-test")
        return 0

    rec = Recorder()
    tools = find_toolchain(args)
    manager = (
        contextlib.nullcontext(args.work_dir)
        if args.work_dir
        else tempfile.TemporaryDirectory(prefix="rvv-pressure-evidence-")
    )
    with manager as raw_work_dir:
        work_dir = Path(raw_work_dir)
        work_dir.mkdir(parents=True, exist_ok=True)
        print(f"INFO: work-dir={work_dir}")
        print(f"INFO: llc={tools.llc}")
        print(f"INFO: vfexp-mattr={SOFTMAX_VLEN_FLAGS[1]}")
        compiler_evidence(rec, tools, args, work_dir)
        runtime_evidence(rec, tools, args, work_dir)
    rec.summary()
    return rec.exit_code()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
