; ModuleID = 'llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.ll'
source_filename = "llvm/utils/memory-carried-addrspace-provenance/artifacts/reduced-godbolt-pattern.ll"
target datalayout = "e-p:64:64-p1:64:64-p3:64:64-p5:64:64-i64:64-i128:128-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

@shared_storage = internal addrspace(3) global [256 x i32] undef, align 4
@global_storage = internal addrspace(1) global [256 x i32] undef, align 4

declare void @unknown_clobber(ptr addrspace(5))

define i32 @positive_single_slot(i32 %idx) {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  %sh.gep = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 %idx
  %sh.generic = addrspacecast ptr addrspace(3) %sh.gep to ptr
  store ptr %sh.generic, ptr addrspace(5) %0, align 8
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
  %value = load i32, ptr addrspace(3) %1, align 4
  ret i32 %value
}

define i32 @positive_dynamic_array(i32 %selector) {
entry:
  %slots.generic = alloca [4 x ptr], align 8
  %0 = addrspacecast ptr %slots.generic to ptr addrspace(5)
  %sh0 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %gen0 = addrspacecast ptr addrspace(3) %sh0 to ptr
  %slot0 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 0
  store ptr %gen0, ptr addrspace(5) %slot0, align 8
  %sh1 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 1
  %gen1 = addrspacecast ptr addrspace(3) %sh1 to ptr
  %slot1 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 1
  store ptr %gen1, ptr addrspace(5) %slot1, align 8
  %sh2 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 2
  %gen2 = addrspacecast ptr addrspace(3) %sh2 to ptr
  %slot2 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 2
  store ptr %gen2, ptr addrspace(5) %slot2, align 8
  %sh3 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 3
  %gen3 = addrspacecast ptr addrspace(3) %sh3 to ptr
  %slot3 = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 3
  store ptr %gen3, ptr addrspace(5) %slot3, align 8
  %which = and i32 %selector, 3
  %slot.dynamic = getelementptr inbounds [4 x ptr], ptr addrspace(5) %0, i32 0, i32 %which
  %p.reload = load ptr, ptr addrspace(5) %slot.dynamic, align 8
  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
  %value = load i32, ptr addrspace(3) %1, align 4
  %global.gep = getelementptr inbounds [256 x i32], ptr addrspace(1) @global_storage, i32 0, i32 %which
  %global = load i32, ptr addrspace(1) %global.gep, align 4
  %sum = add i32 %value, %global
  ret i32 %sum
}

define i32 @positive_cfg_merge(i1 %cond) {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  br i1 %cond, label %left, label %right

left:                                             ; preds = %entry
  %sh0 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 10
  %gen0 = addrspacecast ptr addrspace(3) %sh0 to ptr
  store ptr %gen0, ptr addrspace(5) %0, align 8
  br label %merge

right:                                            ; preds = %entry
  %sh1 = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 20
  %gen1 = addrspacecast ptr addrspace(3) %sh1 to ptr
  store ptr %gen1, ptr addrspace(5) %0, align 8
  br label %merge

merge:                                            ; preds = %right, %left
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %1 = addrspacecast ptr %p.reload to ptr addrspace(3)
  %value = load i32, ptr addrspace(3) %1, align 4
  ret i32 %value
}

define i32 @negative_conflicting_shared_global(i1 %cond) {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  br i1 %cond, label %shared, label %global

shared:                                           ; preds = %entry
  %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
  store ptr %sh.gen, ptr addrspace(5) %0, align 8
  br label %merge

global:                                           ; preds = %entry
  %gl = getelementptr inbounds [256 x i32], ptr addrspace(1) @global_storage, i32 0, i32 0
  %gl.gen = addrspacecast ptr addrspace(1) %gl to ptr
  store ptr %gl.gen, ptr addrspace(5) %0, align 8
  br label %merge

merge:                                            ; preds = %global, %shared
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_partial_dynamic_array_init(i32 %selector) {
entry:
  %slots.generic = alloca [2 x ptr], align 8
  %0 = addrspacecast ptr %slots.generic to ptr addrspace(5)
  %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
  %slot0 = getelementptr inbounds [2 x ptr], ptr addrspace(5) %0, i32 0, i32 0
  store ptr %sh.gen, ptr addrspace(5) %slot0, align 8
  %which = and i32 %selector, 1
  %slot.dynamic = getelementptr inbounds [2 x ptr], ptr addrspace(5) %0, i32 0, i32 %which
  %p.reload = load ptr, ptr addrspace(5) %slot.dynamic, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_unknown_call_clobber() {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
  store ptr %sh.gen, ptr addrspace(5) %0, align 8
  call void @unknown_clobber(ptr addrspace(5) %0)
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_volatile_slot_load() {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
  store ptr %sh.gen, ptr addrspace(5) %0, align 8
  %p.reload = load volatile ptr, ptr addrspace(5) %0, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_atomic_slot_load() {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  %sh = getelementptr inbounds [256 x i32], ptr addrspace(3) @shared_storage, i32 0, i32 0
  %sh.gen = addrspacecast ptr addrspace(3) %sh to ptr
  store ptr %sh.gen, ptr addrspace(5) %0, align 8
  %p.reload = load atomic ptr, ptr addrspace(5) %0 monotonic, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_slot_address_space_is_not_value_address_space(ptr %unknown) {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  store ptr %unknown, ptr addrspace(5) %0, align 8
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}

define i32 @negative_inttoptr_stored_value(i64 %raw) {
entry:
  %slot.generic = alloca ptr, align 8
  %0 = addrspacecast ptr %slot.generic to ptr addrspace(5)
  %p = inttoptr i64 %raw to ptr
  store ptr %p, ptr addrspace(5) %0, align 8
  %p.reload = load ptr, ptr addrspace(5) %0, align 8
  %value = load i32, ptr %p.reload, align 4
  ret i32 %value
}
