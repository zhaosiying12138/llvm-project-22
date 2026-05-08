from pathlib import Path
import re
import unittest


YSX_ROOT = Path(__file__).resolve().parents[2]
CMAKE = YSX_ROOT / "CMakeLists.txt"
INSTR_INFO = YSX_ROOT / "YSXInstrInfo.td"

GENERATED_OUTPUTS = (
    "YSXGenAutoTinyFInstrInfo.inc",
    "YSXGenAutoTinyVInstrInfo.inc",
    "YSXGenAutoTinyVPseudos.inc",
    "YSXGenAutoTinyVPatterns.inc",
    "YSXGenAutoTinyVBuiltins.inc",
)

LLVM_TARGET_INCLUDES = GENERATED_OUTPUTS[:-1]


class CMakeIntegrationTest(unittest.TestCase):
    def test_cmake_defines_build_tree_auto_td_outputs(self):
        text = CMAKE.read_text()

        self.assertIn(
            "set(YSX_AUTO_TD_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/auto-td)",
            text,
        )
        for output in GENERATED_OUTPUTS:
            self.assertRegex(
                text,
                rf"set\(YSX_AUTO_TD_[A-Z_]+"
                rf" \$\{{YSX_AUTO_TD_OUT_DIR\}}/{re.escape(output)}\)",
            )
            self.assertIn(output, text)

    def test_cmake_invokes_generator_with_required_inputs(self):
        text = CMAKE.read_text()

        self.assertIn("add_custom_command(", text)
        self.assertIn("ysx_auto_td_gen.py", text)
        for arg in (
            "--ysx-root",
            "--riscv-opcodes",
            "--ysx-opcodes",
            "--out-dir",
            "--coverage",
        ):
            self.assertIn(arg, text)
        self.assertIn("${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes", text)
        self.assertIn("${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes", text)

    def test_cmake_tracks_generator_yaml_and_opcode_dependencies(self):
        text = CMAKE.read_text()

        self.assertRegex(
            text,
            r"(?s)file\(GLOB_RECURSE\s+YSX_AUTO_TD_PY_DEPS\s+CONFIGURE_DEPENDS"
            r".*auto-td/tools/\*\.py.*auto-td/tools/\*/\*\.py",
        )
        self.assertRegex(
            text,
            r"(?s)file\(GLOB_RECURSE\s+YSX_AUTO_TD_YAML_DEPS\s+CONFIGURE_DEPENDS"
            r".*auto-td/instructions/\*\.yaml.*auto-td/instructions/\*/\*\.yaml"
            r".*auto-td/schema/\*\.yaml.*auto-td/taxonomy/\*\.yaml",
        )
        self.assertRegex(
            text,
            r"(?s)file\(GLOB\s+YSX_AUTO_TD_OPCODE_DEPS\s+CONFIGURE_DEPENDS"
            r".*third_party/riscv-opcodes/extensions/\*"
            r".*third_party/ysx-opcodes/extensions/\*",
        )
        self.assertRegex(
            text,
            r"(?s)DEPENDS\s+.*YSX_AUTO_TD_PY_DEPS.*YSX_AUTO_TD_YAML_DEPS"
            r".*YSX_AUTO_TD_OPCODE_DEPS",
        )

    def test_generated_outputs_are_target_depends_before_tablegen(self):
        text = CMAKE.read_text()
        first_tablegen = text.index("tablegen(")
        self.assertIn("list(APPEND LLVM_TARGET_DEPENDS", text)
        target_depends = text.index("list(APPEND LLVM_TARGET_DEPENDS")

        self.assertLess(target_depends, first_tablegen)
        for output in GENERATED_OUTPUTS:
            self.assertIn(output, text[target_depends:first_tablegen])

    def test_ysx_instr_info_includes_target_fragments_only(self):
        text = INSTR_INFO.read_text()

        for include in LLVM_TARGET_INCLUDES:
            self.assertIn(f'include "{include}"', text)
        self.assertNotIn('include "YSXGenAutoTinyVBuiltins.inc"', text)


if __name__ == "__main__":
    unittest.main()
