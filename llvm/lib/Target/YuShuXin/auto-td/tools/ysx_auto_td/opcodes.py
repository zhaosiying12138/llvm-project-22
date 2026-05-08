from pathlib import Path

from .model import OpcodeRecord


def source_key_from_mnemonic(mnemonic: str) -> str:
    return mnemonic.replace(".", "_").replace("-", "_")


def parse_opcode_file(path: Path) -> dict[str, OpcodeRecord]:
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
        records[key] = OpcodeRecord(key, mnemonic, fields, fixed_bits, path)
    return records


def load_opcode_repo(root: Path) -> dict[tuple[str, str], OpcodeRecord]:
    records: dict[tuple[str, str], OpcodeRecord] = {}
    extensions = root / "extensions"
    if not extensions.is_dir():
        return records
    for path in sorted(extensions.glob("*")):
        if not path.is_file():
            continue
        for key, record in parse_opcode_file(path).items():
            records[(path.name, key)] = record
    return records
