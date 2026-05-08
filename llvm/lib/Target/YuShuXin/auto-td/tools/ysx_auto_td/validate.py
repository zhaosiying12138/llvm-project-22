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
ALLOWED_STATUS = {
    "auto_full",
    "auto_with_structured_override",
    "retained_schema_gap",
}


def validate_instruction_set(instructions):
    for instruction in instructions:
        text = instruction.path.read_text()
        for token in FORBIDDEN:
            if token in text:
                raise ValueError(f"{instruction.path}: forbidden token {token}")
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
