import yaml


FORBIDDEN = (
    "raw_td",
    "def : Pat",
    "RVInst",
    "VUnitStrideLoad",
    "VRED_",
    "VPseudo",
    "let Inst{",
    "bits<",
)
FORBIDDEN_FIELDS = {"raw_cpp", "encoding", "fixed_bits"}
ALLOWED_STATUS = {
    "auto_full",
    "auto_with_structured_override",
    "retained_schema_gap",
}
SUPPORTED_BUILTIN_CODEGEN_OPERATIONS = {
    "add",
    "sub",
    "mul",
    "ireduce_sum",
    "freduce_sum",
    "gather",
    "slideup",
    "fexp",
}


def validate_instruction_set(instructions):
    for instruction in instructions:
        text = instruction.path.read_text()
        for token in FORBIDDEN:
            if token in text:
                raise ValueError(f"{instruction.path}: forbidden token {token}")
        data = yaml.safe_load(text)
        if not isinstance(data, dict):
            raise ValueError(f"{instruction.path}: expected YAML mapping")
        for field in sorted(FORBIDDEN_FIELDS.intersection(data)):
            raise ValueError(f"{instruction.path}: forbidden field {field}")
        if instruction.status not in ALLOWED_STATUS:
            raise ValueError(f"{instruction.path}: invalid status {instruction.status}")
        if (
            instruction.status == "retained_schema_gap"
            and not instruction.retained_owner_files
        ):
            raise ValueError(
                f"{instruction.path}: retained_schema_gap requires retained_owner_files"
            )
        if instruction.mnemonic != instruction.opcode.mnemonic and "aliases:" not in text:
            raise ValueError(
                f"{instruction.path}: mnemonic {instruction.mnemonic} does not match "
                f"opcode {instruction.opcode.mnemonic}"
            )
        _validate_builtin_codegen(instruction)


def _validate_builtin_codegen(instruction) -> None:
    if not instruction.builtin or not instruction.builtin.get("codegen", False):
        return

    pattern = _intrinsic_pattern(instruction)
    operation = pattern.get("operation")
    if operation not in SUPPORTED_BUILTIN_CODEGEN_OPERATIONS:
        raise ValueError(
            f"{instruction.path}: unsupported builtin.codegen operation {operation}"
        )
    intrinsic = pattern.get("intrinsic")
    if not isinstance(intrinsic, str) or not intrinsic.startswith("ysx."):
        raise ValueError(
            f"{instruction.path}: builtin.codegen intrinsic must use ysx. prefix"
        )
    for name in instruction.builtin["names"]:
        _validate_codegen_builtin_name(instruction.path, name)


def _intrinsic_pattern(instruction) -> dict:
    for pattern in instruction.patterns:
        if pattern.get("kind") == "intrinsic_to_pseudo":
            return pattern
    raise ValueError(f"{instruction.path}: builtin.codegen requires intrinsic pattern")


def _validate_codegen_builtin_name(path, name: str) -> None:
    if not name.startswith("ysx_"):
        raise ValueError(f"{path}: builtin.codegen name must start with ysx_: {name}")
    if not name.endswith(("_i32m1", "_f32m1")):
        raise ValueError(f"{path}: unsupported builtin vector type: {name}")
    if "_vv_" in name or "_vs_" in name or "_vx_" in name or "_v_" in name:
        return
    raise ValueError(f"{path}: unsupported builtin prototype shape: {name}")
