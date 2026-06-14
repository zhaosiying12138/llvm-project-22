import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from ysx_auto_td.loader import load_instruction_set
from ysx_auto_td.model import (
    InstructionRecord,
    OpcodeFieldRange,
    OpcodeRecord,
    OpcodeSource,
)
from ysx_auto_td.opcodes import (
    load_arg_lut,
    load_opcode_repo,
    parse_opcode_file,
    source_key_from_mnemonic,
)
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

    def test_parse_opcode_file_attaches_arg_lut_field_ranges(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            (tmp / "arg_lut.csv").write_text('"vm", 25, 25\n"vd", 11, 7\n')
            opcode_file = tmp / "rv_v"
            opcode_file.write_text("test.v vm vd 6..0=0x57\n")

            records = parse_opcode_file(opcode_file, load_arg_lut(tmp))

        self.assertEqual(
            records["test_v"].field_ranges,
            (
                OpcodeFieldRange("vm", 25, 25),
                OpcodeFieldRange("vd", 11, 7),
            ),
        )

    def test_load_opcode_repo_ignores_editor_temp_files(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            extensions = tmp / "extensions"
            extensions.mkdir(parents=True)
            (extensions / ".rv_v.swp").write_bytes(b"b0VIM 9.0\x00\xed")
            (extensions / "rv_v").write_text("vadd.vv 31..26=0x00 vd 6..0=0x57\n")

            records = load_opcode_repo(tmp)

        self.assertIn(("rv_v", "vadd_vv"), records)
        self.assertNotIn((".rv_v.swp", "vadd_vv"), records)

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

    def test_loader_rejects_malformed_alias_entries(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: tinyv.vector-alu.int_add\n"
                "aliases: [{name: missing_mnemonic}]\n",
            )

            with self.assertRaisesRegex(ValueError, "aliases\\[0\\] must define mnemonic"):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_rejects_incomplete_intrinsic_pattern_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: tinyv.vector-alu.int_add\n"
                "patterns:\n"
                "  - {kind: intrinsic_to_pseudo, intrinsic: ysx.vadd}\n",
            )

            with self.assertRaisesRegex(
                ValueError, "patterns\\[0\\] must define operation"
            ):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_rejects_incomplete_pseudo_matrix_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: tinyv.vector-alu.int_add\n"
                "pseudos:\n"
                "  matrix: {element_types: [i32], lmuls: standard, masked: yes}\n",
            )

            with self.assertRaisesRegex(
                ValueError, "pseudos.matrix must define policy"
            ):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_rejects_non_boolean_builtin_overloaded_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: tinyv.vector-alu.int_add\n"
                "builtin:\n"
                "  header: ysx_vector.h\n"
                "  names: [ysx_vadd_vv_i32m1]\n"
                "  overloaded: \"false\"\n",
            )

            with self.assertRaisesRegex(ValueError, "builtin.overloaded must be a bool"):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_rejects_non_boolean_builtin_codegen_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            ysx_root = tmp / "YuShuXin"
            self._write_opcode(tmp / "riscv-opcodes", "rv_v", "vadd.vv 31..26=0x00 vd")
            self._write_instruction(
                ysx_root,
                "bad.yaml",
                "mnemonic: vadd.vv\n"
                "opcode_source: {repo: riscv-opcodes, extension: rv_v, key: vadd_vv}\n"
                "spec_ref: tinyv.vector-alu.int_add\n"
                "builtin:\n"
                "  header: ysx_vector.h\n"
                "  names: [ysx_vadd_vv_i32m1]\n"
                "  codegen: \"true\"\n",
            )

            with self.assertRaisesRegex(ValueError, "builtin.codegen must be a bool"):
                load_instruction_set(ysx_root, tmp / "riscv-opcodes", tmp / "ysx-opcodes")

    def test_loader_attaches_taxonomy_operands_and_effects(self):
        instructions = load_instruction_set(
            YSX_ROOT,
            REPO_ROOT / "third_party" / "riscv-opcodes",
            REPO_ROOT / "third_party" / "ysx-opcodes",
        )
        by_name = {instruction.mnemonic: instruction for instruction in instructions}

        vadd = by_name["vadd.vv"]
        self.assertEqual([operand.field for operand in vadd.operands_out], ["vd"])
        self.assertEqual([operand.field for operand in vadd.operands_in], ["vs2", "vs1", "vm"])
        self.assertEqual(vadd.operands_in[-1].operand, "VMaskOp")
        self.assertFalse(vadd.effects.may_load)
        self.assertFalse(vadd.effects.may_store)
        self.assertEqual(vadd.effects.implicit_uses, ("VL", "VTYPE"))

        vse = by_name["vse32.v"]
        self.assertEqual([operand.role for operand in vse.operands_in], ["value", "base", "mask_policy"])
        self.assertTrue(vse.effects.may_store)

        vlse = by_name["vlse32.v"]
        self.assertEqual([operand.role for operand in vlse.operands_in], ["base", "stride", "mask_policy"])
        self.assertEqual(vlse.operands_in[1].field, "rs2")
        self.assertEqual(vlse.operands_in[1].reg_class, "GPR")
        self.assertTrue(vlse.effects.may_load)

        vluxei = by_name["vluxei32.v"]
        self.assertEqual([operand.role for operand in vluxei.operands_in], ["base", "indices", "mask_policy"])
        self.assertEqual(vluxei.operands_in[1].field, "vs2")
        self.assertEqual(vluxei.operands_in[1].reg_class, "VR")
        self.assertTrue(vluxei.effects.may_load)

        vmerge = by_name["vmerge.vvm"]
        self.assertEqual([operand.field for operand in vmerge.operands_in], ["vs2", "vs1", "vm"])
        self.assertEqual(vmerge.operands_in[-1].operand, "VMaskCarryInOp")

        vmseq = by_name["vmseq.vv"]
        self.assertEqual(vmseq.operands_out[0].role, "mask_dest")
        self.assertEqual([operand.field for operand in vmseq.operands_in], ["vs2", "vs1", "vm"])

        vslideup = by_name["vslideup.vx"]
        self.assertEqual([operand.field for operand in vslideup.operands_in], ["vs2", "rs1", "vm"])
        self.assertEqual(vslideup.operands_in[1].reg_class, "GPR")

        vmv_x = by_name["vmv.v.x"]
        self.assertEqual([operand.field for operand in vmv_x.operands_in], ["rs1"])
        self.assertEqual(vmv_x.operands_in[0].reg_class, "GPR")

        vsetvli = by_name["vsetvli"]
        self.assertEqual([operand.field for operand in vsetvli.operands_out], ["rd"])
        self.assertEqual([operand.field for operand in vsetvli.operands_in], ["rs1", "zimm11"])
        self.assertEqual(vsetvli.operands_in[0].reg_class, "GPR")
        self.assertEqual(vsetvli.operands_in[1].operand, "UImm11")
        self.assertTrue(vsetvli.effects.has_side_effects)
        self.assertEqual(vsetvli.effects.implicit_defs, ("VL", "VTYPE"))

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

    def test_validator_rejects_codegen_builtin_without_intrinsic_pattern(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(
                path,
                builtin={
                    "header": "ysx_vector.h",
                    "names": ("ysx_vadd_vv_i32m1",),
                    "overloaded": False,
                    "codegen": True,
                },
            )

            with self.assertRaisesRegex(
                ValueError, "builtin.codegen requires intrinsic pattern"
            ):
                validate_instruction_set([instruction])

    def test_validator_rejects_codegen_builtin_unsupported_operation(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(
                path,
                patterns=(
                    {
                        "kind": "intrinsic_to_pseudo",
                        "intrinsic": "ysx.vadd",
                        "operation": "wide_loop",
                    },
                ),
                builtin={
                    "header": "ysx_vector.h",
                    "names": ("ysx_vadd_vv_i32m1",),
                    "overloaded": False,
                    "codegen": True,
                },
            )

            with self.assertRaisesRegex(
                ValueError, "unsupported builtin.codegen operation wide_loop"
            ):
                validate_instruction_set([instruction])

    def test_validator_rejects_codegen_builtin_bad_name_prefix(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(
                path,
                patterns=(
                    {
                        "kind": "intrinsic_to_pseudo",
                        "intrinsic": "ysx.vadd",
                        "operation": "add",
                    },
                ),
                builtin={
                    "header": "ysx_vector.h",
                    "names": ("riscv_vadd_vv_i32m1",),
                    "overloaded": False,
                    "codegen": True,
                },
            )

            with self.assertRaisesRegex(
                ValueError, "builtin.codegen name must start with ysx_"
            ):
                validate_instruction_set([instruction])

    def test_validator_rejects_codegen_builtin_unsupported_type(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "bad.yaml"
            path.write_text("mnemonic: vadd.vv\n")
            instruction = self._instruction(
                path,
                patterns=(
                    {
                        "kind": "intrinsic_to_pseudo",
                        "intrinsic": "ysx.vadd",
                        "operation": "add",
                    },
                ),
                builtin={
                    "header": "ysx_vector.h",
                    "names": ("ysx_vadd_vv_i64m1",),
                    "overloaded": False,
                    "codegen": True,
                },
            )

            with self.assertRaisesRegex(
                ValueError, "unsupported builtin vector type"
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

    def test_generator_writes_real_tinyv_instrinfo_from_opcode_sources(self):
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
                    "--clang-builtins-td",
                    str(out / "YSXGenAutoTinyVClangBuiltins.td"),
                    "--clang-builtin-cg-inc",
                    str(out / "YSXGenAutoTinyVBuiltinCG.inc"),
                    "--llvm-intrinsics-td",
                    str(out / "YSXGenAutoTinyVIntrinsics.td"),
                ],
                env=self._generator_env(),
            )

            for stub in STUBS:
                self.assertTrue((out / stub).is_file(), stub)
            tinyv = (out / "YSXGenAutoTinyVInstrInfo.inc").read_text()
            pseudos = (out / "YSXGenAutoTinyVPseudos.inc").read_text()
            patterns = (out / "YSXGenAutoTinyVPatterns.inc").read_text()
            builtins = (out / "YSXGenAutoTinyVBuiltins.inc").read_text()
            clang_builtins = (out / "YSXGenAutoTinyVClangBuiltins.td").read_text()
            clang_codegen = (out / "YSXGenAutoTinyVBuiltinCG.inc").read_text()
            llvm_intrinsics = (out / "YSXGenAutoTinyVIntrinsics.td").read_text()
            text = coverage.read_text()

        self.assertIn("def YSXAutoVMaskAsmOperand", tinyv)
        self.assertIn("def YSXAutoVMaskOp", tinyv)
        self.assertIn("def YSXAutoVMaskCarryInAsmOperand", tinyv)
        self.assertIn("def YSXAutoVMaskCarryInOp", tinyv)
        self.assertIn('let DecoderMethod = "decodeVMaskCarryInReg";', tinyv)
        self.assertIn('let OperandNamespace = "YSXOp";', tinyv)
        self.assertIn('let OperandType = "OPERAND_VMASK";', tinyv)
        self.assertIn("def YSX_AUTO_VADD_VV", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs2, VR:$vs1, YSXAutoVMaskOp:$vm), "vadd.vv"', tinyv)
        self.assertIn("let Inst{31-26} = 0b000000;", tinyv)
        self.assertIn("let Inst{25} = vm;", tinyv)
        self.assertIn("let Inst{6-0} = 0b1010111;", tinyv)
        self.assertIn("def YSX_AUTO_VLE32_V", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins GPRMemZeroOffset:$rs1, YSXAutoVMaskOp:$vm), "vle32.v", "$vd, $rs1$vm"', tinyv)
        self.assertIn("let Inst{31-29} = 0b000;", tinyv)
        self.assertIn("let Inst{24-20} = 0b00000;", tinyv)
        self.assertIn("def YSX_AUTO_VSE32_V", tinyv)
        self.assertIn('RVInst<(outs), (ins VR:$vs3, GPRMemZeroOffset:$rs1, YSXAutoVMaskOp:$vm), "vse32.v", "$vs3, $rs1$vm"', tinyv)
        self.assertIn("def YSX_AUTO_VLSE32_V", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins GPRMemZeroOffset:$rs1, GPR:$rs2, YSXAutoVMaskOp:$vm), "vlse32.v", "$vd, $rs1, $rs2$vm"', tinyv)
        self.assertIn("def YSX_AUTO_VSSE32_V", tinyv)
        self.assertIn('RVInst<(outs), (ins VR:$vs3, GPRMemZeroOffset:$rs1, GPR:$rs2, YSXAutoVMaskOp:$vm), "vsse32.v", "$vs3, $rs1, $rs2$vm"', tinyv)
        self.assertIn("def YSX_AUTO_VLUXEI32_V", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins GPRMemZeroOffset:$rs1, VR:$vs2, YSXAutoVMaskOp:$vm), "vluxei32.v", "$vd, $rs1, $vs2$vm"', tinyv)
        self.assertIn("def YSX_AUTO_VSUXEI32_V", tinyv)
        self.assertIn('RVInst<(outs), (ins VR:$vs3, GPRMemZeroOffset:$rs1, VR:$vs2, YSXAutoVMaskOp:$vm), "vsuxei32.v", "$vs3, $rs1, $vs2$vm"', tinyv)
        for record in (
            "YSX_AUTO_VSUB_VV",
            "YSX_AUTO_VMUL_VV",
            "YSX_AUTO_VMIN_VV",
            "YSX_AUTO_VMAX_VV",
            "YSX_AUTO_VAND_VV",
            "YSX_AUTO_VOR_VV",
            "YSX_AUTO_VXOR_VV",
            "YSX_AUTO_VMSEQ_VV",
            "YSX_AUTO_VMSLT_VV",
        ):
            self.assertIn(f"def {record}", tinyv)
        self.assertIn("def YSX_AUTO_VMERGE_VVM", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs2, VR:$vs1, YSXAutoVMaskCarryInOp:$vm), "vmerge.vvm", "$vd, $vs2, $vs1, $vm"', tinyv)
        self.assertIn("def YSX_AUTO_VREDSUM_VS", tinyv)
        self.assertIn("def YSX_AUTO_VREDMIN_VS", tinyv)
        self.assertIn("def YSX_AUTO_VREDMAX_VS", tinyv)
        self.assertIn("def YSX_AUTO_VREDAND_VS", tinyv)
        self.assertIn("def YSX_AUTO_VREDOR_VS", tinyv)
        self.assertIn("def YSX_AUTO_VREDXOR_VS", tinyv)
        self.assertIn("def YSX_AUTO_VFREDMIN_VS", tinyv)
        self.assertIn("def YSX_AUTO_VFREDMAX_VS", tinyv)
        self.assertIn("def YSX_AUTO_VSLIDEUP_VX", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs2, GPR:$rs1, YSXAutoVMaskOp:$vm), "vslideup.vx", "$vd, $vs2, $rs1$vm"', tinyv)
        self.assertIn("def YSX_AUTO_VSLIDEDOWN_VX", tinyv)
        self.assertIn("def YSX_AUTO_VRGATHER_VV", tinyv)
        self.assertIn("def YSX_AUTO_VMV_V_X", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins GPR:$rs1), "vmv.v.x", "$vd, $rs1"', tinyv)
        self.assertIn("def YSX_AUTO_VMV_V_V", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs1), "vmv.v.v", "$vd, $vs1"', tinyv)
        self.assertIn("def YSX_AUTO_VFREDUSUM_VS", tinyv)
        self.assertIn('def : MnemonicAlias<"vfredsum.vs", "vfredusum.vs">;', tinyv)
        self.assertIn("def YSX_AUTO_VSETIVLI", tinyv)
        self.assertIn('RVInst<(outs GPR:$rd), (ins uimm5:$zimm5, uimm10:$zimm10), "vsetivli"', tinyv)
        self.assertIn("let Defs = [VL, VTYPE];", tinyv)
        self.assertIn("bits<10> zimm10;", tinyv)
        self.assertIn("let Inst{31} = 0b1;", tinyv)
        self.assertIn("def YSX_AUTO_VSETVLI", tinyv)
        self.assertIn('RVInst<(outs GPR:$rd), (ins GPR:$rs1, uimm11:$zimm11), "vsetvli"', tinyv)
        self.assertIn("bits<11> zimm11;", tinyv)
        self.assertIn("def YSX_AUTO_VMV1R_V", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs2), "vmv1r.v"', tinyv)
        self.assertIn("let Inst{31-26} = 0b100111;", tinyv)
        self.assertIn("def YSX_AUTO_YUSHUXIN_VFEXP", tinyv)
        self.assertIn('RVInst<(outs VR:$vd), (ins VR:$vs2, YSXAutoVMaskOp:$vm), "yushuxin.vfexp"', tinyv)
        self.assertIn("let Inst{31-26} = 0b101010;", tinyv)
        self.assertIn("let Inst{19-15} = 0b00000;", tinyv)
        self.assertIn("let Inst{6-0} = 0b0001011;", tinyv)
        self.assertIn("let Predicates = [HasStdExtXTinyV];", tinyv)
        self.assertIn("let Uses = [VL, VTYPE];", tinyv)
        self.assertIn("// opcode-source: ysx-opcodes/rv_xtinyv/yushuxin_vfexp", tinyv)
        self.assertNotIn("RVInstVV", tinyv)
        self.assertNotIn("VUnitStrideLoad", tinyv)
        self.assertNotIn("VPseudo", tinyv)
        self.assertNotIn("no generated records yet", pseudos)
        self.assertNotIn("no generated records yet", patterns)
        self.assertNotIn("no generated records yet", builtins)
        self.assertIn(
            "// auto-td-pseudo: yushuxin.vfexp matrix element_types=f32 lmuls=standard masked=true policy=llvm_default",
            pseudos,
        )
        self.assertIn(
            "// auto-td-pattern: yushuxin.vfexp intrinsic=ysx.vfexp operation=fexp record=YSX_AUTO_YUSHUXIN_VFEXP",
            patterns,
        )
        self.assertIn(
            "// auto-td-builtin: yushuxin.vfexp header=ysx_vector.h name=ysx_vfexp_v_f32m1 overloaded=false codegen=true record=YSX_AUTO_YUSHUXIN_VFEXP",
            builtins,
        )
        self.assertIn(
            "// auto-td-builtin: vadd.vv header=ysx_vector.h name=ysx_vadd_vv_i32m1 overloaded=false codegen=true record=YSX_AUTO_VADD_VV",
            builtins,
        )
        self.assertIn(
            'def vadd_vv_i32m1 : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)", "xtinyv,zvl128b">;',
            clang_builtins,
        )
        self.assertIn(
            'def vfexp_v_f32m1 : YSXBuiltin<"_ExtVector<4, float>(_ExtVector<4, float>, unsigned long int)", "xtinyv,zvl128b">;',
            clang_builtins,
        )
        self.assertIn("case YSX::BI__builtin_ysx_vadd_vv_i32m1:", clang_codegen)
        self.assertIn("Intrinsic::ysx_vadd", clang_codegen)
        self.assertIn("case YSX::BI__builtin_ysx_vfexp_v_f32m1:", clang_codegen)
        self.assertIn("Intrinsic::ysx_vfexp", clang_codegen)
        self.assertIn("def int_ysx_vadd", llvm_intrinsics)
        self.assertIn("def int_ysx_vfexp", llvm_intrinsics)

        for mnemonic in (
            "vle32.v",
            "vse32.v",
            "vadd.vv",
            "vfredusum.vs",
            "vsetivli",
            "vsetvli",
            "vmv1r.v",
            "yushuxin.vfexp",
            "vlse32.v",
            "vsse32.v",
            "vluxei32.v",
            "vsuxei32.v",
            "vsub.vv",
            "vmul.vv",
            "vmin.vv",
            "vmax.vv",
            "vand.vv",
            "vor.vv",
            "vxor.vv",
            "vmseq.vv",
            "vmslt.vv",
            "vmerge.vvm",
            "vredsum.vs",
            "vredmin.vs",
            "vredmax.vs",
            "vredand.vs",
            "vredor.vs",
            "vredxor.vs",
            "vfredmin.vs",
            "vfredmax.vs",
            "vslideup.vx",
            "vslidedown.vx",
            "vrgather.vv",
            "vmv.v.x",
            "vmv.v.v",
        ):
            self.assertIn(mnemonic, text)
        self.assertIn("ysx-opcodes/rv_xtinyv/yushuxin_vfexp", text)
        self.assertIn(
            "llvm/lib/Target/YuShuXin/auto-td/instructions/tiny-v/yushuxin_vfexp.yaml",
            text,
        )
        self.assertNotIn(str(YSX_ROOT), text)

    def test_generator_writes_real_tinyf_instrinfo_from_opcode_sources(self):
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

            tinyf = (out / "YSXGenAutoTinyFInstrInfo.inc").read_text()
            text = coverage.read_text()

        self.assertIn("def YSX_AUTO_FADD_S", tinyf)
        self.assertIn('RVInst<(outs FPR32:$rd), (ins FPR32:$rs1, FPR32:$rs2, uimm3:$rm), "fadd.s"', tinyf)
        self.assertIn("def YSX_AUTO_FSUB_S", tinyf)
        self.assertIn("def YSX_AUTO_FMUL_S", tinyf)
        self.assertIn("def YSX_AUTO_FEQ_S", tinyf)
        self.assertIn('RVInst<(outs GPR:$rd), (ins FPR32:$rs1, FPR32:$rs2), "feq.s"', tinyf)
        self.assertIn("def YSX_AUTO_FLT_S", tinyf)
        self.assertIn("def YSX_AUTO_FLE_S", tinyf)
        self.assertIn("def YSX_AUTO_FCVT_W_S", tinyf)
        self.assertIn('RVInst<(outs GPR:$rd), (ins FPR32:$rs1, uimm3:$rm), "fcvt.w.s"', tinyf)
        self.assertIn("def YSX_AUTO_FCVT_WU_S", tinyf)
        self.assertIn("def YSX_AUTO_FCVT_S_W", tinyf)
        self.assertIn('RVInst<(outs FPR32:$rd), (ins GPR:$rs1, uimm3:$rm), "fcvt.s.w"', tinyf)
        self.assertIn("def YSX_AUTO_FCVT_S_WU", tinyf)
        self.assertIn("def YSX_AUTO_FSGNJ_S", tinyf)
        self.assertIn('RVInst<(outs FPR32:$rd), (ins FPR32:$rs1, FPR32:$rs2), "fsgnj.s"', tinyf)
        self.assertIn("def YSX_AUTO_FMV_X_W", tinyf)
        self.assertIn('RVInst<(outs GPR:$rd), (ins FPR32:$rs1), "fmv.x.w"', tinyf)
        self.assertIn("def YSX_AUTO_FMV_W_X", tinyf)
        self.assertIn('RVInst<(outs FPR32:$rd), (ins GPR:$rs1), "fmv.w.x"', tinyf)
        self.assertIn("def YSX_AUTO_FLW", tinyf)
        self.assertIn('RVInst<(outs FPR32:$rd), (ins GPRMem:$rs1, simm12_lo:$imm12), "flw"', tinyf)
        self.assertIn('"$rd, ${imm12}(${rs1})"', tinyf)
        self.assertIn("let mayLoad = 1;", tinyf)
        self.assertIn("let Inst{31-20} = imm12;", tinyf)
        self.assertIn("def YSX_AUTO_FSW", tinyf)
        self.assertIn('RVInst<(outs), (ins FPR32:$rs2, GPRMem:$rs1, simm12_lo:$imm12), "fsw"', tinyf)
        self.assertIn('"$rs2, ${imm12}(${rs1})"', tinyf)
        self.assertIn("let mayStore = 1;", tinyf)
        self.assertIn("let Inst{31-25} = imm12{11-5};", tinyf)
        self.assertIn("let Inst{11-7} = imm12{4-0};", tinyf)
        self.assertIn("let Predicates = [HasStdExtXTinyF];", tinyf)
        self.assertIn("let Inst{14-12} = rm;", tinyf)
        self.assertIn("// opcode-source: riscv-opcodes/rv_f/fadd_s", tinyf)
        self.assertIn("// opcode-source: riscv-opcodes/rv_f/flw", tinyf)
        self.assertNotIn("FPALU_rr_frm_m", tinyf)
        self.assertNotIn("FPCmp_rr_m", tinyf)

        for mnemonic in (
            "fadd.s",
            "fsub.s",
            "fmul.s",
            "feq.s",
            "flt.s",
            "fle.s",
            "fcvt.w.s",
            "fcvt.wu.s",
            "fcvt.s.w",
            "fcvt.s.wu",
            "fsgnj.s",
            "fmv.x.w",
            "fmv.w.x",
            "flw",
            "fsw",
        ):
            self.assertIn(mnemonic, text)

    def _instruction(
        self,
        path,
        mnemonic="vadd.vv",
        status="auto_full",
        source=None,
        opcode=None,
        patterns=(),
        builtin=None,
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
            patterns=patterns,
            builtin=builtin,
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
