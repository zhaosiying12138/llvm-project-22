#!/usr/bin/env python3
"""Count short-distance RISC-V RVV write-after-read register pairs."""

import argparse
import collections
import re
import sys


VECTOR_RE = re.compile(r"\bv(?:[0-9]|[12][0-9]|3[01])\b")
LABEL_RE = re.compile(r"^[A-Za-z_.$][\w.$]*:$")


def strip_comment(line):
    return line.split("#", 1)[0].split("//", 1)[0].strip()


def vector_regs(text):
    return VECTOR_RE.findall(text)


def analyze(lines, window):
    recent_reads = collections.defaultdict(collections.deque)
    pairs = []

    for line_no, raw_line in enumerate(lines, 1):
        line = strip_comment(raw_line)
        if not line:
            continue
        if LABEL_RE.match(line):
            recent_reads.clear()
            continue

        parts = line.split(None, 1)
        if not parts:
            continue
        opcode = parts[0]
        operands = parts[1] if len(parts) > 1 else ""
        regs = vector_regs(operands)
        if not opcode.startswith("v") or not regs:
            continue

        defs = []
        reads = list(regs)
        if not opcode.startswith("vs") and not opcode.startswith("vse"):
            defs = [regs[0]]
            reads = regs[1:]

        for reg in defs:
            for read_line, read_inst in list(recent_reads[reg]):
                if line_no - read_line <= window:
                    pairs.append((read_line, read_inst, line_no, line, reg))

        for reg in reads:
            q = recent_reads[reg]
            q.append((line_no, line))
            while q and line_no - q[0][0] > window:
                q.popleft()

    return pairs


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("input", nargs="?", help="assembly file, or stdin")
    parser.add_argument("--window", type=int, default=8)
    parser.add_argument("--samples", type=int, default=8)
    args = parser.parse_args(argv)

    if args.input:
        with open(args.input, "r", encoding="utf-8") as f:
            lines = f.readlines()
    else:
        lines = sys.stdin.readlines()

    pairs = analyze(lines, args.window)
    print(f"RVV WAR pairs: {len(pairs)}")
    print(f"Window: {args.window}")
    if not pairs:
        print("Samples: none")
        return 0

    print("Samples:")
    for read_line, read_inst, write_line, write_inst, reg in pairs[: args.samples]:
        print(
            f"  line {read_line} reads {reg} -> "
            f"line {write_line} writes {reg}"
        )
        print(f"    read:  {read_inst}")
        print(f"    write: {write_inst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
