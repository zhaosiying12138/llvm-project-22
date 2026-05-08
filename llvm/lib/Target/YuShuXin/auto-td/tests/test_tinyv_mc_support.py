from pathlib import Path
import unittest


YSX_ROOT = Path(__file__).resolve().parents[2]


class TinyVMCSourceSupportTest(unittest.TestCase):
    def test_asm_parser_supports_optional_v0_t_mask_operand(self):
        text = (YSX_ROOT / "AsmParser" / "YSXAsmParser.cpp").read_text()

        self.assertIn("ParseStatus parseVMaskReg(OperandVector &Operands);", text)
        self.assertIn("std::unique_ptr<YSXOperand> defaultMaskRegOp() const;", text)
        self.assertIn("bool isV0Reg() const", text)
        self.assertIn("return Kind == KindTy::Register && Reg.Reg == YSX::V0;", text)
        self.assertIn("ParseStatus YSXAsmParser::parseVMaskReg", text)
        self.assertIn('Name.consume_back(".t")', text)
        self.assertIn("Reg != YSX::V0", text)
        self.assertIn("YSXOperand::createReg(MCRegister()", text)

    def test_mc_printer_emitter_and_disassembler_support_vmask_and_vr(self):
        printer_h = (YSX_ROOT / "MCTargetDesc" / "YSXInstPrinter.h").read_text()
        printer_cpp = (YSX_ROOT / "MCTargetDesc" / "YSXInstPrinter.cpp").read_text()
        emitter = (YSX_ROOT / "MCTargetDesc" / "YSXMCCodeEmitter.cpp").read_text()
        disassembler = (YSX_ROOT / "Disassembler" / "YSXDisassembler.cpp").read_text()

        self.assertIn("void printVMaskReg", printer_h)
        self.assertIn("void YSXInstPrinter::printVMaskReg", printer_cpp)
        self.assertIn('O << ", ";', printer_cpp)
        self.assertIn('O << ".t";', printer_cpp)
        self.assertIn("unsigned getVMaskReg", emitter)
        self.assertIn("case YSX::V0:", emitter)
        self.assertIn("case YSX::NoRegister:", emitter)
        self.assertIn("static DecodeStatus DecodeVRRegisterClass", disassembler)
        self.assertIn("MCRegister Reg = YSX::V0 + RegNo;", disassembler)
        self.assertIn("static DecodeStatus decodeVMaskReg", disassembler)
        self.assertIn("RegNo == 0", disassembler)

    def test_retained_insn_directive_opcode_filter_knows_tiny_fv_opcodes(self):
        text = (YSX_ROOT / "AsmParser" / "YSXAsmParser.cpp").read_text()

        self.assertIn("case 0b0000111: // LOAD_FP", text)
        self.assertIn("case 0b0001011: // CUSTOM_0", text)
        self.assertIn("case 0b0100111: // STORE_FP", text)
        self.assertIn("case 0b1010011: // OP_FP", text)
        self.assertIn("case 0b1010111: // OP_V", text)

    def test_vmask_has_target_operand_type(self):
        text = (YSX_ROOT / "MCTargetDesc" / "YSXBaseInfo.h").read_text()

        self.assertIn("OPERAND_VMASK", text)


if __name__ == "__main__":
    unittest.main()
