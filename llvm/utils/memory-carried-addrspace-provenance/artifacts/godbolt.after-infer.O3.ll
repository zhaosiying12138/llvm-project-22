; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.before-infer.O3.ll'
source_filename = "llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu"
target datalayout = "e-p6:32:32-i64:64-i128:128-i256:256-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

@_ZZ27mcasi_digit_counters_kernelE14shared_storage = internal addrspace(3) global [256 x i32] undef, align 4

; Function Attrs: convergent mustprogress noinline norecurse nounwind
define dso_local ptx_kernel void @mcasi_digit_counters_kernel(ptr nofree noundef writeonly captures(none) %0, ptr nofree noundef readonly captures(none) %1, i32 noundef %2) local_unnamed_addr #0 {
  %4 = addrspacecast ptr %1 to ptr addrspace(1)
  %5 = addrspacecast ptr %0 to ptr addrspace(1)
  %6 = alloca [4 x ptr], align 8
  %7 = addrspacecast ptr %6 to ptr addrspace(5)
  %8 = tail call i32 asm sideeffect "mov.u32 $0, %tid.x;", "=r"() #2, !srcloc !8
  %9 = icmp ult i32 %8, 256
  %10 = zext i32 %8 to i64
  br i1 %9, label %11, label %15

11:                                               ; preds = %3
  %12 = getelementptr inbounds [4 x i8], ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage, i64 %10
  %13 = getelementptr inbounds [4 x i8], ptr addrspace(1) %4, i64 %10
  %14 = load i32, ptr addrspace(1) %13, align 4, !tbaa !9
  store i32 %14, ptr addrspace(3) %12, align 4, !tbaa !9
  br label %15

15:                                               ; preds = %11, %3
  tail call void asm sideeffect "bar.sync 0;", "~{memory}"() #2, !srcloc !10
  call void @llvm.lifetime.start.p0(ptr nonnull %6) #3
  %16 = shl i32 %8, 2
  %17 = and i32 %16, 124
  %18 = zext nneg i32 %17 to i64
  %19 = getelementptr inbounds [4 x i8], ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage, i64 %18
  %20 = addrspacecast ptr addrspace(3) %19 to ptr
  %21 = addrspacecast ptr addrspace(3) %19 to ptr
  %22 = addrspacecast ptr addrspace(3) %19 to ptr
  %23 = addrspacecast ptr addrspace(3) %19 to ptr
  store ptr %20, ptr addrspace(5) %7, align 8, !tbaa !11
  %24 = getelementptr inbounds nuw i8, ptr %21, i64 4
  %25 = getelementptr inbounds i8, ptr addrspace(5) %7, i64 8
  store ptr %24, ptr addrspace(5) %25, align 8, !tbaa !11
  %26 = getelementptr inbounds nuw i8, ptr %22, i64 8
  %27 = getelementptr inbounds i8, ptr addrspace(5) %7, i64 16
  store ptr %26, ptr addrspace(5) %27, align 8, !tbaa !11
  %28 = getelementptr inbounds nuw i8, ptr %23, i64 12
  %29 = getelementptr inbounds i8, ptr addrspace(5) %7, i64 24
  store ptr %28, ptr addrspace(5) %29, align 8, !tbaa !11
  %30 = and i32 %2, 3
  %31 = zext nneg i32 %30 to i64
  %32 = getelementptr inbounds [8 x i8], ptr addrspace(5) %7, i64 %31
  %33 = load ptr, ptr addrspace(5) %32, align 8, !tbaa !11
  %34 = load i32, ptr %33, align 4, !tbaa !9
  %35 = add i32 %34, %30
  %36 = getelementptr inbounds [4 x i8], ptr addrspace(1) %5, i64 %10
  store i32 %35, ptr addrspace(1) %36, align 4, !tbaa !9
  call void @llvm.lifetime.end.p0(ptr nonnull %6) #3
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(ptr captures(none)) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(ptr captures(none)) #1

attributes #0 = { convergent mustprogress noinline norecurse nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_80" "target-features"="+sm_80" "uniform-work-group-size" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { convergent nounwind }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}
!llvm.errno.tbaa = !{!3}

!0 = !{i32 4, !"nvvm-reflect-ftz", i32 0}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{!"clang version 23.0.0git (https://github.com/llvm/llvm-project.git 750f3bf803251abacfd440dbd883ce660597a986)"}
!3 = !{!4, !5, i64 0}
!4 = !{!"__libc_errno", !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = !{i64 360}
!9 = !{!5, !5, i64 0}
!10 = !{i64 466}
!11 = !{!12, !12, i64 0}
!12 = !{!"p1 int", !13, i64 0}
!13 = !{!"any pointer", !6, i64 0}
