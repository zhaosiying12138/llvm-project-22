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
        self.assertIn("__builtin_ysx_vfexp_v_f32m1", text)
        self.assertIn("ysx_vadd_vv_i32m1", text)
        self.assertIn("ysx_vfexp_v_f32m1", text)
        self.assertNotIn("__rvv_int32m1_t", text)
        self.assertNotIn("__riscv_vector", text)

    def test_header_is_installed_with_riscv_resource_headers(self):
        cmake = REPO_ROOT / "clang" / "lib" / "Headers" / "CMakeLists.txt"
        text = cmake.read_text()
        riscv_list = text[text.index("set(riscv_files") : text.index("set(spirv_files")]

        self.assertIn("ysx_vector.h", riscv_list)

    def test_builtin_is_declared_as_ysx_prefixed_target_builtin(self):
        builtins = REPO_ROOT / "clang" / "include" / "clang" / "Basic" / "BuiltinsYSX.td"
        text = builtins.read_text()

        self.assertIn("class YSXBuiltin", text)
        self.assertIn('__builtin_ysx_" # NAME', text)
        self.assertIn('include "clang/Basic/YSXGenAutoTinyVClangBuiltins.td"', text)
        self.assertNotIn("def vadd_vv_i32m1", text)
        self.assertNotIn("def vfexp_v_f32m1", text)

        riscv_builtins = (
            REPO_ROOT / "clang" / "include" / "clang" / "Basic" / "BuiltinsRISCV.td"
        ).read_text()
        self.assertNotIn("__builtin_ysx_", riscv_builtins)
        self.assertNotIn("YSXBuiltin", riscv_builtins)

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
        self.assertIn('include "llvm/IR/YSXGenAutoTinyVIntrinsics.td"', text)
        self.assertNotIn("def int_ysx_vadd", text)
        self.assertNotIn("def int_ysx_vfexp", text)

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
        self.assertIn("getTarget().getTriple().isYSX64()", text)
        self.assertIn('#include "clang/Basic/YSXGenAutoTinyVBuiltinCG.inc"', text)
        self.assertNotIn("case YSX::BI__builtin_ysx_vadd_vv_i32m1:", text)
        self.assertNotIn("case YSX::BI__builtin_ysx_vfexp_v_f32m1:", text)

    def test_global_tablegen_consumes_auto_td_outputs(self):
        clang_basic = (
            REPO_ROOT / "clang" / "include" / "clang" / "Basic" / "CMakeLists.txt"
        ).read_text()
        clang_codegen = (REPO_ROOT / "clang" / "lib" / "CodeGen" / "CMakeLists.txt").read_text()
        llvm_ir = (REPO_ROOT / "llvm" / "include" / "llvm" / "IR" / "CMakeLists.txt").read_text()

        self.assertIn("YSXGenAutoTinyVClangBuiltins.td", clang_basic)
        self.assertIn("YSXGenAutoTinyVBuiltinCG.inc", clang_basic)
        self.assertIn("ClangYSXAutoTDGen", clang_basic)
        self.assertIn("arg_lut.csv", clang_basic)
        self.assertIn("ClangYSXAutoTDGen", clang_codegen)
        self.assertIn("YSXGenAutoTinyVIntrinsics.td", llvm_ir)
        self.assertIn("LLVMYSXAutoTDIntrinsics", llvm_ir)
        self.assertIn("arg_lut.csv", llvm_ir)

        backend = (REPO_ROOT / "llvm" / "lib" / "Target" / "YuShuXin" / "CMakeLists.txt").read_text()
        self.assertIn("arg_lut.csv", backend)

    def test_ysx_has_separate_builtin_shard_without_rvv_shards(self):
        target_builtins = REPO_ROOT / "clang" / "include" / "clang" / "Basic" / "TargetBuiltins.h"
        target_builtins_text = target_builtins.read_text()
        self.assertIn("namespace YSX", target_builtins_text)
        self.assertIn('#include "clang/Basic/BuiltinsYSX.inc"', target_builtins_text)

        targets = REPO_ROOT / "clang" / "lib" / "Basic" / "Targets" / "RISCV.cpp"
        targets_text = targets.read_text()
        ysx_get_builtins = targets_text[
            targets_text.index("YSX64TargetInfo::getTargetBuiltins()")
            : targets_text.index("void YSX64TargetInfo::getTargetDefines")
        ]
        self.assertIn("YSXBuiltins::BuiltinStrings", ysx_get_builtins)
        self.assertIn("YSXBuiltins::BuiltinInfos", ysx_get_builtins)
        self.assertNotIn("RVV::BuiltinInfos", ysx_get_builtins)

    def test_ysx_triple_uses_riscv_builtin_codegen_dispatch(self):
        dispatch = REPO_ROOT / "clang" / "lib" / "CodeGen" / "CGBuiltin.cpp"
        text = dispatch.read_text()

        riscv_dispatch = text[
            text.index("case llvm::Triple::riscv32:")
            : text.index("case llvm::Triple::spirv32:")
        ]
        self.assertIn("case llvm::Triple::ysx64:", riscv_dispatch)
        self.assertIn("EmitRISCVBuiltinExpr", riscv_dispatch)

    def test_lit_test_covers_header_builtin_and_ir_intrinsic(self):
        lit = REPO_ROOT / "clang" / "test" / "CodeGen" / "YSX" / "tinyv-builtins.c"
        self.assertTrue(lit.exists(), "missing Clang YSX tiny-v builtin lit test")
        text = lit.read_text()

        self.assertIn("#include <ysx_vector.h>", text)
        self.assertIn("ysx_vadd_vv_i32m1", text)
        self.assertIn("@llvm.ysx.vadd", text)
        self.assertIn("YSX-NOT: __riscv_vector", text)
        self.assertIn("YSX-NOT: __riscv_v_intrinsic", text)

        custom = REPO_ROOT / "clang" / "test" / "CodeGen" / "YSX" / "yushuxin-vfexp.c"
        self.assertTrue(custom.exists(), "missing custom YSX vfexp lit test")
        custom_text = custom.read_text()
        self.assertIn("ysx_vfexp_v_f32m1", custom_text)
        self.assertIn("@llvm.ysx.vfexp", custom_text)


if __name__ == "__main__":
    unittest.main()
