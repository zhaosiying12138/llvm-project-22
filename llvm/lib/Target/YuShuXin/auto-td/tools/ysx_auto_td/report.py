from collections import Counter
from pathlib import Path


def write_coverage(path, instructions):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    counts = Counter(instruction.status for instruction in instructions)
    lines = ["# YSX Auto TD Coverage", ""]
    for status in ("auto_full", "auto_with_structured_override", "retained_schema_gap"):
        lines.append(f"- {status}: {counts.get(status, 0)}")
    lines.append("")
    lines.append("| instruction | status | opcode source | yaml |")
    lines.append("|---|---|---|---|")
    for instruction in instructions:
        source = (
            f"{instruction.opcode_source.repo}/"
            f"{instruction.opcode_source.extension}/"
            f"{instruction.opcode_source.key}"
        )
        lines.append(
            f"| {instruction.mnemonic} | {instruction.status} | {source} | "
            f"{instruction.path.as_posix()} |"
        )
    path.write_text("\n".join(lines) + "\n")
