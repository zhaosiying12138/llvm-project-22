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

GENERATED_OUTPUT_VARS = (
    "YSX_AUTO_TD_TINYF_INSTRINFO",
    "YSX_AUTO_TD_TINYV_INSTRINFO",
    "YSX_AUTO_TD_TINYV_PSEUDOS",
    "YSX_AUTO_TD_TINYV_PATTERNS",
    "YSX_AUTO_TD_TINYV_BUILTINS",
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

    def test_custom_command_outputs_generated_fragments_and_coverage(self):
        text = CMAKE.read_text()
        outputs_list = self._cmake_call(text, "set", "YSX_AUTO_TD_OUTPUTS")
        custom_command = self._cmake_call(text, "add_custom_command")
        output_section = self._cmake_section(
            custom_command,
            "OUTPUT",
            ("COMMAND", "DEPENDS", "VERBATIM", "COMMENT"),
        )

        self.assertIn("${YSX_AUTO_TD_OUTPUTS}", output_section)
        self.assertIn("${YSX_AUTO_TD_COVERAGE}", output_section)
        for var in GENERATED_OUTPUT_VARS:
            self.assertIn(f"${{{var}}}", outputs_list)

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

    def test_cmake_registers_auto_td_guard_verify_step(self):
        text = CMAKE.read_text()

        # A build-time target runs the auto-td guard suite so a plain build gates
        # on the no-handwritten-instruction-TD promise, not just `check-llvm`.
        self.assertIn("add_custom_target(YSXAutoTdGuards", text)
        self.assertIn("add_dependencies(YSXCommonTableGen YSXAutoTdGuards)", text)
        self.assertRegex(text, r"(?s)-m\s+unittest\s+discover\s+.*auto-td/tests")

        # The guard re-runs when the tests, generator, YAML, or backend .td change.
        self.assertIn("YSX_AUTO_TD_GUARD_TEST_DEPS", text)
        self.assertIn("YSX_AUTO_TD_SOURCE_TD_DEPS", text)

    def _cmake_call(self, text, name, first_arg=None):
        if first_arg is None:
            pattern = rf"\b{re.escape(name)}\s*\("
        else:
            pattern = rf"\b{re.escape(name)}\s*\(\s*{re.escape(first_arg)}\b"
        match = re.search(pattern, text)
        self.assertIsNotNone(match, f"missing CMake call {name}({first_arg or ''}")
        start = match.start()
        depth = 0
        for index in range(start, len(text)):
            char = text[index]
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    return text[start : index + 1]
        self.fail(f"unterminated CMake call {name}({first_arg or ''}")

    def _cmake_section(self, call, section, terminators):
        section_match = re.search(rf"\b{re.escape(section)}\b", call)
        self.assertIsNotNone(section_match, f"missing CMake section {section}")
        start = section_match.end()
        end = len(call)
        for terminator in terminators:
            terminator_match = re.search(rf"\b{re.escape(terminator)}\b", call[start:])
            if terminator_match:
                end = min(end, start + terminator_match.start())
        return call[start:end]


if __name__ == "__main__":
    unittest.main()
