# Key IR/PTX diffs for memory-carried address-space provenance

## Reduced IR before/after infer-address-spaces

Files:
- before: `artifacts/reduced-godbolt-pattern.ll`
- after: `artifacts/reduced-godbolt-pattern.after-infer.ll`

```diff
--- reduced-godbolt-pattern.ll

+++ reduced-godbolt-pattern.after-infer.ll

@@ -1,11 +1,6 @@

-; RUN: opt -S -passes='verify,infer-address-spaces,verify' %s | FileCheck %s
-; RUN: opt -S -passes=infer-address-spaces %s | llc -march=nvptx64 -mcpu=sm_80 -o - | FileCheck %s --check-prefix=PTX
-
-; Check that InferAddressSpaces can recover the concrete address space of a
-; pointer value carried through private/local memory without rewriting the
-; pointer slot itself.
-
+; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.ll'
+source_filename = "llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.ll"
 target datalayout = "e-p:64:64-p1:64:64-p3:64:64-p5:64:64-i64:64-i128:128-n16:32:64"
 target triple = "nvptx64-nvidia-cuda"
 
 @shared_storage = internal addrspace(3) global [256 x i32] undef, align 4
@@ -13,250 +8,168 @@

 
 declare void @unknown_clobber(ptr addrspace(5))
 
 define i32 @positive_single_slot(i32 %idx) {
-; CHECK-LABEL: define i32 @positive_single_slot(
-; CHECK:       [[SLOT:%.*]] = addrspacecast ptr {{%.*}} to ptr addrspace(5)
-; CHECK:       store ptr {{%.*}}, ptr addrspace(5) [[SLOT]], align 8
-; CHECK:       [[RELOAD:%.*]] = load ptr, ptr addrspace(5) [[SLOT]], align 8
-; CHECK-NEXT:  [[AS3:%.*]] = addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(1)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr addrspace(3) [[AS3]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
-;
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   %sh.gep = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 %idx
   %sh.generic = addrspacecast ptr addrspace(3) %sh.gep to ptr
-  store ptr %sh.generic, ptr addrspace(5) %slot, align 8
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
-  %value = load i32, ptr %p.reload, align 4
+  store ptr %sh.generic, ptr addrspace(5) %0, align 8
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
+  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
+  %value = load i32, ptr addrspace(3) %1, align 4
   ret i32 %value
 }
 
 define i32 @positive_dynamic_array(i32 %selector) {
-; CHECK-LABEL: define i32 @positive_dynamic_array(
-; CHECK:       [[SLOTS:%.*]] = addrspacecast ptr {{%.*}} to ptr addrspace(5)
-; CHECK:       [[DYNAMIC:%.*]] = getelementptr inbounds [4 x ptr], ptr addrspace(5) [[SLOTS]], i32 0, i32 {{%.*}}
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) [[DYNAMIC]], align 8
-; CHECK-NEXT:  [[AS3:%.*]] = addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(1)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr addrspace(3) [[AS3]], align 4
-; CHECK-NEXT:  [[GLOBAL_GEP:%.*]] = getelementptr inbounds [256 x i32], ptr addrspace(1) @global_storage, i32 0, i32 {{%.*}}
-; CHECK-NEXT:  [[GLOBAL:%.*]] = load i32, ptr addrspace(1) [[GLOBAL_GEP]], align 4
-; CHECK-NEXT:  [[SUM:%.*]] = add i32 [[VALUE]], [[GLOBAL]]
-; CHECK-NEXT:  ret i32 [[SUM]]
-;
-; PTX-LABEL: .visible .func {{.*}}positive_dynamic_array
-; PTX:       ld.local.b64
-; PTX-NEXT:  cvta.to.shared.u64
-; PTX-NEXT:  ld.shared.b32
-; PTX:       ld.global.b32
 entry:
   %slots.generic = alloca [4 x ptr], align 8
-  %slots = addrspacecast ptr %slots.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slots.generic to ptr addrspace(5)
   %sh0 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %gen0 = addrspacecast ptr addrspace(3) %sh0 to ptr
-  %slot0 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %slots, i32 0, i32 0
+  %slot0 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 0
   store ptr %gen0, ptr addrspace(5) %slot0, align 8
   %sh1 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 1
   %gen1 = addrspacecast ptr addrspace(3) %sh1 to ptr
-  %slot1 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %slots, i32 0, i32 1
+  %slot1 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 1
   store ptr %gen1, ptr addrspace(5) %slot1, align 8
   %sh2 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 2
   %gen2 = addrspacecast ptr addrspace(3) %sh2 to ptr
-  %slot2 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %slots, i32 0, i32 2
+  %slot2 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 2
   store ptr %gen2, ptr addrspace(5) %slot2, align 8
   %sh3 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 3
   %gen3 = addrspacecast ptr addrspace(3) %sh3 to ptr
-  %slot3 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %slots, i32 0, i32 3
+  %slot3 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 3
   store ptr %gen3, ptr addrspace(5) %slot3, align 8
   %which = and i32 %selector, 3
-  %slot.dynamic = getelementptr inbounds [4 x ptr], ptr addrspace(5) %slots, i32 0, i32 %which
+  %slot.dynamic = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 %which
   %p.reload = load ptr, ptr addrspace(5) %slot.dynamic, align 8
-  %value = load i32, ptr %p.reload, align 4
+  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
+  %value = load i32, ptr addrspace(3) %1, align 4
   %global.gep = getelementptr inbounds [256 x i32], ptr addrspace(1) @global_storage, i32 0, i32 %which
   %global = load i32, ptr addrspace(1) %global.gep, align 4
   %sum = add i32 %value, %global
   ret i32 %sum
 }
 
 define i32 @positive_cfg_merge(i1 %cond) {
-; CHECK-LABEL: define i32 @positive_cfg_merge(
-; CHECK:       merge:
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NEXT:  [[AS3:%.*]] = addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(1)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr addrspace(3) [[AS3]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   br i1 %cond, label %left, label %right
 
-left:
+left:                                             ; preds = %entry
   %sh0 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 10
   %gen0 = addrspacecast ptr addrspace(3) %sh0 to ptr
-  store ptr %gen0, ptr addrspace(5) %slot, align 8
+  store ptr %gen0, ptr addrspace(5) %0, align 8
   br label %merge
 
-right:
+right:                                            ; preds = %entry
   %sh1 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 20
   %gen1 = addrspacecast ptr addrspace(3) %sh1 to ptr
-  store ptr %gen1, ptr addrspace(5) %slot, align 8
+  store ptr %gen1, ptr addrspace(5) %0, align 8
   br label %merge
 
-merge:
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
-  %value = load i32, ptr %p.reload, align 4
+merge:                                            ; preds = %right, %left
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
+  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
+  %value = load i32, ptr addrspace(3) %1, align 4
   ret i32 %value
 }
 
 define i32 @negative_conflicting_shared_global(i1 %cond) {
-; CHECK-LABEL: define i32 @negative_conflicting_shared_global(
-; CHECK:       merge:
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(1)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   br i1 %cond, label %shared, label %global
 
-shared:
+shared:                                           ; preds = %entry
   %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
-  store ptr %sh.gen, ptr addrspace(5) %slot, align 8
+  store ptr %sh.gen, ptr addrspace(5) %0, align 8
   br label %merge
 
-global:
+global:                                           ; preds = %entry
   %gl = getelementptr inbounds [256 x i32], ptr addrspace(1) @global_storage, i32 0, i32 0
   %gl.gen = addrspacecast ptr addrspace(1) %gl to ptr
-  store ptr %gl.gen, ptr addrspace(5) %slot, align 8
+  store ptr %gl.gen, ptr addrspace(5) %0, align 8
   br label %merge
 
-merge:
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
+merge:                                            ; preds = %global, %shared
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_partial_dynamic_array_init(i32 %selector) {
-; CHECK-LABEL: define i32 @negative_partial_dynamic_array_init(
-; CHECK:       [[SLOTS:%.*]] = addrspacecast ptr {{%.*}} to ptr addrspace(5)
-; CHECK:       [[SLOT0:%.*]] = getelementptr inbounds [2 x ptr], ptr addrspace(5) [[SLOTS]], i32 0, i32 0
-; CHECK-NEXT:  store ptr {{%.*}}, ptr addrspace(5) [[SLOT0]], align 8
-; CHECK:       [[WHICH:%.*]] = and i32 {{%.*}}, 1
-; CHECK-NEXT:  [[DYNAMIC:%.*]] = getelementptr inbounds [2 x ptr], ptr addrspace(5) [[SLOTS]], i32 0, i32 [[WHICH]]
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) [[DYNAMIC]], align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slots.generic = alloca [2 x ptr], align 8
-  %slots = addrspacecast ptr %slots.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slots.generic to ptr addrspace(5)
   %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
-  %slot0 = getelementptr inbounds [2 x ptr], ptr addrspace(5) %slots, i32 0, i32 0
+  %slot0 = getelementptr inbounds [2 x ptr], ptr addrspace(5) %0, i32 0, i32 0
   store ptr %sh.gen, ptr addrspace(5) %slot0, align 8
   %which = and i32 %selector, 1
-  %slot.dynamic = getelementptr inbounds [2 x ptr], ptr addrspace(5) %slots, i32 0, i32 %which
+  %slot.dynamic = getelementptr inbounds [2 x ptr], ptr addrspace(5) %0, i32 0, i32 %which
   %p.reload = load ptr, ptr addrspace(5) %slot.dynamic, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_unknown_call_clobber() {
-; CHECK-LABEL: define i32 @negative_unknown_call_clobber(
-; CHECK:       call void @unknown_clobber(ptr addrspace(5) {{%.*}})
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
-  store ptr %sh.gen, ptr addrspace(5) %slot, align 8
-  call void @unknown_clobber(ptr addrspace(5) %slot)
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
+  store ptr %sh.gen, ptr addrspace(5) %0, align 8
+  call void @unknown_clobber(ptr addrspace(5) %0)
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_volatile_slot_load() {
-; CHECK-LABEL: define i32 @negative_volatile_slot_load(
-; CHECK:       [[RELOAD:%.*]] = load volatile ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
-  store ptr %sh.gen, ptr addrspace(5) %slot, align 8
-  %p.reload = load volatile ptr, ptr addrspace(5) %slot, align 8
+  store ptr %sh.gen, ptr addrspace(5) %0, align 8
+  %p.reload = load volatile ptr, ptr addrspace(5) %0, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_atomic_slot_load() {
-; CHECK-LABEL: define i32 @negative_atomic_slot_load(
-; CHECK:       [[RELOAD:%.*]] = load atomic ptr, ptr addrspace(5) {{%.*}} monotonic, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
   %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
-  store ptr %sh.gen, ptr addrspace(5) %slot, align 8
-  %p.reload = load atomic ptr, ptr addrspace(5) %slot monotonic, align 8
+  store ptr %sh.gen, ptr addrspace(5) %0, align 8
+  %p.reload = load atomic ptr, ptr addrspace(5) %0 monotonic, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_slot_address_space_is_not_value_address_space(ptr %unknown) {
-; CHECK-LABEL: define i32 @negative_slot_address_space_is_not_value_address_space(
-; CHECK:       store ptr [[UNKNOWN:%.*]], ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NOT:   load i32, ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
-  store ptr %unknown, ptr addrspace(5) %slot, align 8
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  store ptr %unknown, ptr addrspace(5) %0, align 8
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
 
 define i32 @negative_inttoptr_stored_value(i64 %raw) {
-; CHECK-LABEL: define i32 @negative_inttoptr_stored_value(
-; CHECK:       [[P:%.*]] = inttoptr i64 {{%.*}} to ptr
-; CHECK-NEXT:  store ptr [[P]], ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NEXT:  [[RELOAD:%.*]] = load ptr, ptr addrspace(5) {{%.*}}, align 8
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(3)
-; CHECK-NOT:   addrspacecast ptr [[RELOAD]] to ptr addrspace(5)
-; CHECK-NEXT:  [[VALUE:%.*]] = load i32, ptr [[RELOAD]], align 4
-; CHECK-NEXT:  ret i32 [[VALUE]]
 entry:
   %slot.generic = alloca ptr, align 8
-  %slot = addrspacecast ptr %slot.generic to ptr addrspace(5)
+  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
   %p = inttoptr i64 %raw to ptr
-  store ptr %p, ptr addrspace(5) %slot, align 8
-  %p.reload = load ptr, ptr addrspace(5) %slot, align 8
+  store ptr %p, ptr addrspace(5) %0, align 8
+  %p.reload = load ptr, ptr addrspace(5) %0, align 8
   %value = load i32, ptr %p.reload, align 4
   ret i32 %value
 }
```

## Reduced PTX before infer

File: `artifacts/reduced-godbolt-pattern.before-infer.ptx`

```text
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:30: 	ld.shared.b32 	%r2, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:65: 	ld.local.b64 	%rd14, [%rd13];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:66: 	cvta.to.shared.u64 	%rd15, %rd14;
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:67: 	ld.shared.b32 	%r1, [%rd15];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:71: 	ld.global.b32 	%r2, [%rd18];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:97: 	ld.shared.b32 	%r1, [%rd2];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:123: 	ld.b32 	%r1, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:149: 	ld.local.b64 	%rd8, [%rd7];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:150: 	ld.b32 	%r1, [%rd8];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:175: 	ld.local.b64 	%rd4, [%rd1];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:176: 	ld.b32 	%r1, [%rd4];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:197: 	ld.local.b64 	%rd4, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:198: 	ld.b32 	%r1, [%rd4];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:209: 	ld.shared.b32 	%r1, [shared_storage];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:224: 	ld.b32 	%r1, [%rd1];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.before-infer.ptx:239: 	ld.b32 	%r1, [%rd1];
```

## Reduced PTX after infer

File: `artifacts/reduced-godbolt-pattern.after-infer.ptx`

```text
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:30: 	ld.shared.b32 	%r2, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:65: 	ld.local.b64 	%rd14, [%rd13];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:66: 	cvta.to.shared.u64 	%rd15, %rd14;
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:67: 	ld.shared.b32 	%r1, [%rd15];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:71: 	ld.global.b32 	%r2, [%rd18];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:97: 	ld.shared.b32 	%r1, [%rd2];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:123: 	ld.b32 	%r1, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:149: 	ld.local.b64 	%rd8, [%rd7];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:150: 	ld.b32 	%r1, [%rd8];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:175: 	ld.local.b64 	%rd4, [%rd1];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:176: 	ld.b32 	%r1, [%rd4];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:197: 	ld.local.b64 	%rd4, [%rd3];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:198: 	ld.b32 	%r1, [%rd4];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:209: 	ld.shared.b32 	%r1, [shared_storage];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:224: 	ld.b32 	%r1, [%rd1];
llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.after-infer.ptx:239: 	ld.b32 	%r1, [%rd1];
```

## Full CUDA frontend O3 PTX baseline snippets

File: `artifacts/godbolt.original-clang21.O3.ptx`

```text
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:31: 	cvta.to.global.u64 	%rd8, %rd7;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:32: 	cvta.to.global.u64 	%rd1, %rd6;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:45: 	ld.global.b32 	%r3, [%rd5];
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:54: 	cvta.shared.u64 	%rd13, %rd11;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:67: 	ld.local.b64 	%rd21, [%rd20];
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.original-clang21.O3.ptx:68: 	ld.b32 	%r8, [%rd21];
```

## Full CUDA frontend O3 PTX patched snippets

File: `artifacts/godbolt.patched-clang23.O3.ptx`

```text
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:31: 	cvta.to.global.u64 	%rd8, %rd7;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:32: 	cvta.to.global.u64 	%rd1, %rd6;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:45: 	ld.global.b32 	%r3, [%rd5];
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:56: 	cvta.shared.u64 	%rd13, %rd12;
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:67: 	ld.local.b64 	%rd19, [%rd18];
llvm/utils/memory-carried-addrspace-provenance/artifacts/godbolt.patched-clang23.O3.ptx:68: 	ld.b32 	%r8, [%rd19];
```

Conclusion: the reduced/RFC-shaped IR is the reliable pass-level proof. Full CUDA frontend O0/O3 artifacts are complete local compiler outputs for comparison; frontend and pipeline differences can reshape the source pattern before the standalone InferAddressSpaces pass sees it.
