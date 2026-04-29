#!/usr/bin/env python3
import sys


def emit_add(count):
    print(
        "define void @scale_add(ptr noalias %a, ptr noalias %b, "
        "ptr noalias %out) {"
    )
    for i in range(count):
        print(f"  %ap{i} = getelementptr <128 x float>, ptr %a, i64 {i}")
        print(f"  %bp{i} = getelementptr <128 x float>, ptr %b, i64 {i}")
        print(f"  %av{i} = load <128 x float>, ptr %ap{i}, align 4")
        print(f"  %bv{i} = load <128 x float>, ptr %bp{i}, align 4")
    for i in range(count):
        print(f"  %sum{i} = fadd <128 x float> %av{i}, %bv{i}")
    for i in range(count):
        print(f"  %op{i} = getelementptr <128 x float>, ptr %out, i64 {i}")
        print(f"  store <128 x float> %sum{i}, ptr %op{i}, align 4")
    print("  ret void")
    print("}")


def emit_softmax(count):
    print("declare float @llvm.vector.reduce.fmax.v128f32(<128 x float>)")
    print("declare float @llvm.vector.reduce.fadd.v128f32(float, <128 x float>)")
    print("declare float @llvm.maxnum.f32(float, float)")
    print("declare <128 x float> @llvm.exp2.v128f32(<128 x float>)")
    print(
        "define void @scale_softmax(ptr noalias %in, ptr noalias %out, "
        "ptr noalias %sum) {"
    )
    for i in range(count):
        print(f"  %ip{i} = getelementptr <128 x float>, ptr %in, i64 {i}")
        print(f"  %v{i} = load <128 x float>, ptr %ip{i}, align 4")

    for i in range(count):
        print(
            f"  %m{i} = call fast float @llvm.vector.reduce.fmax.v128f32("
            f"<128 x float> %v{i})"
        )
        if i:
            print(
                f"  %max{i} = call fast float @llvm.maxnum.f32("
                f"float %max{i - 1}, float %m{i})"
            )
        else:
            print("  %max0 = call fast float @llvm.maxnum.f32(float %m0, float %m0)")

    print(
        f"  %max.ins = insertelement <128 x float> poison, "
        f"float %max{count - 1}, i64 0"
    )
    print(
        "  %maxv = shufflevector <128 x float> %max.ins, "
        "<128 x float> poison, <128 x i32> zeroinitializer"
    )
    print(
        "  %c.ins = insertelement <128 x float> poison, "
        "float 0x3FF7154760000000, i64 0"
    )
    print(
        "  %log2e = shufflevector <128 x float> %c.ins, "
        "<128 x float> poison, <128 x i32> zeroinitializer"
    )

    for i in range(count):
        print(f"  %centered{i} = fsub fast <128 x float> %v{i}, %maxv")
        print(f"  %scaled{i} = fmul fast <128 x float> %centered{i}, %log2e")
        print(
            f"  %exp{i} = call fast <128 x float> @llvm.exp2.v128f32("
            f"<128 x float> %scaled{i})"
        )

    for i in range(count):
        seed = "float 0.000000e+00" if i == 0 else f"float %r{i - 1}"
        print(
            f"  %r{i} = call fast float @llvm.vector.reduce.fadd.v128f32("
            f"{seed}, <128 x float> %exp{i})"
        )

    print(
        f"  %sum.ins = insertelement <128 x float> poison, "
        f"float %r{count - 1}, i64 0"
    )
    print(
        "  %sumv = shufflevector <128 x float> %sum.ins, "
        "<128 x float> poison, <128 x i32> zeroinitializer"
    )
    for i in range(count):
        print(f"  %norm{i} = fdiv fast <128 x float> %exp{i}, %sumv")
        print(f"  %op{i} = getelementptr <128 x float>, ptr %out, i64 {i}")
        print(f"  store <128 x float> %norm{i}, ptr %op{i}, align 4")
    print(f"  store float %r{count - 1}, ptr %sum, align 4")
    print("  ret void")
    print("}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: rvv-pressure-scale.py add|softmax COUNT")
    kind = sys.argv[1]
    count = int(sys.argv[2])
    if kind == "add":
        emit_add(count)
    elif kind == "softmax":
        emit_softmax(count)
    else:
        raise SystemExit(f"unknown workload: {kind}")


if __name__ == "__main__":
    main()
