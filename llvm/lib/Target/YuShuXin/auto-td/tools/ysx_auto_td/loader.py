from pathlib import Path

import yaml

from .model import EffectSpec, InstructionRecord, OpcodeSource, OperandSpec
from .opcodes import load_arg_lut, load_opcode_repo


def load_instruction_set(
    ysx_root: Path, riscv_opcodes: Path, ysx_opcodes: Path
) -> list[InstructionRecord]:
    riscv_arg_lut = load_arg_lut(riscv_opcodes)
    taxonomy = _load_taxonomy(ysx_root / "auto-td" / "taxonomy")
    opcode_repos = {
        "riscv-opcodes": load_opcode_repo(riscv_opcodes),
        "ysx-opcodes": load_opcode_repo(ysx_opcodes, riscv_arg_lut),
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
        spec_ref = _required_string(path, data, "spec_ref")
        category = taxonomy.get(_category_key(spec_ref), {})
        records.append(
            InstructionRecord(
                path=path,
                mnemonic=_required_string(path, data, "mnemonic"),
                opcode_source=source,
                opcode=opcode,
                spec_ref=spec_ref,
                status=data.get("status", "auto_full"),
                aliases=_load_aliases(path, data),
                pseudos=_load_pseudos(path, data),
                patterns=_load_patterns(path, data),
                builtin=_load_builtin(path, data),
                operands_out=_load_operand_specs(category, "outs"),
                operands_in=_load_operand_specs(category, "ins"),
                effects=_load_effects(category),
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


def _load_aliases(path: Path, data: dict) -> tuple[str, ...]:
    aliases = data.get("aliases", [])
    if not aliases:
        return ()
    if not isinstance(aliases, list):
        raise ValueError(f"{path}: aliases must be a list")
    result: list[str] = []
    for index, alias in enumerate(aliases):
        if not isinstance(alias, dict) or not isinstance(alias.get("mnemonic"), str):
            raise ValueError(f"{path}: aliases[{index}] must define mnemonic")
        result.append(alias["mnemonic"])
    return tuple(result)


def _load_pseudos(path: Path, data: dict) -> dict:
    pseudos = data.get("pseudos", {})
    if not pseudos:
        return {}
    if not isinstance(pseudos, dict):
        raise ValueError(f"{path}: pseudos must be a mapping")
    matrix = pseudos.get("matrix")
    if matrix is not None:
        if not isinstance(matrix, dict):
            raise ValueError(f"{path}: pseudos.matrix must be a mapping")
        element_types = matrix.get("element_types")
        if not isinstance(element_types, list) or not element_types:
            raise ValueError(f"{path}: pseudos.matrix must define element_types")
        if not all(isinstance(item, str) and item for item in element_types):
            raise ValueError(
                f"{path}: pseudos.matrix element_types must be non-empty strings"
            )
        if not isinstance(matrix.get("lmuls"), str) or not matrix["lmuls"]:
            raise ValueError(f"{path}: pseudos.matrix must define lmuls")
        if "masked" not in matrix or not isinstance(matrix["masked"], bool):
            raise ValueError(f"{path}: pseudos.matrix.masked must be a bool")
        if not isinstance(matrix.get("policy"), str) or not matrix["policy"]:
            raise ValueError(f"{path}: pseudos.matrix must define policy")
    return dict(pseudos)


def _load_patterns(path: Path, data: dict) -> tuple[dict, ...]:
    patterns = data.get("patterns", [])
    if not patterns:
        return ()
    if not isinstance(patterns, list):
        raise ValueError(f"{path}: patterns must be a list")
    result: list[dict] = []
    for index, pattern in enumerate(patterns):
        if not isinstance(pattern, dict):
            raise ValueError(f"{path}: patterns[{index}] must be a mapping")
        if not isinstance(pattern.get("kind"), str) or not pattern["kind"]:
            raise ValueError(f"{path}: patterns[{index}] must define kind")
        if pattern["kind"] == "intrinsic_to_pseudo":
            if not isinstance(pattern.get("intrinsic"), str) or not pattern["intrinsic"]:
                raise ValueError(f"{path}: patterns[{index}] must define intrinsic")
            if not isinstance(pattern.get("operation"), str) or not pattern["operation"]:
                raise ValueError(f"{path}: patterns[{index}] must define operation")
        result.append(dict(pattern))
    return tuple(result)


def _load_builtin(path: Path, data: dict) -> dict | None:
    builtin = data.get("builtin")
    if not builtin:
        return None
    if not isinstance(builtin, dict):
        raise ValueError(f"{path}: builtin must be a mapping")
    header = builtin.get("header")
    if not isinstance(header, str) or not header:
        raise ValueError(f"{path}: builtin must define header")
    names = builtin.get("names", [])
    if not isinstance(names, list) or not names:
        raise ValueError(f"{path}: builtin must define names")
    if not all(isinstance(name, str) and name for name in names):
        raise ValueError(f"{path}: builtin names must be non-empty strings")
    overloaded = builtin.get("overloaded", False)
    if not isinstance(overloaded, bool):
        raise ValueError(f"{path}: builtin.overloaded must be a bool")
    codegen = builtin.get("codegen", False)
    if not isinstance(codegen, bool):
        raise ValueError(f"{path}: builtin.codegen must be a bool")
    return {
        "header": header,
        "names": tuple(names),
        "overloaded": overloaded,
        "codegen": codegen,
    }


def _load_taxonomy(root: Path) -> dict[str, dict]:
    categories: dict[str, dict] = {}
    if not root.is_dir():
        return categories
    for path in sorted(root.glob("*.yaml")):
        data = _load_yaml_mapping(path)
        file_categories = data.get("categories", {})
        if not isinstance(file_categories, dict):
            raise ValueError(f"{path}: categories must be a mapping")
        for name, category in file_categories.items():
            if not isinstance(category, dict):
                raise ValueError(f"{path}: category {name} must be a mapping")
            categories[name] = category
    return categories


def _category_key(spec_ref: str) -> str:
    return spec_ref.rsplit(".", 1)[-1]


def _load_operand_specs(category: dict, direction: str) -> tuple[OperandSpec, ...]:
    operands = category.get("operands", {})
    if not isinstance(operands, dict):
        return ()
    specs = operands.get(direction, [])
    if not isinstance(specs, list):
        return ()
    return tuple(_load_operand_spec(spec) for spec in specs)


def _load_operand_spec(spec: dict) -> OperandSpec:
    if not isinstance(spec, dict):
        return OperandSpec(role="")
    return OperandSpec(
        role=str(spec.get("role", "")),
        field=spec.get("field"),
        reg_class=spec.get("reg_class"),
        operand=spec.get("operand"),
    )


def _load_effects(category: dict) -> EffectSpec:
    effects = category.get("effects", {})
    if not isinstance(effects, dict):
        return EffectSpec()
    return EffectSpec(
        may_load=bool(effects.get("may_load", False)),
        may_store=bool(effects.get("may_store", False)),
        has_side_effects=bool(effects.get("has_side_effects", False)),
        implicit_uses=_load_string_list(effects.get("implicit_uses", [])),
        implicit_defs=_load_string_list(effects.get("implicit_defs", [])),
    )


def _load_string_list(value) -> tuple[str, ...]:
    if not isinstance(value, list):
        return ()
    return tuple(item for item in value if isinstance(item, str) and item)


def _coverage_path(ysx_root: Path, path: Path) -> str:
    try:
        relative = path.relative_to(ysx_root)
    except ValueError:
        relative = path
    return f"llvm/lib/Target/YuShuXin/{relative.as_posix()}"
