from pathlib import Path
import unittest


YSX_ROOT = Path(__file__).resolve().parents[2]


class TinyFMCSourceSupportTest(unittest.TestCase):
    def test_disassembler_supports_fpr32_register_class(self):
        disassembler = (YSX_ROOT / "Disassembler" / "YSXDisassembler.cpp").read_text()

        self.assertIn("static DecodeStatus DecodeFPR32RegisterClass", disassembler)
        self.assertIn("MCRegister Reg = YSX::F0_F + RegNo;", disassembler)


if __name__ == "__main__":
    unittest.main()
