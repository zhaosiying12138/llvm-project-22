//===-- YSXProcessors.td - YuShuXin Processors -------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Predefined scheduling direction.
defvar TopDown       = [{ MISched::TopDown }];
defvar BottomUp      = [{ MISched::BottomUp }];
defvar Bidirectional = [{ MISched::Bidirectional }];

class YSXTuneInfo {
  bits<8> PrefFunctionAlignment = 1;
  bits<8> PrefLoopAlignment = 1;

  bits<16> CacheLineSize = 0;
  bits<16> PrefetchDistance = 0;
  bits<16> MinPrefetchStride = 1;
  bits<32> MaxPrefetchIterationsAhead = -1;

  bits<32> MinimumJumpTableEntries = 5;
  bits<32> TailDupAggressiveThreshold = 6;
  bits<32> MaxStoresPerMemsetOptSize = 4;
  bits<32> MaxStoresPerMemset = 8;
  bits<32> MaxGluedStoresPerMemcpy = 0;
  bits<32> MaxStoresPerMemcpyOptSize = 4;
  bits<32> MaxStoresPerMemcpy = 8;
  bits<32> MaxStoresPerMemmoveOptSize = 4;
  bits<32> MaxStoresPerMemmove = 8;
  bits<32> MaxLoadsPerMemcmpOptSize = 4;
  bits<32> MaxLoadsPerMemcmp = 8;

  code PostRASchedDirection = TopDown;
}

def YSXTuneInfoTable : GenericTable {
  let FilterClass = "YSXTuneInfo";
  let CppTypeName = "YSXTuneInfo";
  let Fields = ["Name", "PrefFunctionAlignment", "PrefLoopAlignment",
                "CacheLineSize", "PrefetchDistance", "MinPrefetchStride",
                "MaxPrefetchIterationsAhead", "MinimumJumpTableEntries",
                "TailDupAggressiveThreshold", "MaxStoresPerMemsetOptSize",
                "MaxStoresPerMemset", "MaxGluedStoresPerMemcpy",
                "MaxStoresPerMemcpyOptSize", "MaxStoresPerMemcpy",
                "MaxStoresPerMemmoveOptSize", "MaxStoresPerMemmove",
                "MaxLoadsPerMemcmpOptSize", "MaxLoadsPerMemcmp",
                "PostRASchedDirection"];
}

def getYSXTuneInfo : SearchIndex {
  let Table = YSXTuneInfoTable;
  let Key = ["Name"];
}

class GenericTuneInfo : YSXTuneInfo;

class YSXProcessorModel<string n, SchedMachineModel m,
                        list<SubtargetFeature> f,
                        list<SubtargetFeature> tunef = [],
                        string default_march = "">
    : ProcessorModel<n, m, f, tunef> {
  string DefaultMarch = default_march;
  int MArchID = 0;
  int MImpID = 0;
}

def GENERIC
    : YSXProcessorModel<"generic", NoSchedModel,
                        [Feature64Bit, FeatureStdExtI, FeatureStdExtM,
                         FeatureStdExtA],
                        [], "rv64ima">,
      GenericTuneInfo;

def GENERIC_RV64
    : YSXProcessorModel<"generic-rv64", NoSchedModel,
                        [Feature64Bit, FeatureStdExtI, FeatureStdExtM,
                         FeatureStdExtA],
                        [], "rv64ima">,
      GenericTuneInfo;
