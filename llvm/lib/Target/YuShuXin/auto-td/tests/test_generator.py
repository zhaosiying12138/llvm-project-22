import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from ysx_auto_td.loader import load_instruction_set
from ysx_auto_td.model import InstructionRecord, OpcodeRecord, OpcodeSource
from ysx_auto_td.opcodes import parse_opcode_file, source_key_from_mnemonic
from ysx_auto_td.report import write_coverage
from ysx_auto_td.validate import validate_instruction_set


YSX_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = Path(__file__).resolve().parents[6]
TOOL = TOOLS_DIR / "ysx_auto_td_gen.py"
STUBS = (
    "YSXGenAutoTinyFInstrInfo.inc",
    "YSXGenAutoTinyVInstrInfo.inc",
    "YSXGenAutoTinyVPseudos.inc",
    "YSXGenAutoTinyVPatterns.inc",
    "YSXGenAutoTinyVBuiltins.inc",
)


class GeneratorTest(unittest.TestCase):
    def test_source_key_from_mnemonic_normalizes_tablegen_names(self):
        self.assertEqual(source_key_from_mnemonic("vadd.vv"), "vadd_vv")
        self.assertEqual(source_key_from_mnemonic("yushuxin.vfexp"), "yushuxin_vfexp")
        self.assertEqual(source_key_from_mnemonic("custom-dot.op"), "custom_dot_op")

    def test_parse_opcode_file_splits_fields_and_fixed_bits(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            opcode_file = Path(tmpdir) / "rv_v"
            opcode_file.write_text(
                "\n".join(
                    [
                        "# comment",
                        "$pseudo_op rv_v::vadd.vv custom 31..26=0x00 vd",
                        "vadd.vv 31..26=0x00 vm vs2 vs1 14..12=0x0 vd 6..0=0x57",
                    ]
                )
                + "\n"
            )

            records = parse_opcode_file(opcode_file)

        self.assertEqual(records["vadd_vv"].mnemonic, "vadd.vv")
        self.assertEqual(records["vadd_vv"].fields, ("vm", "vs2", "vs1", "vd"))
        self.assertEqual(
            records["vadd_vv"].fixed_bits,
            ("31..26=0x00", "14..12=0x0", "6..0=0x57"),
        )

    def test_loader_reports_missing_opcode_repo(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: bad\n"
                "opcode_source: {repo: missing-opcodes, extension: rv_v, key: bad}\n"
                "spec_ref: test.bad\n",
            )

            with self.assertRaisesRegex(ValueError, "unknown opcode repo 'missing-opcodes'"):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_reports_missing_opcode_key(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: bad\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: missing_key}\n"
                "spec_ref: test.bad\n",
            )

            with self.assertRaisesRegex(
                ValueError, "missing opcode source riscv-opcodes/rv_v/missing_key"
            ):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_validator_rejects_forbidden_raw_td_token(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text(
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: test.bad\n"
                "raw_td: def BAD\n"
            )

            instruction = self._instruction(path)
            with self.assertRaisesRegex(ValueError, "forbidden token raw_td"):
                validate_instruction_set([instruction])

    def test_generator_cli_rejects_forbidden_raw_td_token(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: test.bad\n"
                "raw_td: def BAD\n",
            )

            result = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--ysx-root",
                    str(ysx_root),
                    "--riscv-opcodes",
                    str(tmp / "riscv-opcodes"),
                    "--ysx-opcodes",
                    str(tmp / "ysx-opcodes"),
                    "--out-dir",
                    str(tmp / "out"),
                    "--coverage",
                    str(tmp / "coverage.md"),
                ],
                env=self._generator_env(),
                stderr=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
            )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("forbidden token raw_td", result.stderr)

    def test_validator_rejects_forbidden_yaml_fields(self):
        for field in ("raw_cpp", "encoding", "fixed_bits"):
            with self.subTest(field=field):
                with tempfile.TemporaryDirectory() as tmpdir:
                    path = Path(tmpdir) / "bad.yaml"
                    path.write_text(
                        "mnemonic: vadd.vv\n"
                        "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                        "spec_ref: test.bad\n"
                        f"{field}: copied fact\n"
                    )
                    instruction = self._instruction(path)

                    with self.assertRaisesRegex(
                        ValueError, f"forbidden field {field}"
                    ):
                        validate_instruction_set([instruction])

    def test_validator_rejects_invalid_status(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(path, status="handwritten")

            with self.assertRaisesRegex(ValueError, "invalid status handwritten"):
                validate_instruction_set([instruction])

    def test_validator_requires_owner_for_retained_schema_gap(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(path, status="retained_schema_gap")

            with self.assertRaisesRegex(
                ValueError, "retained_schema_gap requires retained_owner_files"
            ):
                validate_instruction_set([instruction])

    def test_coverage_lists_instruction_status_and_opcode_source(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            coverage = tmp / "coverage.md"
            write_coverage(
                coverage,
                [
                    self._instruction(tmp / "vadd_vv.yaml"),
                    self._instruction(
                        tmp / "yushuxin_vfexp.yaml",
                        mnemonic="yushuxin.vfexp",
                        source=OpcodeSource("ysx-opcodes", "rv_xtinyv", "yushuxin_vfexp"),
                        opcode=OpcodeRecord(
                            "yushuxin_vfexp",
                            "yushuxin.vfexp",
                            ("vm", "vs2", "vd"),
                            ("31..26=0x2a", "6..0=0x0b"),
                            tmp / "rv_xtinyv",
                        ),
                    ),
                ],
            )

            text = coverage.read_text()

        self.assertIn("| instruction | status | opcode source | yaml |", text)
        self.assertIn("| vadd.vv | auto_full | riscv-opcodes/rv_v/vadd_vv |", text)
        self.assertIn("vadd_vv.yaml", text)
        self.assertIn(
            "| yushuxin.vfexp | auto_full | ysx-opcodes/rv_xtinyv/yushuxin_vfexp |",
            text,
        )
        self.assertIn("yushuxin_vfexp.yaml", text)

    def test_generator_writes_stubs_and_current_coverage(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            out = tmp / "out"
            coverage = tmp / "coverage.md"

            subprocess.check_call(
                [
                    "python3",
                    str(TOOL),
                    "--ysx-root",
                    str(YSX_ROOT),
                    "--riscv-opcodes",
                    str(REPO_ROOT / "third_party" / "riscv-opcodes"),
                    "--ysx-opcodes",
                    str(REPO_ROOT / "third_party" / "ysx-opcodes"),
                    "--out-dir",
                    str(out),
                    "--coverage",
                    str(coverage),
                ],
                env=self._generator_env(),
            )

            for stub in STUBS:
                self.assertEqual(
                    (out / stub).read_text(),
                    "// generated by ysx_auto_td_gen\n",
                )
            text = coverage.read_text()

        for mnemonic in ("vle32.v", "vse32.v", "vadd.vv", "vfredusum.vs", "yushuxin.vfexp"):
            self.assertIn(mnemonic, text)
        self.assertIn("ysx-opcodes/rv_xtinyv/yushuxin_vfexp", text)
        self.assertIn(
            "llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml",
            text,
        )
        self.assertNotIn(str(YSX_ROOT), text)

    def _instruction(
        self,
        path,
        mnemonic="vadd.vv",
        status="auto_full",
        source=None,
        opcode=None,
    ):
        source = source or OpcodeSource("riscv-opcodes", "rv_v", "vadd_vv")
        opcode = opcode or OpcodeRecord(
            "vadd_vv",
            "vadd.vv",
            ("vm", "vs2", "vs1", "vd"),
            ("31..26=0x00", "14..12=0x0", "6..0=0x57"),
            Path("rv_v"),
        )
        return InstructionRecord(
            path=Path(path),
            mnemonic=mnemonic,
            opcode_source=source,
            opcode=opcode,
            spec_ref="test.spec",
            status=status,
        )

    def _write_instruction(self, ysx_root, name, text):
        path = ysx_root / "auto-td" / "instructions" / "tiny-v" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def _write_opcode(self, root, extension, text):
        path = root / "extensions" / extension
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text + "\n")

    def _generator_env(self):
        env = os.environ.copy()
        existing = env.get("PYTHONPATH")
        env["PYTHONPATH"] = (
            str(TOOLS_DIR) if not existing else str(TOOLS_DIR) + os.pathsep + existing
        )
        return env


if __name__ == "__main__":
    unittest.main()
