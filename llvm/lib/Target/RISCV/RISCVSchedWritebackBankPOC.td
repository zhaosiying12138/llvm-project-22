//===-- RISCVSchedWritebackBankPOC.td - WB bank PoC model -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Minimal single-issue, in-order model for the writeback-bank scheduling PoC.
// It intentionally models one pipelined execution resource and fixed RVV
// latencies used by RISCVWritebackBankScheduler/RISCVCustomBehaviour.
def WritebackBankPOCModel : SchedMachineModel {
  let MicroOpBufferSize = 0;
  let IssueWidth = 1;
  let LoadLatency = 1;
  let MispredictPenalty = 1;
  let CompleteModel = 0;
}

let SchedModel = WritebackBankPOCModel in {

let BufferSize = 0 in {
def WBPOC_EXEC : ProcResource<1>;
}

let Latency = 1, ReleaseAtCycles = [1] in {
def : WriteRes<WriteIALU, [WBPOC_EXEC]>;
def : WriteRes<WriteIALU32, [WBPOC_EXEC]>;
def : WriteRes<WriteShiftImm, [WBPOC_EXEC]>;
def : WriteRes<WriteShiftImm32, [WBPOC_EXEC]>;
def : WriteRes<WriteShiftReg, [WBPOC_EXEC]>;
def : WriteRes<WriteShiftReg32, [WBPOC_EXEC]>;
def : WriteRes<WriteIMul, [WBPOC_EXEC]>;
def : WriteRes<WriteIMul32, [WBPOC_EXEC]>;
def : WriteRes<WriteIDiv, [WBPOC_EXEC]>;
def : WriteRes<WriteIDiv32, [WBPOC_EXEC]>;
def : WriteRes<WriteIRem, [WBPOC_EXEC]>;
def : WriteRes<WriteIRem32, [WBPOC_EXEC]>;
def : WriteRes<WriteJmp, [WBPOC_EXEC]>;
def : WriteRes<WriteJal, [WBPOC_EXEC]>;
def : WriteRes<WriteJalr, [WBPOC_EXEC]>;
def : WriteRes<WriteNop, [WBPOC_EXEC]>;
def : WriteRes<WriteLDB, [WBPOC_EXEC]>;
def : WriteRes<WriteLDH, [WBPOC_EXEC]>;
def : WriteRes<WriteLDW, [WBPOC_EXEC]>;
def : WriteRes<WriteLDD, [WBPOC_EXEC]>;
def : WriteRes<WriteSTB, [WBPOC_EXEC]>;
def : WriteRes<WriteSTH, [WBPOC_EXEC]>;
def : WriteRes<WriteSTW, [WBPOC_EXEC]>;
def : WriteRes<WriteSTD, [WBPOC_EXEC]>;
def : WriteRes<WriteCSR, [WBPOC_EXEC]>;
}

def WBPOCWriteOne : SchedWriteRes<[WBPOC_EXEC]> {
  let Latency = 1;
  let ReleaseAtCycles = [1];
}

def WBPOCWriteVADD : SchedWriteRes<[WBPOC_EXEC]> {
  let Latency = 2;
  let ReleaseAtCycles = [1];
}

def WBPOCWriteVFADD : SchedWriteRes<[WBPOC_EXEC]> {
  let Latency = 3;
  let ReleaseAtCycles = [1];
}

def WBPOCWriteVFMACC : SchedWriteRes<[WBPOC_EXEC]> {
  let Latency = 4;
  let ReleaseAtCycles = [1];
}

def : InstRW<[WriteIALU], (instrs COPY)>;
def : InstRW<[WBPOCWriteOne], (instrs VSETVLI, VSETIVLI, VSETVL)>;
def : InstRW<[WBPOCWriteOne], (instrs VMV_V_V, PseudoVMV_V_V_M1)>;
def : InstRW<[WBPOCWriteVADD], (instrs VADD_VV, PseudoVADD_VV_M1)>;
def : InstRW<[WBPOCWriteVFADD],
             (instrs VFADD_VF, PseudoVFADD_VFPR32_M1_E32)>;
def : InstRW<[WBPOCWriteVFMACC],
             (instrs VFMACC_VF, PseudoVFMACC_VFPR32_M1_E32)>;

def : ReadAdvance<ReadJmp, 0>;
def : ReadAdvance<ReadJalr, 0>;
def : ReadAdvance<ReadCSR, 0>;
def : ReadAdvance<ReadMemBase, 0>;
def : ReadAdvance<ReadStoreData, 0>;
def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadIALU32, 0>;
def : ReadAdvance<ReadShiftImm, 0>;
def : ReadAdvance<ReadShiftImm32, 0>;
def : ReadAdvance<ReadShiftReg, 0>;
def : ReadAdvance<ReadShiftReg32, 0>;
def : ReadAdvance<ReadIMul, 0>;
def : ReadAdvance<ReadIMul32, 0>;
def : ReadAdvance<ReadIDiv, 0>;
def : ReadAdvance<ReadIDiv32, 0>;
def : ReadAdvance<ReadIRem, 0>;
def : ReadAdvance<ReadIRem32, 0>;

defm : UnsupportedSchedA;
defm : UnsupportedSchedF;
defm : UnsupportedSchedSFB;
defm : UnsupportedSchedV;
defm : UnsupportedSchedZabha;
defm : UnsupportedSchedZba;
defm : UnsupportedSchedZbb;
defm : UnsupportedSchedZbc;
defm : UnsupportedSchedZbs;
defm : UnsupportedSchedZbkb;
defm : UnsupportedSchedZbkx;
defm : UnsupportedSchedZfa;
defm : UnsupportedSchedZvk;
defm : UnsupportedSchedXsf;

} // SchedModel = WritebackBankPOCModel
