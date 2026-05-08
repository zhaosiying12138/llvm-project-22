from pathlib import Path

import yaml

from .model import InstructionRecord, OpcodeSource
from .opcodes import load_opcode_repo


def load_instruction_set(
    ysx_root: Path, riscv_opcodes: Path, ysx_opcodes: Path
) -> list[InstructionRecord]:
    opcode_repos = {
        "riscv-opcodes": load_opcode_repo(riscv_opcodes),
        "ysx-opcodes": load_opcode_repo(ysx_opcodes),
    }
    records: list[InstructionRecord] = []
    instructions_root = ysx_root / "auto-td" / "instructions"
    for path in sorted(instructions_root.glob("*/*.yaml")):
        data = _load_yaml_mapping(path)
        source = _load_opcode_source(path, data)
        repo = opcode_repos.get(source.repo)
        if repo is None:
            raise ValueError(f"{path}: unknown opcode repo '{source.repo}'")
        opcode = repo.get((source.extension, source.key))
        if opcode is None:
            source_name = f"{source.repo}/{source.extension}/{source.key}"
            raise ValueError(f"{path}: missing opcode source {source_name}")
        records.append(
            InstructionRecord(
                path=path,
                mnemonic=_required_string(path, data, "mnemonic"),
                opcode_source=source,
                opcode=opcode,
                spec_ref=_required_string(path, data, "spec_ref"),
                status=data.get("status", "auto_full"),
                retained_owner_files=list(data.get("retained_owner_files", [])),
                coverage_path=_coverage_path(ysx_root, path),
            )
        )
    return records


def _load_yaml_mapping(path: Path) -> dict:
    data = yaml.safe_load(path.read_text())
    if not isinstance(data, dict):
        raise ValueError(f"{path}: expected YAML mapping")
    return data


def _load_opcode_source(path: Path, data: dict) -> OpcodeSource:
    source_data = data.get("opcode_source")
    if not isinstance(source_data, dict):
        raise ValueError(f"{path}: missing opcode_source mapping")
    return OpcodeSource(
        _required_string(path, source_data, "repo"),
        _required_string(path, source_data, "extension"),
        _required_string(path, source_data, "key"),
    )


def _required_string(path: Path, data: dict, key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value:
        raise ValueError(f"{path}: missing required string field {key}")
    return value


def _coverage_path(ysx_root: Path, path: Path) -> str:
    try:
        relative = path.relative_to(ysx_root)
    except ValueError:
        relative = path
    return f"llvm/lib/Target/YuShuXin/{relative.as_posix()}"
