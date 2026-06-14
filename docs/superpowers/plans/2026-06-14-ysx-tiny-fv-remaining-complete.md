# YSX Tiny-F/Tiny-V Remaining Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the remaining YSX tiny-F/tiny-V auto-td goals by defining the direct tiny-V ABI boundary, proving minimal tiny-V automatic-vectorization smoke, and updating final documentation and validation evidence.

**Architecture:** Keep the schema-generated instruction surface intact and restrict handwritten C++ to YSX-owned lowering, calling convention, selection, and TTI behavior. Code-changing work is serial; read-only exploration and reviews may run in parallel. The direct vector ABI is attempted first as a narrow fixed-128-bit VR-register ABI; if that proves broader than YSX-local backend work, land an explicit unsupported boundary instead.

**Tech Stack:** LLVM YSX backend C++/TableGen, Clang YSX resource header and CodeGen tests, lit/FileCheck, Python auto-td generator tests, CMake/Ninja build in `build`.

---

## Ground Rules

- Worktree: `/home/zhaosiying/.config/superpowers/worktrees/llvm-project-22.1.3-ysx/ysx-tiny-fv-ccu`
- Branch: `ysx-tiny-fv-ccu`
- Use the existing `build` directory first. Rebuild only the tools needed by the focused tests.
- Code-changing implementation is serial. Do not run two workers that write overlapping files.
- Read-only exploration, spec review, code quality review, and final audit may use subagents in parallel.
- Subagents should use `gpt-5.5` with `xhigh` reasoning when the tool surface allows explicit model selection.
- Follow TDD for each behavior change: write the lit test, run it to see the expected failure, implement the smallest change, rerun the focused test, then run adjacent tests.
- Do not introduce standard RVV frontend exposure for YSX. The `ysx_vector.h` path remains YSX-prefixed and fixed-width.

## File Structure

### New Tests

- `clang/test/CodeGen/YSX/tinyv-vector-abi.c`
  - Direct fixed-vector ABI proof for `ysx_vint32m1_t` and `ysx_vfloat32m1_t`, or explicit unsupported-boundary diagnostic if the narrow ABI gate fails.
- `llvm/test/CodeGen/YSX/tinyv-autovec.ll`
  - LLVM-level fixed-vector smoke for generic `<4 x i32>` add and reduction lowering.
- `clang/test/CodeGen/YSX/tinyv-autovec.c`
  - Clang loop-vectorizer smoke for contiguous `i32` add and integer reduction loops.

### Backend Files

- `llvm/lib/Target/YuShuXin/YSXCallingConv.cpp`
  - Narrow fixed-vector calling convention assignment for `v4i32` and `v4f32`, using VR argument/return registers when `+xtinyv` is enabled.
- `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
  - Legal/custom operation declarations for fixed tiny-V vector add/reduction and any custom lowering needed for scalar reduction results.
- `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
  - Direct selection for generic fixed-vector `ISD::ADD` and any tiny-V helper needed by reduction lowering.
- `llvm/lib/Target/YuShuXin/YSXTargetTransformInfo.h`
  - Minimal TTI hooks that let the loop vectorizer see a 128-bit fixed-vector register path only for `+xtinyv,+zvl128b`.

### Documentation

- `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`
- `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`

## Task 1: Baseline And Build Reference

**Files:**
- Read: `docs/superpowers/specs/2026-06-14-ysx-tiny-fv-remaining-complete-design.md`
- Read: `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`
- Read: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`

- [ ] **Step 1: Confirm the worktree and branch**

Run:

```bash
git status --short
git rev-parse --abbrev-ref HEAD
git rev-parse HEAD
```

Expected:

```text
ysx-tiny-fv-ccu
9891919c40e83c6bc8feef1b5e94802485c2b5df
```

The status output may be empty or may show this plan file before the plan commit. Do not start code changes with unrelated dirty source files.

- [ ] **Step 2: Build the focused tools if missing or stale**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
```

Expected: Ninja exits 0. If Ninja reports no work to do, that is acceptable.

- [ ] **Step 3: Run existing tiny-F/tiny-V focused baseline**

Run:

```bash
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX/tinyf-auto-td.s \
  llvm/test/MC/YSX/tinyv-auto-td.s \
  llvm/test/MC/YSX/tinyv-invalid-disassemble.s \
  llvm/test/CodeGen/YSX/tinyf-isel.ll \
  llvm/test/CodeGen/YSX/tinyf-abi.ll \
  llvm/test/CodeGen/YSX/tinyf-unsupported-f32-to-i64.ll \
  llvm/test/CodeGen/YSX/tinyf-unsupported-f32-to-u64.ll \
  llvm/test/CodeGen/YSX/tinyf-unsupported-i64-to-f32.ll \
  llvm/test/CodeGen/YSX/tinyf-unsupported-unordered-cmp.ll \
  llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll \
  clang/test/CodeGen/YSX/tinyv-builtins.c \
  clang/test/CodeGen/YSX/tinyv-builtins-asm.c
```

Expected: all discovered tests pass. If a baseline failure appears, stop and fix the baseline failure before adding new behavior.

- [ ] **Step 4: Commit the plan**

Run:

```bash
git add docs/superpowers/plans/2026-06-14-ysx-tiny-fv-remaining-complete.md
OMX_LORE_COMMIT_GUARD=0 git commit -m "docs: add YSX tiny FV remaining completion plan"
```

Expected: commit exits 0 and the worktree is clean.

## Task 2: Direct Fixed-Vector ABI Red Test

**Files:**
- Create: `clang/test/CodeGen/YSX/tinyv-vector-abi.c`

- [ ] **Step 1: Add the direct ABI test**

Create `clang/test/CodeGen/YSX/tinyv-vector-abi.c` with this content:

```c
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -O2 -S -o - %s | FileCheck %s --check-prefix=ASM
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -O2 -emit-obj -o %t.o %s
// RUN: llvm-objdump --triple=ysx64 --mattr=+xtinyv --no-print-imm-hex -d %t.o | FileCheck %s --check-prefix=OBJ

#include <ysx_vector.h>

ysx_vint32m1_t test_vadd_abi(ysx_vint32m1_t a, ysx_vint32m1_t b,
                             unsigned long vl) {
  return ysx_vadd_vv_i32m1(a, b, vl);
}

// ASM-LABEL: test_vadd_abi:
// ASM: vsetvli {{[a-z0-9]+}}, a0, 208
// ASM: vadd.vv
// ASM-NOT: call
// ASM: ret

// OBJ-LABEL: <test_vadd_abi>:
// OBJ: vsetvli {{[a-z0-9]+}}, a0, 208
// OBJ: vadd.vv
// OBJ: ret

ysx_vfloat32m1_t test_vfexp_abi(ysx_vfloat32m1_t x, unsigned long vl) {
  return ysx_vfexp_v_f32m1(x, vl);
}

// ASM-LABEL: test_vfexp_abi:
// ASM: vsetvli {{[a-z0-9]+}}, a0, 208
// ASM: yushuxin.vfexp
// ASM-NOT: call
// ASM: ret

// OBJ-LABEL: <test_vfexp_abi>:
// OBJ: vsetvli {{[a-z0-9]+}}, a0, 208
// OBJ: yushuxin.vfexp
// OBJ: ret

void test_call_abi(int *dst, ysx_vint32m1_t a, ysx_vint32m1_t b,
                   unsigned long vl) {
  *(ysx_vint32m1_t *)dst = test_vadd_abi(a, b, vl);
}

// ASM-LABEL: test_call_abi:
// ASM: call test_vadd_abi
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vse32.v

// OBJ-LABEL: <test_call_abi>:
// OBJ: call
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vse32.v
```

- [ ] **Step 2: Run the red test**

Run:

```bash
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-vector-abi.c
```

Expected before implementation: FAIL. The failure should be a backend lowering/selection failure for direct fixed-vector args or returns, or a FileCheck failure showing stack-only direct vector ABI rather than VR-register direct ABI.

- [ ] **Step 3: Record the observed failure**

Run:

```bash
./build/bin/clang -cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -Iclang/lib/Headers -O2 -S -o - clang/test/CodeGen/YSX/tinyv-vector-abi.c 2>&1 | sed -n '1,120p'
```

Expected: output identifies the first direct-ABI blocker. Keep this output in the task summary for review.

Do not commit this red test by itself.

## Task 3: Direct Fixed-Vector ABI Implementation Or Boundary

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/YSXCallingConv.cpp`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp` only if the red failure proves lowering needs an additional local diagnostic or return guard
- Test: `clang/test/CodeGen/YSX/tinyv-vector-abi.c`

### Narrow ABI Decision Gate

Take the narrow implementation path when the red failure is confined to one of these YSX-owned surfaces:

- `CC_YSX` assigns fixed `v4i32`/`v4f32` args or returns to stack memory instead of VR registers.
- `LowerReturn` asserts because fixed-vector returns are not register locations.
- `LowerCall` or `LowerFormalArguments` needs only the existing register copy machinery once `CC_YSX` returns VR locations.
- `YSXInstrInfo.cpp` already supports VR copy/spill/reload for `VRRegClass`.

Take the unsupported-boundary path only when implementation requires one of these broader changes:

- Standard RVV C ABI policy.
- Clang type-system changes for standard RVV frontend types.
- New public non-YSX-prefixed vector ABI surface.
- A generic vector calling convention redesign outside YSX-owned files.

### Preferred Narrow ABI Path

- [ ] **Step 1: Add fixed-vector ABI helpers in `YSXCallingConv.cpp`**

Insert these helpers after `getArgGPRs`:

```c++
static bool isYSXTinyVFixedVectorVT(MVT VT) {
  return VT == MVT::v4i32 || VT == MVT::v4f32;
}

static ArrayRef<MCPhysReg> getArgVRs() {
  static const MCPhysReg ArgVRs[] = {YSX::V8,  YSX::V9,  YSX::V10, YSX::V11,
                                     YSX::V12, YSX::V13, YSX::V14, YSX::V15};
  return ArrayRef(ArgVRs);
}
```

- [ ] **Step 2: Assign fixed tiny-V vectors before GPR-only scalar logic**

In `CC_YSX_GPROnly`, after `XLenVT` is initialized and before the `LocVT.isScalableVector()` check, add:

```c++
  if (Subtarget.hasStdExtXTinyV() && isYSXTinyVFixedVectorVT(ValVT) &&
      isYSXTinyVFixedVectorVT(LocVT) && !ArgFlags.isByVal() &&
      !ArgFlags.isVarArg()) {
    ArrayRef<MCPhysReg> ArgVRs = getArgVRs();
    if (MCRegister Reg = State.AllocateReg(ArgVRs)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }

    unsigned StoreSize = LocVT.getStoreSize();
    State.addLoc(CCValAssign::getMem(
        ValNo, ValVT, State.AllocateStack(StoreSize, Align(16)), LocVT,
        LocInfo));
    return false;
  }
```

This makes the first fixed-vector arg or return use `v8`, the second use `v9`, and so on. Scalar arguments continue to use `a0`-`a7` through `getArgGPRs`.

- [ ] **Step 3: Keep unsupported vector forms explicit**

Replace the existing scalable-vector-only diagnostic:

```c++
  if (LocVT.isScalableVector())
    reportFatalUsageError("YSX rv64ima does not support vector arguments");
```

with:

```c++
  if (LocVT.isScalableVector())
    reportFatalUsageError("YSX does not support scalable vector arguments");

  if (LocVT.isFixedLengthVector())
    reportFatalUsageError(
        "YSX fixed vector arguments require +xtinyv and a 128-bit tiny-V type");
```

This diagnostic is reached only for fixed vectors that did not match the narrow `v4i32`/`v4f32` `+xtinyv` ABI path.

- [ ] **Step 4: Run the focused ABI test**

Run:

```bash
ninja -C build clang llc llvm-objdump FileCheck
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-vector-abi.c
```

Expected: PASS. Assembly and object output show `vadd.vv`, `yushuxin.vfexp`, and call/store behavior for direct fixed-vector values.

- [ ] **Step 5: Run adjacent tiny-V builtin tests**

Run:

```bash
python3 build/bin/llvm-lit -sv \
  clang/test/CodeGen/YSX/tinyv-builtins.c \
  clang/test/CodeGen/YSX/tinyv-builtins-asm.c \
  llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll
```

Expected: PASS.

- [ ] **Step 6: Commit the ABI implementation**

Run:

```bash
git diff --check
git add llvm/lib/Target/YuShuXin/YSXCallingConv.cpp clang/test/CodeGen/YSX/tinyv-vector-abi.c
OMX_LORE_COMMIT_GUARD=0 git commit -m "feat: define YSX tiny-v fixed vector ABI"
```

Expected: commit exits 0 and worktree is clean.

### Unsupported-Boundary Fallback

Use this branch only if the Narrow ABI Decision Gate says the local path is too broad.

- [ ] **Fallback Step 1: Replace the direct ABI proof with a checked diagnostic**

Replace `clang/test/CodeGen/YSX/tinyv-vector-abi.c` with:

```c
// RUN: not %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -O2 -S -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED

#include <ysx_vector.h>

ysx_vint32m1_t test_vadd_abi(ysx_vint32m1_t a, ysx_vint32m1_t b,
                             unsigned long vl) {
  return ysx_vadd_vv_i32m1(a, b, vl);
}

// UNSUPPORTED: YSX tiny-V direct fixed-vector C ABI is not supported
```

- [ ] **Fallback Step 2: Add the diagnostic in `YSXISelLowering.cpp` or `YSXCallingConv.cpp`**

Use `reportFatalUsageError("YSX tiny-V direct fixed-vector C ABI is not supported");` at the earliest YSX-owned direct ABI lowering point that handles the red failure.

- [ ] **Fallback Step 3: Run and commit the boundary**

Run:

```bash
ninja -C build clang FileCheck
python3 build/bin/llvm-lit -sv clang/test/CodeGen/YSX/tinyv-vector-abi.c
git diff --check
git add llvm/lib/Target/YuShuXin clang/test/CodeGen/YSX/tinyv-vector-abi.c
OMX_LORE_COMMIT_GUARD=0 git commit -m "feat: define YSX tiny-v vector ABI boundary"
```

Expected: the test passes only because the diagnostic is explicit and checked. Documentation tasks must state that direct fixed-vector C ABI remains unsupported.

## Task 4: Automatic Vectorization Red Tests

**Files:**
- Create: `llvm/test/CodeGen/YSX/tinyv-autovec.ll`
- Create: `clang/test/CodeGen/YSX/tinyv-autovec.c`

- [ ] **Step 1: Add LLVM-level generic fixed-vector smoke**

Create `llvm/test/CodeGen/YSX/tinyv-autovec.ll` with this content:

```llvm
; RUN: llc -mtriple=ysx64 -mattr=+xtinyv,+zvl128b -stop-after=finalize-isel -o - %s | FileCheck %s --check-prefix=ISEL

define void @generic_v4i32_add(ptr %dst, ptr %a, ptr %b) {
; ISEL-LABEL: name: generic_v4i32_add
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VADD_VV
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VSE32_V
entry:
  %va = load <4 x i32>, ptr %a, align 16
  %vb = load <4 x i32>, ptr %b, align 16
  %sum = add <4 x i32> %va, %vb
  store <4 x i32> %sum, ptr %dst, align 16
  ret void
}

define i32 @generic_v4i32_reduce_add(ptr %a) {
; ISEL-LABEL: name: generic_v4i32_reduce_add
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VREDSUM_VS
entry:
  %va = load <4 x i32>, ptr %a, align 16
  %sum = call i32 @llvm.vector.reduce.add.v4i32(<4 x i32> %va)
  ret i32 %sum
}

declare i32 @llvm.vector.reduce.add.v4i32(<4 x i32>)
```

- [ ] **Step 2: Add Clang loop-vectorizer smoke**

Create `clang/test/CodeGen/YSX/tinyv-autovec.c` with this content:

```c
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -O3 -S -o - %s | FileCheck %s --check-prefix=ASM
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -O3 -emit-llvm -o - %s | FileCheck %s --check-prefix=IR

void tinyv_add_i32(int *__restrict dst, const int *__restrict a,
                   const int *__restrict b, int n) {
#pragma clang loop vectorize(enable) vectorize_width(4) interleave_count(1)
  for (int i = 0; i < n; ++i)
    dst[i] = a[i] + b[i];
}

// IR-LABEL: define {{.*}}tinyv_add_i32
// IR: load <4 x i32>
// IR: add <4 x i32>
// IR: store <4 x i32>

// ASM-LABEL: tinyv_add_i32:
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vle32.v
// ASM: vadd.vv
// ASM: vse32.v

int tinyv_reduce_i32(const int *__restrict a, int n) {
  int sum = 0;
#pragma clang loop vectorize(enable) vectorize_width(4) interleave_count(1)
  for (int i = 0; i < n; ++i)
    sum += a[i];
  return sum;
}

// IR-LABEL: define {{.*}}tinyv_reduce_i32
// IR: call i32 @llvm.vector.reduce.add.v4i32

// ASM-LABEL: tinyv_reduce_i32:
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vle32.v
// ASM: vredsum.vs
```

- [ ] **Step 3: Run the red autovec tests**

Run:

```bash
python3 build/bin/llvm-lit -sv \
  llvm/test/CodeGen/YSX/tinyv-autovec.ll \
  clang/test/CodeGen/YSX/tinyv-autovec.c
```

Expected before implementation: FAIL. The LLVM test should fail because generic `<4 x i32> add` or vector reduction does not select YSX tiny-V instructions. The Clang test should fail because either the loop is not vectorized or the generated vector IR cannot be selected.

Do not commit these red tests by themselves.

## Task 5: Minimal Tiny-V Autovec Implementation

**Files:**
- Modify: `llvm/lib/Target/YuShuXin/YSXTargetTransformInfo.h`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelLowering.cpp`
- Modify: `llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp`
- Test: `llvm/test/CodeGen/YSX/tinyv-autovec.ll`
- Test: `clang/test/CodeGen/YSX/tinyv-autovec.c`

- [ ] **Step 1: Advertise only a fixed 128-bit vector register to TTI**

In `YSXTargetTransformInfo.h`, replace `getRegisterBitWidth` with:

```c++
  TypeSize
  getRegisterBitWidth(TargetTransformInfo::RegisterKind K) const override {
    if (K == TargetTransformInfo::RGK_Scalar)
      return TypeSize::getFixed(ST->getXLen());
    if (K == TargetTransformInfo::RGK_FixedWidthVector &&
        ST->hasStdExtXTinyV())
      return TypeSize::getFixed(128);
    return TypeSize::getFixed(0);
  }
```

Add this method below it:

```c++
  unsigned getNumberOfRegisters(unsigned ClassID) const override {
    if (ST->hasStdExtXTinyV())
      return 8;
    return BaseT::getNumberOfRegisters(ClassID);
  }
```

Do not change `supportsScalableVectors()` or `enableScalableVectorization()`; both must remain `false`.

- [ ] **Step 2: Mark the minimal fixed-vector DAG operations**

In the `YSXTargetLowering` constructor, inside `if (Subtarget.hasStdExtXTinyV())`, add:

```c++
    setOperationAction(ISD::ADD, MVT::v4i32, Legal);
    setOperationAction(ISD::VECREDUCE_ADD, MVT::v4i32, Custom);
```

Leave non-i32 vector arithmetic and scalable vector operations unsupported for this task.

- [ ] **Step 3: Add custom lowering for `VECREDUCE_ADD v4i32`**

Add a private helper near the other lowering helpers in `YSXISelLowering.cpp`:

```c++
static SDValue lowerTinyVVecReduceAdd(SDValue Op, SelectionDAG &DAG,
                                      const YSXSubtarget &Subtarget) {
  SDLoc DL(Op);
  SDValue Vec = Op.getOperand(0);
  if (!Subtarget.hasStdExtXTinyV() ||
      Vec.getSimpleValueType() != MVT::v4i32 ||
      Op.getSimpleValueType() != MVT::i32)
    return SDValue();

  MVT XLenVT = Subtarget.getXLenVT();
  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue Seed = DAG.getSplatBuildVector(MVT::v4i32, DL, Zero);
  SDValue AVL = DAG.getConstant(4, DL, XLenVT);
  SDValue IID = DAG.getTargetConstant(Intrinsic::ysx_vredsum, DL, XLenVT);
  SDValue RedVec = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL, MVT::v4i32,
                               IID, Vec, Seed, AVL);
  return DAG.getNode(ISD::EXTRACT_VECTOR_ELT, DL, MVT::i32, RedVec,
                     DAG.getConstant(0, DL, XLenVT));
}
```

Add this case to `LowerOperation`:

```c++
  case ISD::VECREDUCE_ADD:
    return lowerTinyVVecReduceAdd(Op, DAG, Subtarget);
```

If the build shows `getSplatBuildVector` is unavailable in this LLVM revision, replace the `Seed` line with:

```c++
  SDValue Seed = DAG.getBuildVector(MVT::v4i32, DL,
                                    {Zero, Zero, Zero, Zero});
```

- [ ] **Step 4: Select generic fixed-vector add**

In `YSXISelDAGToDAG.cpp`, add this helper after `selectTinyVVectorOp`:

```c++
static MachineSDNode *selectTinyVFixedVectorOp(SelectionDAG *CurDAG,
                                               const YSXSubtarget *Subtarget,
                                               const SDLoc &DL, MVT VT,
                                               unsigned MachineOpcode,
                                               SDValue LHS, SDValue RHS) {
  SDValue Glue = emitTinyVSetIVLI(CurDAG, Subtarget, DL);
  SDValue Mask = CurDAG->getRegister(YSX::NoRegister, VT);
  SDValue Ops[] = {LHS, RHS, Mask, Glue};
  return CurDAG->getMachineNode(MachineOpcode, DL, VT, Ops);
}
```

Add this case in `Select`, before the scalar `ISD::ADD` handling:

```c++
  case ISD::ADD: {
    if (Subtarget->hasStdExtXTinyV() && VT == MVT::v4i32) {
      MachineSDNode *New = selectTinyVFixedVectorOp(
          CurDAG, Subtarget, DL, VT, YSX::YSX_AUTO_VADD_VV,
          Node->getOperand(0), Node->getOperand(1));
      ReplaceNode(Node, New);
      return;
    }
    break;
  }
```

- [ ] **Step 5: Select scalar splat seed if needed**

If the reduction test fails because the zero seed `BUILD_VECTOR` or splat cannot be selected, add this case in `YSXISelDAGToDAG.cpp`:

```c++
  case ISD::BUILD_VECTOR: {
    if (!Subtarget->hasStdExtXTinyV() || VT != MVT::v4i32)
      break;
    if (!llvm::all_of(Node->ops(), [](SDValue Op) {
          auto *C = dyn_cast<ConstantSDNode>(Op);
          return C && C->isZero();
        }))
      break;

    SDValue Glue = emitTinyVSetIVLI(CurDAG, Subtarget, DL);
    SDValue Zero = CurDAG->getRegister(YSX::X0, Subtarget->getXLenVT());
    SDValue Mask = CurDAG->getRegister(YSX::NoRegister, VT);
    SDValue Ops[] = {Zero, Mask, Glue};
    MachineSDNode *New =
        CurDAG->getMachineNode(YSX::YSX_AUTO_VMV_V_X, DL, VT, Ops);
    ReplaceNode(Node, New);
    return;
  }
```

If the generated `YSX_AUTO_VMV_V_X` operand order differs, inspect the generated instruction record with:

```bash
rg -n "YSX_AUTO_VMV_V_X" build/lib/Target/YuShuXin/YSXGenInstrInfo.inc llvm/lib/Target/YuShuXin
```

Then adjust only this case to match the generated operand order.

- [ ] **Step 6: Run the focused autovec tests**

Run:

```bash
ninja -C build clang llc FileCheck
python3 build/bin/llvm-lit -sv \
  llvm/test/CodeGen/YSX/tinyv-autovec.ll \
  clang/test/CodeGen/YSX/tinyv-autovec.c
```

Expected: PASS. The LLVM test must show `YSX_AUTO_VADD_VV` and `YSX_AUTO_VREDSUM_VS`. The Clang test must show vector IR for add/reduction and final assembly with `vadd.vv` and `vredsum.vs`.

- [ ] **Step 7: Run adjacent CodeGen tests**

Run:

```bash
python3 build/bin/llvm-lit -sv \
  llvm/test/CodeGen/YSX/tinyv-builtins-isel.ll \
  clang/test/CodeGen/YSX/tinyv-builtins.c \
  clang/test/CodeGen/YSX/tinyv-builtins-asm.c \
  llvm/test/CodeGen/YSX/tinyf-isel.ll \
  llvm/test/CodeGen/YSX/tinyf-abi.ll
```

Expected: PASS.

- [ ] **Step 8: Commit the autovec implementation**

Run:

```bash
git diff --check
git add \
  llvm/lib/Target/YuShuXin/YSXTargetTransformInfo.h \
  llvm/lib/Target/YuShuXin/YSXISelLowering.cpp \
  llvm/lib/Target/YuShuXin/YSXISelDAGToDAG.cpp \
  llvm/test/CodeGen/YSX/tinyv-autovec.ll \
  clang/test/CodeGen/YSX/tinyv-autovec.c
OMX_LORE_COMMIT_GUARD=0 git commit -m "feat: add YSX tiny-v autovec smoke"
```

Expected: commit exits 0 and worktree is clean.

## Task 6: Documentation, Final Validation, And Audit

**Files:**
- Modify: `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`
- Modify: `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`
- Modify: `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`
- Modify: `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`

- [ ] **Step 1: Update the design spec final-state notes**

In `docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md`, update the sections that currently say direct vector ABI and automatic vectorization are not complete. The final wording must distinguish:

```text
- The auto-td generated instruction surface remains complete for the agreed first slice.
- Explicit YSX-prefixed builtin CodeGen and object paths are supported for selected fixed 128-bit tiny-V operations.
- Direct fixed-vector ABI is supported only for the narrow YSX tiny-V v4i32/v4f32 path, or explicitly unsupported if Task 3 used the fallback.
- Automatic vectorization is proven only for the minimal contiguous i32 add and i32 reduction smoke tests.
```

- [ ] **Step 2: Update the feature checklist**

In `docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md`, add verification entries for:

```text
- clang/test/CodeGen/YSX/tinyv-vector-abi.c
- llvm/test/CodeGen/YSX/tinyv-autovec.ll
- clang/test/CodeGen/YSX/tinyv-autovec.c
```

Update the remaining-scope section so it no longer lists these three tasks as open. If Task 3 used fallback, list direct vector ABI as a checked unsupported boundary rather than implemented support.

- [ ] **Step 3: Update the blog**

In `docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md`, update the "current boundary" or equivalent section with:

```text
The final tiny-V proof remains deliberately narrow: generated instruction records and explicit builtins cover the selected fixed 128-bit surface, and the new autovec smoke only covers contiguous i32 add/reduce loops. This is not a full RVV frontend or a full vector ABI claim.
```

If Task 3 implemented the narrow ABI, also state:

```text
Direct vector values are only proven for YSX fixed v4i32/v4f32 tiny-V values on the YSX backend path.
```

If Task 3 used fallback, instead state:

```text
Direct vector C ABI remains an explicit unsupported boundary; supported C code keeps vector values inside builtins and memory stores.
```

- [ ] **Step 4: Mark the original remaining plan closed**

In `docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md`, add a short completion note under Tasks 5, 6, and 7 naming the new tests and final behavior. Use checked boxes only for steps actually completed in this pass.

- [ ] **Step 5: Run generator unit tests**

Run:

```bash
python3 -m unittest discover -s llvm/lib/Target/YuShuXin/auto-td/tests -p 'test_*.py' -v
```

Expected:

```text
Ran 36 tests
OK
```

- [ ] **Step 6: Run generator coverage review**

Run:

```bash
rm -rf build/ysx-auto-td-final-review
python3 llvm/lib/Target/YuShuXin/auto-td/tools/ysx_auto_td_gen.py \
  --ysx-root llvm/lib/Target/YuShuXin \
  --riscv-opcodes third_party/riscv-opcodes \
  --ysx-opcodes third_party/ysx-opcodes \
  --out-dir build/ysx-auto-td-final-review \
  --coverage build/ysx-auto-td-final-review/coverage.md
rg -n "auto_full: 50|retained_schema_gap: 0" build/ysx-auto-td-final-review/coverage.md
```

Expected: both `auto_full: 50` and `retained_schema_gap: 0` appear in the coverage report.

- [ ] **Step 7: Run final focused lit validation**

Run:

```bash
ninja -C build clang llc llvm-mc llvm-objdump FileCheck opt llvm-readelf
python3 build/bin/llvm-lit -sv \
  llvm/test/MC/YSX \
  llvm/test/CodeGen/YSX \
  clang/test/Driver/YSX \
  clang/test/CodeGen/YSX
```

Expected: all discovered tests pass.

- [ ] **Step 8: Run whitespace and status checks**

Run:

```bash
git diff --check
git status --short
```

Expected: `git diff --check` exits 0. `git status --short` shows only the documentation files changed before the docs commit.

- [ ] **Step 9: Commit documentation**

Run:

```bash
git add docs/superpowers/specs/2026-05-08-ysx-tiny-fv-auto-td-design.md \
        docs/superpowers/implementation/ysx-tiny-fv-feature-checklist.md \
        docs/superpowers/blog/2026-05-08-ysx-tiny-fv-auto-td.md \
        docs/superpowers/plans/2026-05-08-ysx-tiny-fv-remaining.md
OMX_LORE_COMMIT_GUARD=0 git commit -m "docs: summarize completed YSX tiny FV remaining work"
```

Expected: commit exits 0.

- [ ] **Step 10: Run read-only final audit**

Dispatch a read-only audit subagent with this prompt:

```text
Review the full diff in /home/zhaosiying/.config/superpowers/worktrees/llvm-project-22.1.3-ysx/ysx-tiny-fv-ccu from commit 9891919c40e83c6bc8feef1b5e94802485c2b5df to HEAD. Do not edit files. Evaluate whether the approved remaining-complete design and plan are fully satisfied. Focus on: direct tiny-V ABI boundary, minimal autovec add/reduce smoke, no generic RVV frontend exposure, generator coverage, focused lit validation, and documentation honesty. Return Critical/Important/Minor findings with file:line references, plus an explicit PASS/FAIL audit verdict.
```

Expected: PASS audit verdict or only Minor findings that do not affect the acceptance criteria. Fix Critical and Important findings, rerun affected tests, commit fixes, and rerun the final audit until it passes.

- [ ] **Step 11: Close the goal**

Run:

```bash
git status --short
git log --oneline -5
```

Expected: worktree is clean. The recent log includes the plan commit, ABI/boundary commit, autovec commit, and docs commit. Mark the active Codex goal complete only after the final audit has passed.
