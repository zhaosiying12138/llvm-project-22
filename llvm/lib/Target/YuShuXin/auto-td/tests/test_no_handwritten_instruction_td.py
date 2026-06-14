"""Source-side guard for the auto-td promise.

The headline auto-td contract is: every YSX tiny-F/tiny-V instruction is added
from an opcode source plus one structured YAML file, and its real instruction
TableGen record is *generated* -- never hand-authored as a nested instruction
class in the committed backend TableGen.

Until now that contract was enforced only by convention and code review:
``validate.py`` inspects the instruction YAML but never opens any ``.td``, so a
contributor could hand-write ``def FOO : RVInstR<..., "fadd.s", ...>`` under a
tiny-F/tiny-V predicate directly in the backend and nothing would fail. This
test converts the contract into a mechanically enforced invariant.

Checks:

* No instruction-record ``def`` in the committed YSX backend TableGen targets a
  tiny-F/tiny-V predicate or mnemonic (such records must come from the generated
  ``YSXGenAuto*`` includes, which live in the build tree, not in source).
* The generated-include region in ``YSXInstrInfo.td`` is fenced by sentinels and
  contains only ``include`` lines -- no hand-written instruction ``def``.
* The set of generated ``YSX_AUTO_*`` records is exactly the set of mnemonics
  declared by the instruction YAML files (catches both a hand-written bypass and
  a silently dropped instruction).
"""

import re
import sys
import tempfile
from pathlib import Path
import unittest

import yaml

TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from ysx_auto_td.emit_td import write_td_outputs
from ysx_auto_td.loader import load_instruction_set
from ysx_auto_td.opcodes import source_key_from_mnemonic

YSX_ROOT = Path(__file__).resolve().parents[2]            # .../Target/YuShuXin
REPO_ROOT = Path(__file__).resolve().parents[6]           # worktree root
INSTRUCTIONS_ROOT = YSX_ROOT / "auto-td" / "instructions"
INSTR_INFO_TD = YSX_ROOT / "YSXInstrInfo.td"

TINY_FV_PREDICATES = ("HasStdExtXTinyF", "HasStdExtXTinyV")
AUTO_TD_BEGIN = "// YSX-AUTO-TD-BEGIN"
AUTO_TD_END = "// YSX-AUTO-TD-END"

# An instruction record derives from the RVInst family. Pseudo/operand/register
# classes and the `class RVInst*` definitions themselves are intentionally not
# matched (we only scan `def`s).
_INSTR_DEF_RE = re.compile(
    r"^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*RVInst[A-Za-z0-9_]*\b"
)


def _declared_mnemonics() -> dict[str, Path]:
    declared: dict[str, Path] = {}
    for path in sorted(INSTRUCTIONS_ROOT.glob("*/*.yaml")):
        data = yaml.safe_load(path.read_text())
        declared[data["mnemonic"]] = path
    return declared


def _instruction_records(td_path: Path):
    """Yield (name, line_number, body_text) for each RVInst-family ``def``.

    Handles both single-statement records (terminated by ``;``) and block
    records with a ``{ ... }`` body.
    """
    lines = td_path.read_text().splitlines()
    index = 0
    total = len(lines)
    while index < total:
        match = _INSTR_DEF_RE.match(lines[index])
        if not match:
            index += 1
            continue
        name = match.group(1)
        start = index
        depth = 0
        seen_brace = False
        body: list[str] = []
        while index < total:
            line = lines[index]
            body.append(line)
            depth += line.count("{") - line.count("}")
            if "{" in line:
                seen_brace = True
            stripped = line.rstrip()
            index += 1
            if not seen_brace and depth <= 0 and stripped.endswith(";"):
                break
            if seen_brace and depth <= 0 and stripped.endswith("}"):
                break
        yield name, start + 1, "\n".join(body)


class NoHandwrittenInstructionTdTest(unittest.TestCase):
    def test_no_handwritten_tiny_fv_instruction_record(self):
        tiny_fv_mnemonics = set(_declared_mnemonics())
        offenders: list[str] = []
        for td_path in sorted(YSX_ROOT.glob("*.td")):
            for name, lineno, body in _instruction_records(td_path):
                has_predicate = any(pred in body for pred in TINY_FV_PREDICATES)
                mnemonic_hit = next(
                    (mn for mn in tiny_fv_mnemonics if f'"{mn}"' in body), None
                )
                if has_predicate or mnemonic_hit:
                    reason = "predicate" if has_predicate else f"mnemonic {mnemonic_hit}"
                    offenders.append(f"{td_path}:{lineno}: def {name} ({reason})")
        self.assertEqual(
            offenders,
            [],
            "hand-written tiny-F/tiny-V instruction TableGen records are forbidden; "
            "add the instruction via auto-td (opcode source + YAML) so the record is "
            "generated:\n" + "\n".join(offenders),
        )

    def test_auto_td_include_region_is_sentineled_and_def_free(self):
        text = INSTR_INFO_TD.read_text()
        self.assertIn(AUTO_TD_BEGIN, text, "missing YSX-AUTO-TD-BEGIN sentinel")
        self.assertIn(AUTO_TD_END, text, "missing YSX-AUTO-TD-END sentinel")
        begin = text.index(AUTO_TD_BEGIN)
        end = text.index(AUTO_TD_END)
        self.assertLess(begin, end, "sentinels out of order")
        region = text[begin:end]
        self.assertNotRegex(
            region,
            r"(?m)^\s*def\s",
            "no hand-written def may appear inside the auto-td generated-include region",
        )
        for fragment in (
            "YSXGenAutoTinyFInstrInfo.inc",
            "YSXGenAutoTinyVInstrInfo.inc",
            "YSXGenAutoTinyVPseudos.inc",
            "YSXGenAutoTinyVPatterns.inc",
        ):
            self.assertIn(fragment, region, f"region must include {fragment}")

    def test_generated_records_biject_with_declared_instructions(self):
        declared = _declared_mnemonics()
        instructions = load_instruction_set(
            YSX_ROOT,
            REPO_ROOT / "third_party" / "riscv-opcodes",
            REPO_ROOT / "third_party" / "ysx-opcodes",
        )
        self.assertEqual(
            {instruction.mnemonic for instruction in instructions},
            set(declared),
            "loaded instruction set must match the declared YAML mnemonics",
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            out = Path(tmpdir)
            write_td_outputs(out, instructions)
            generated_text = (
                (out / "YSXGenAutoTinyFInstrInfo.inc").read_text()
                + (out / "YSXGenAutoTinyVInstrInfo.inc").read_text()
            )

        generated = set(re.findall(r"def (YSX_AUTO_[A-Z0-9_]+)\b", generated_text))
        expected = {
            "YSX_AUTO_" + source_key_from_mnemonic(mnemonic).upper()
            for mnemonic in declared
        }
        self.assertEqual(
            generated,
            expected,
            "every declared instruction must produce exactly one YSX_AUTO_ record "
            "and no generated record may lack a declaring YAML",
        )


if __name__ == "__main__":
    unittest.main()
