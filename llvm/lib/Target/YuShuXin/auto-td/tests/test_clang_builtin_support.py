from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[6]


class ClangTinyVBuiltinSupportTest(unittest.TestCase):
    def test_resource_header_exposes_ysx_vector_api_without_rvv_types(self):
        header = REPO_ROOT / "clang" / "lib" / "Headers" / "ysx_vector.h"
        self.assertTrue(header.exists(), "missing clang resource header ysx_vector.h")
        text = header.read_text()

        self.assertIn("__YSX_TINY_VECTOR__", text)
        self.assertIn("__attribute__((ext_vector_type(4)))", text)
        self.assertIn("typedef int ysx_vint32m1_t", text)
        self.assertIn("__builtin_ysx_vadd_vv_i32m1", text)
        self.assertIn("ysx_vadd_vv_i32m1", text)
        self.assertNotIn("__rvv_int32m1_t", text)
        self.assertNotIn("__riscv_vector", text)

    def test_header_is_installed_with_riscv_resource_headers(self):
        cmake = REPO_ROOT / "clang" / "lib" / "Headers" / "CMakeLists.txt"
        text = cmake.read_text()
        riscv_list = text[text.index("set(riscv_files") : text.index("set(spirv_files")]

        self.assertIn("ysx_vector.h", riscv_list)

    def test_builtin_is_declared_as_ysx_prefixed_target_builtin(self):
        builtins = REPO_ROOT / "clang" / "include" / "clang" / "Basic" / "BuiltinsRISCV.td"
        text = builtins.read_text()

        self.assertIn("class YSXBuiltin", text)
        self.assertIn('__builtin_ysx_" # NAME', text)
        self.assertIn("def vadd_vv_i32m1", text)
        self.assertIn(
            '"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long)"',
            text,
        )
        self.assertIn('"xtinyv,zvl128b"', text)

    def test_ysx_target_defines_private_vector_header_guard_macro(self):
        target = REPO_ROOT / "clang" / "lib" / "Basic" / "Targets" / "RISCV.cpp"
        text = target.read_text()

        self.assertIn("__YSX_TINY_VECTOR__", text)
        self.assertIn("HasXTinyV && HasZvl128b", text)
        self.assertIn("__riscv_xtinyv", text)

    def test_llvm_has_ysx_vadd_intrinsic_tablegen_shard(self):
        intrinsics = REPO_ROOT / "llvm" / "include" / "llvm" / "IR" / "IntrinsicsYSX.td"
        self.assertTrue(intrinsics.exists(), "missing YSX intrinsic td shard")
        text = intrinsics.read_text()

        self.assertIn('TargetPrefix = "ysx"', text)
        self.assertIn("def int_ysx_vadd", text)
        self.assertIn("llvm_anyvector_ty", text)
        self.assertIn("IntrNoMem", text)

        cmake = (REPO_ROOT / "llvm" / "include" / "llvm" / "IR" / "CMakeLists.txt").read_text()
        gn = (
            REPO_ROOT
            / "llvm"
            / "utils"
            / "gn"
            / "secondary"
            / "llvm"
            / "include"
            / "llvm"
            / "IR"
            / "BUILD.gn"
        ).read_text()
        all_intrinsics = (REPO_ROOT / "llvm" / "include" / "llvm" / "IR" / "Intrinsics.td").read_text()
        self.assertIn("IntrinsicsYSX.h", cmake)
        self.assertIn("-intrinsic-prefix=ysx", cmake)
        self.assertIn('gen_arch_intrinsics("IntrinsicsYSX")', gn)
        self.assertIn('":IntrinsicsYSX"', gn)
        self.assertIn('include "llvm/IR/IntrinsicsYSX.td"', all_intrinsics)

    def test_codegen_lowers_builtin_to_ysx_vadd_intrinsic(self):
        codegen = REPO_ROOT / "clang" / "lib" / "CodeGen" / "TargetBuiltins" / "RISCV.cpp"
        text = codegen.read_text()

        self.assertIn('#include "llvm/IR/IntrinsicsYSX.h"', text)
        self.assertIn("case RISCV::BI__builtin_ysx_vadd_vv_i32m1:", text)
        self.assertIn("Intrinsic::ysx_vadd", text)
        self.assertIn("CGM.getIntrinsic(Intrinsic::ysx_vadd", text)

    def test_lit_test_covers_header_builtin_and_ir_intrinsic(self):
        lit = REPO_ROOT / "clang" / "test" / "CodeGen" / "YSX" / "tinyv-builtins.c"
        self.assertTrue(lit.exists(), "missing Clang YSX tiny-v builtin lit test")
        text = lit.read_text()

        self.assertIn("#include <ysx_vector.h>", text)
        self.assertIn("ysx_vadd_vv_i32m1", text)
        self.assertIn("@llvm.ysx.vadd", text)
        self.assertIn("YSX-NOT: __riscv_vector", text)
        self.assertIn("YSX-NOT: __riscv_v_intrinsic", text)


if __name__ == "__main__":
    unittest.main()
