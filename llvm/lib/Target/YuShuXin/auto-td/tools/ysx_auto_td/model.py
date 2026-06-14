from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class OpcodeSource:
    repo: str
    extension: str
    key: str


@dataclass(frozen=True)
class OpcodeFieldRange:
    name: str
    msb: int
    lsb: int


@dataclass(frozen=True)
class OpcodeRecord:
    key: str
    mnemonic: str
    fields: tuple[str, ...]
    fixed_bits: tuple[str, ...]
    source_file: Path
    field_ranges: tuple[OpcodeFieldRange, ...] = ()


@dataclass(frozen=True)
class OperandSpec:
    role: str
    field: str | None = None
    reg_class: str | None = None
    operand: str | None = None


@dataclass(frozen=True)
class EffectSpec:
    may_load: bool = False
    may_store: bool = False
    has_side_effects: bool = False
    implicit_uses: tuple[str, ...] = ()
    implicit_defs: tuple[str, ...] = ()


@dataclass
class InstructionRecord:
    path: Path
    mnemonic: str
    opcode_source: OpcodeSource
    opcode: OpcodeRecord
    spec_ref: str
    status: str = "auto_full"
    aliases: tuple[str, ...] = ()
    pseudos: dict = field(default_factory=dict)
    patterns: tuple[dict, ...] = ()
    builtin: dict | None = None
    operands_out: tuple[OperandSpec, ...] = ()
    operands_in: tuple[OperandSpec, ...] = ()
    effects: EffectSpec = field(default_factory=EffectSpec)
    retained_owner_files: list[str] = field(default_factory=list)
    coverage_path: str | None = None
