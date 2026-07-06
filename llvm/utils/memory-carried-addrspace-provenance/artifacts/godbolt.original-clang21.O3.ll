; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu'
source_filename = "llvm/utils/memory-carried-addrspace-provenance/godbolt_reproducer.cu"
target datalayout = "e-p6:32:32-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

@_ZZ27mcasi_digit_counters_kernelE14shared_storage = internal addrspace(3) global [256 x i32] undef, align 4

; Function Attrs: convergent mustprogress noinline norecurse nounwind
define dso_local ptx_kernel void @mcasi_digit_counters_kernel(ptr noundef writeonly captures(none) %0, ptr noundef readonly captures(none) %1, i32 noundef %2) local_unnamed_addr #0 {
  %4 = alloca [4 x ptr], align 8
  %5 = tail call i32 asm sideeffect "mov.u32 $0, %tid.x;", "=r"() #2, !srcloc !5
  %6 = icmp ult i32 %5, 256
  %7 = zext i32 %5 to i64
  br i1 %6, label %8, label %12

8:                                                ; preds = %3
  %9 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %7
  %10 = getelementptr inbounds nuw i32, ptr %1, i64 %7
  %11 = load i32, ptr %10, align 4, !tbaa !6
  store i32 %11, ptr %9, align 4, !tbaa !6
  br label %12

12:                                               ; preds = %3, %8
  tail call void asm sideeffect "bar.sync 0;", "~{memory}"() #2, !srcloc !10
  call void @llvm.lifetime.start.p0(i64 32, ptr nonnull %4) #3
  %13 = shl i32 %5, 2
  %14 = and i32 %13, 124
  %15 = zext nneg i32 %14 to i64
  %16 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %15
  store ptr %16, ptr %4, align 8, !tbaa !11
  %17 = or disjoint i32 %14, 1
  %18 = zext nneg i32 %17 to i64
  %19 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %18
  %20 = getelementptr inbounds nuw i8, ptr %4, i64 8
  store ptr %19, ptr %20, align 8, !tbaa !11
  %21 = or disjoint i32 %14, 2
  %22 = zext nneg i32 %21 to i64
  %23 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %22
  %24 = getelementptr inbounds nuw i8, ptr %4, i64 16
  store ptr %23, ptr %24, align 8, !tbaa !11
  %25 = or disjoint i32 %14, 3
  %26 = zext nneg i32 %25 to i64
  %27 = getelementptr inbounds nuw [256 x i32], ptr addrspacecast (ptr addrspace(3) @_ZZ27mcasi_digit_counters_kernelE14shared_storage to ptr), i64 0, i64 %26
  %28 = getelementptr inbounds nuw i8, ptr %4, i64 24
  store ptr %27, ptr %28, align 8, !tbaa !11
  %29 = and i32 %2, 3
  %30 = zext nneg i32 %29 to i64
  %31 = getelementptr inbounds nuw [4 x ptr], ptr %4, i64 0, i64 %30
  %32 = load ptr, ptr %31, align 8, !tbaa !11
  %33 = load i32, ptr %32, align 4, !tbaa !6
  %34 = add i32 %33, %29
  %35 = getelementptr inbounds nuw i32, ptr %0, i64 %7
  store i32 %34, ptr %35, align 4, !tbaa !6
  call void @llvm.lifetime.end.p0(i64 32, ptr nonnull %4) #3
  ret void
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr captures(none)) #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr captures(none)) #1

attributes #0 = { convergent mustprogress noinline norecurse nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_80" "target-features"="+ptx42,+sm_80" "uniform-work-group-size"="true" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { convergent nounwind }
attributes #3 = { nounwind }

!nvvm.annotations = !{!0}
!llvm.module.flags = !{!1, !2, !3}
!llvm.ident = !{!4}

!0 = !{ptr @mcasi_digit_counters_kernel}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 4, !"nvvm-reflect-ftz", i32 0}
!3 = !{i32 7, !"frame-pointer", i32 2}
!4 = !{!"Ubuntu clang version 21.1.8 (6ubuntu1)"}
!5 = !{i64 360}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = !{i64 466}
!11 = !{!12, !12, i64 0}
!12 = !{!"p1 int", !13, i64 0}
!13 = !{!"any pointer", !8, i64 0}
