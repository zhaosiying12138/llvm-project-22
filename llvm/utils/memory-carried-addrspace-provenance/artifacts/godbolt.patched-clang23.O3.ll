; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu'
source_filename = "llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu"
target datalayout = "e-p6:32:32-i64:64-i128:128-i256:256-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

@_ZZ27mcasi_digit_counters_kernelE14shared_storage = internal addrspace(3) global [256 x i32] undef, align 4

; Function Attrs: convergent mustprogress noinline norecurse nounwind
define dso_local ptx_kernel void @mcasi_digit_counters_kernel(ptr nofree noundef writeonly captures(none) %0, ptr nofree noundef readonly captures(none) %1, i32 noundef %2) local_unnamed_addr #0 {
  %4 = alloca [4 x ptr], align 8
  %5 = tail call i32 asm sideeffect "mov.u32 $0, %tid.x;", "=r"() #2, !srcloc !8
  %6 = icmp ult i32 %5, 256
  %7 = zext i32 %5 to i64
  br i1 %6, label %8, label %12

8:                                                ; preds = %3
  %9 = getelementptr inbounds nuw [4 x i8], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 %7
  %10 = getelementptr inbounds nuw [4 x i8], ptr %1, i64 %7
  %11 = load i32, ptr %10, align 4, !tbaa !9
  store i32 %11, ptr %9, align 4, !tbaa !9
  br label %12

12:                                               ; preds = %3, %8
  tail call void asm sideeffect "bar.sync 0;", "~{memory}"() #2, !srcloc !10
  call void @llvm.lifetime.start.p0(ptr nonnull %4) #3
  %13 = shl i32 %5, 2
  %14 = and i32 %13, 124
  %15 = zext nneg i32 %14 to i64
  %16 = getelementptr inbounds nuw [4 x i8], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 %15
  store ptr %16, ptr %4, align 8, !tbaa !11
  %17 = getelementptr inbounds nuw i8, ptr %16, i64 4
  %18 = getelementptr inbounds nuw i8, ptr %4, i64 8
  store ptr %17, ptr %18, align 8, !tbaa !11
  %19 = getelementptr inbounds nuw i8, ptr %16, i64 8
  %20 = getelementptr inbounds nuw i8, ptr %4, i64 16
  store ptr %19, ptr %20, align 8, !tbaa !11
  %21 = getelementptr inbounds nuw i8, ptr %16, i64 12
  %22 = getelementptr inbounds nuw i8, ptr %4, i64 24
  store ptr %21, ptr %22, align 8, !tbaa !11
  %23 = and i32 %2, 3
  %24 = zext nneg i32 %23 to i64
  %25 = getelementptr inbounds nuw [8 x i8], ptr %4, i64 %24
  %26 = load ptr, ptr %25, align 8, !tbaa !11
  %27 = load i32, ptr %26, align 4, !tbaa !9
  %28 = add i32 %27, %23
  %29 = getelementptr inbounds nuw [4 x i8], ptr %0, i64 %7
  store i32 %28, ptr %29, align 4, !tbaa !9
  call void @llvm.lifetime.end.p0(ptr nonnull %4) #3
  ret void
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(ptr captures(none)) #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(ptr captures(none)) #1

attributes #0 = { convergent mustprogress noinline norecurse nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_80" "target-features"="+sm_80" "uniform-work-group-size" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
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
