from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class OpcodeSource:
    repo: str
    extension: str
    key: str


@dataclass(frozen=True)
class OpcodeRecord:
    key: str
    mnemonic: str
    fields: tuple[str, ...]
    fixed_bits: tuple[str, ...]
    source_file: Path


@dataclass
class InstructionRecord:
    path: Path
    mnemonic: str
    opcode_source: OpcodeSource
    opcode: OpcodeRecord
    spec_ref: str
    status: str = "auto_full"
    retained_owner_files: list[str] = field(default_factory=list)
