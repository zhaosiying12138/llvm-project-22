#!/usr/bin/env python3
import argparse
from pathlib import Path

from ysx_auto_td.emit_td import (
    write_clang_builtin_cg_inc,
    write_clang_builtins_td,
    write_llvm_intrinsics_td,
    write_td_outputs,
)
from ysx_auto_td.loader import load_instruction_set
from ysx_auto_td.report import write_coverage
from ysx_auto_td.validate import validate_instruction_set


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate YSX auto TD includes")
    parser.add_argument("--ysx-root", required=True)
    parser.add_argument("--riscv-opcodes", required=True)
    parser.add_argument("--ysx-opcodes", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--coverage", required=True)
    parser.add_argument("--clang-builtins-td")
    parser.add_argument("--clang-builtin-cg-inc")
    parser.add_argument("--llvm-intrinsics-td")
    args = parser.parse_args()

    instructions = load_instruction_set(
        Path(args.ysx_root),
        Path(args.riscv_opcodes),
        Path(args.ysx_opcodes),
    )
    validate_instruction_set(instructions)
    out_dir = Path(args.out_dir)
    write_td_outputs(out_dir, instructions)
    if args.clang_builtins_td:
        write_clang_builtins_td(Path(args.clang_builtins_td), instructions)
    if args.clang_builtin_cg_inc:
        write_clang_builtin_cg_inc(Path(args.clang_builtin_cg_inc), instructions)
    if args.llvm_intrinsics_td:
        write_llvm_intrinsics_td(Path(args.llvm_intrinsics_td), instructions)
    write_coverage(Path(args.coverage), instructions)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
