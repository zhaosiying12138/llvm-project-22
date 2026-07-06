; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.before-infer.O0.ll'
source_filename = "llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu"
target datalayout = "e-p6:32:32-i64:64-i128:128-i256:256-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

@_ZZ27mcasi_digit_counters_kernelE14shared_storage = internal addrspace(3) global [256 x i32] undef, align 4

; Function Attrs: convergent mustprogress noinline norecurse nounwind optnone
define dso_local ptx_kernel void @mcasi_digit_counters_kernel(ptr noundef %0, ptr noundef %1, i32 noundef %2) #0 {
  %4 = alloca ptr, align 8
  %5 = alloca ptr, align 8
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca [4 x ptr], align 8
  %9 = alloca i32, align 4
  %10 = alloca i32, align 4
  %11 = alloca ptr, align 8
  %12 = alloca i32, align 4
  store ptr %0, ptr %4, align 8
  store ptr %1, ptr %5, align 8
  store i32 %2, ptr %6, align 4
  %13 = call i32 asm sideeffect "mov.u32 $0, %tid.x;", "=r"() #1, !srcloc !3
  store i32 %13, ptr %7, align 4
  %14 = load i32, ptr %7, align 4
  %15 = icmp ult i32 %14, 256
  br i1 %15, label %16, label %25

16:                                               ; preds = %3
  %17 = load ptr, ptr %5, align 8
  %18 = load i32, ptr %7, align 4
  %19 = zext i32 %18 to i64
  %20 = getelementptr inbounds nuw i32, ptr %17, i64 %19
  %21 = load i32, ptr %20, align 4
  %22 = load i32, ptr %7, align 4
  %23 = zext i32 %22 to i64
  %24 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %23
  store i32 %21, ptr %24, align 4
  br label %25

25:                                               ; preds = %16, %3
  call void asm sideeffect "bar.sync 0;", "~{memory}"() #1, !srcloc !4
  %26 = load i32, ptr %7, align 4
  %27 = and i32 %26, 31
  %28 = mul i32 %27, 4
  store i32 %28, ptr %9, align 4
  %29 = load i32, ptr %9, align 4
  %30 = add i32 %29, 0
  %31 = and i32 %30, 255
  %32 = zext i32 %31 to i64
  %33 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %32
  %34 = getelementptr inbounds [4 x ptr], ptr %8, i64 0, i64 0
  store ptr %33, ptr %34, align 8
  %35 = load i32, ptr %9, align 4
  %36 = add i32 %35, 1
  %37 = and i32 %36, 255
  %38 = zext i32 %37 to i64
  %39 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %38
  %40 = getelementptr inbounds [4 x ptr], ptr %8, i64 0, i64 1
  store ptr %39, ptr %40, align 8
  %41 = load i32, ptr %9, align 4
  %42 = add i32 %41, 2
  %43 = and i32 %42, 255
  %44 = zext i32 %43 to i64
  %45 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %44
  %46 = getelementptr inbounds [4 x ptr], ptr %8, i64 0, i64 2
  store ptr %45, ptr %46, align 8
  %47 = load i32, ptr %9, align 4
  %48 = add i32 %47, 3
  %49 = and i32 %48, 255
  %50 = zext i32 %49 to i64
  %51 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %50
  %52 = getelementptr inbounds [4 x ptr], ptr %8, i64 0, i64 3
  store ptr %51, ptr %52, align 8
  %53 = load i32, ptr %6, align 4
  %54 = and i32 %53, 3
  store i32 %54, ptr %10, align 4
  %55 = load i32, ptr %10, align 4
  %56 = zext i32 %55 to i64
  %57 = getelementptr inbounds nuw [4 x ptr], ptr %8, i64 0, i64 %56
  %58 = load ptr, ptr %57, align 8
  store ptr %58, ptr %11, align 8
  %59 = load ptr, ptr %11, align 8
  %60 = load i32, ptr %59, align 4
  store i32 %60, ptr %12, align 4
  %61 = load i32, ptr %12, align 4
  %62 = load i32, ptr %10, align 4
  %63 = add i32 %61, %62
  %64 = load ptr, ptr %4, align 8
  %65 = load i32, ptr %7, align 4
  %66 = zext i32 %65 to i64
  %67 = getelementptr inbounds nuw i32, ptr %64, i64 %66
  store i32 %63, ptr %67, align 4
  ret void
}

attributes #0 = { convergent mustprogress noinline norecurse nounwind optnone "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_80" "target-features"="+sm_80" "uniform-work-group-size" }
attributes #1 = { convergent nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 4, !"nvvm-reflect-ftz", i32 0}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{!"clang version 23.0.0git (https://github.com/llvm/llvm-project.git 750f3bf803251abacfd440dbd883ce660597a986)"}
!3 = !{i64 360}
!4 = !{i64 466}
