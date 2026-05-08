import csv
from pathlib import Path

from .model import OpcodeFieldRange, OpcodeRecord


def source_key_from_mnemonic(mnemonic: str) -> str:
    return mnemonic.replace(".", "_").replace("-", "_")


def parse_opcode_file(
    path: Path, arg_lut: dict[str, tuple[int, int]] | None = None
) -> dict[str, OpcodeRecord]:
    arg_lut = arg_lut or {}
    records: dict[str, OpcodeRecord] = {}
    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line or line.startswith("$"):
            continue
        parts = line.split()
        mnemonic = parts[0]
        key = source_key_from_mnemonic(mnemonic)
        fields = tuple(part for part in parts[1:] if "=" not in part)
        fixed_bits = tuple(part for part in parts[1:] if "=" in part)
        field_ranges = tuple(
            OpcodeFieldRange(field, *arg_lut[field])
            for field in fields
            if field in arg_lut
        )
        records[key] = OpcodeRecord(
            key, mnemonic, fields, fixed_bits, path, field_ranges
        )
    return records


def load_arg_lut(root: Path) -> dict[str, tuple[int, int]]:
    path = root / "arg_lut.csv"
    if not path.is_file():
        return {}

    result: dict[str, tuple[int, int]] = {}
    with path.open(newline="") as handle:
        for row in csv.reader(handle, skipinitialspace=True):
            if len(row) != 3:
                continue
            result[row[0]] = (int(row[1]), int(row[2]))
    return result


def load_opcode_repo(
    root: Path, fallback_arg_lut: dict[str, tuple[int, int]] | None = None
) -> dict[tuple[str, str], OpcodeRecord]:
    records: dict[tuple[str, str], OpcodeRecord] = {}
    arg_lut = load_arg_lut(root) or (fallback_arg_lut or {})
    extensions = root / "extensions"
    if not extensions.is_dir():
        return records
    for path in sorted(extensions.glob("*")):
        if not _is_opcode_source_file(path):
            continue
        for key, record in parse_opcode_file(path, arg_lut).items():
            records[(path.name, key)] = record
    return records


def _is_opcode_source_file(path: Path) -> bool:
    return path.is_file() and not path.name.startswith(".")
