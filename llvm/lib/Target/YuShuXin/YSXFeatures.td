//===-- YSXFeatures.td - RISC-V Features and Extensions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// RISC-V subtarget features and instruction predicates.
//===----------------------------------------------------------------------===//

// Subclass of SubtargetFeature to be used when the feature is also a RISC-V
// extension. Extensions have a version and may be experimental.
// NOTE: The extension name must start with
//       - "FeatureStdExt" for standard extensions
//       - "YSXDisabledVendorFeature" for vendor-specific extensions
//
// major     - Major version of extension.
// minor     - Minor version of extension.
// desc      - Description of extension.
// implies   - Extensions or features implied by this extension.
// fieldname - name of field to create in YSXSubtarget. By default replaces
//             uses the record name by replacing Feature with Has.
// value     - Value to assign to the field in YSXSubtarget when this
//             extension is enabled. Usually "true", but can be changed.
// IsExperimental
//           - Indicates whether the extension is experimental
class YSXExtension<int major, int minor, string desc,
                     list<SubtargetFeature> implies = [],
                     string fieldname = !subst("Feature", "Has", NAME),
                     string value = "true", bit IsExperimental = false>
    : SubtargetFeature<"", fieldname, value, "", implies> {
  defvar extName = !subst("YSXDisabledVendorFeature", "", !subst("FeatureStdExt", "", NAME));

  // Name - Name of the extension in lower case.
  let Name = !if(IsExperimental, "experimental-", "") # !tolower(extName);

  // Desc - Description of extension with its name.
  let Desc = "'" # extName # "' (" # desc # ")";

  // MajorVersion - The major version for this extension.
  int MajorVersion = major;

  // MinorVersion - The minor version for this extension.
  int MinorVersion = minor;

  // Experimental - Does extension require -menable-experimental-extensions.
  bit Experimental = false;
}

// The groupID/bitmask of YSXExtension is used to retrieve a specific bit value
// from __ysx_feature_bits based on the groupID and bitmask.
// groupID - groupID of extension
// bitPos  - bit position of extension bitmask
class YSXExtensionBitmask<bits<3> groupID, int bitPos> {
    int GroupID = groupID;
    int BitPos = bitPos;
}

// Version of YSXExtension to be used for Experimental extensions. This
// sets the Experimental flag and prepends experimental- to the -mattr name.
class YSXExperimentalExtension<int major, int minor, string desc,
                                 list<YSXExtension> implies = [],
                                 string fieldname = !subst("Feature", "Has", NAME),
                                 string value = "true">
    : YSXExtension<major, minor, desc, implies, fieldname, value, true> {
  let Experimental = true;
}

// Integer Extensions

def FeatureStdExtI
    : YSXExtension<2, 1, "Base Integer Instruction Set">,
      YSXExtensionBitmask<0, 8>;

def FeatureStdExtE
    : YSXExtension<2, 0, "Embedded Instruction Set with 16 GPRs">,
      YSXExtensionBitmask<0, 4>;

def FeatureStdExtZibi
    : YSXExperimentalExtension<0, 1, "Branch with Immediate">;
def HasStdExtZibi : Predicate<"Subtarget->hasStdExtZibi()">,
                    AssemblerPredicate<(all_of FeatureStdExtZibi),
                                       "'Zibi' (Branch with Immediate)">;

def FeatureStdExtZic64b
    : YSXExtension<1, 0, "Cache Block Size Is 64 Bytes">;

def FeatureStdExtZicbom
    : YSXExtension<1, 0, "Cache-Block Management Instructions">;
def HasStdExtZicbom : Predicate<"Subtarget->hasStdExtZicbom()">,
                      AssemblerPredicate<(all_of FeatureStdExtZicbom),
                          "'Zicbom' (Cache-Block Management Instructions)">;

def FeatureStdExtZicbop
    : YSXExtension<1, 0, "Cache-Block Prefetch Instructions">;
def HasStdExtZicbop : Predicate<"Subtarget->hasStdExtZicbop()">,
                      AssemblerPredicate<(all_of FeatureStdExtZicbop),
                          "'Zicbop' (Cache-Block Prefetch Instructions)">;

def FeatureStdExtZicboz
    : YSXExtension<1, 0, "Cache-Block Zero Instructions">,
      YSXExtensionBitmask<0, 37>;
def HasStdExtZicboz : Predicate<"Subtarget->hasStdExtZicboz()">,
                      AssemblerPredicate<(all_of FeatureStdExtZicboz),
                          "'Zicboz' (Cache-Block Zero Instructions)">;

def FeatureStdExtZiccamoa
    : YSXExtension<1, 0, "Main Memory Supports All Atomics in A">;

def FeatureStdExtZiccamoc
    : YSXExtension<1, 0, "Main Memory Supports Atomics in Zacas">;

def FeatureStdExtZiccif
    : YSXExtension<1, 0,
                     "Main Memory Supports Instruction Fetch with Atomicity Requirement">;

def FeatureStdExtZicclsm
    : YSXExtension<1, 0, "Main Memory Supports Misaligned Loads/Stores">;

def FeatureStdExtZiccrse
    : YSXExtension<1, 0,
                     "Main Memory Supports Forward Progress on LR/SC Sequences">;

def FeatureStdExtZicsr
    : YSXExtension<2, 0, "CSRs">;
def HasStdExtZicsr : Predicate<"Subtarget->hasStdExtZicsr()">,
                     AssemblerPredicate<(all_of FeatureStdExtZicsr),
                                        "'Zicsr' (CSRs)">;

def FeatureStdExtZicntr
    : YSXExtension<2, 0, "Base Counters and Timers",
                     [FeatureStdExtZicsr]>;

def FeatureStdExtZicond
    : YSXExtension<1, 0, "Integer Conditional Operations">,
      YSXExtensionBitmask<0, 38>;
def HasStdExtZicond : Predicate<"Subtarget->hasStdExtZicond()">,
                      AssemblerPredicate<(all_of FeatureStdExtZicond),
                          "(Integer Conditional Operations)">;

def FeatureStdExtZifencei
    : YSXExtension<2, 0, "fence.i">;
def HasStdExtZifencei : Predicate<"Subtarget->hasStdExtZifencei()">,
                        AssemblerPredicate<(all_of FeatureStdExtZifencei),
                                           "'Zifencei' (fence.i)">;

def FeatureStdExtZihintpause
    : YSXExtension<2, 0, "Pause Hint">,
      YSXExtensionBitmask<0, 40>;
def HasStdExtZihintpause : Predicate<"Subtarget->hasStdExtZihintpause()">,
                           AssemblerPredicate<(all_of FeatureStdExtZihintpause),
                                              "'Zihintpause' (Pause Hint)">;

def FeatureStdExtZihintntl
    : YSXExtension<1, 0, "Non-Temporal Locality Hints">,
      YSXExtensionBitmask<0, 39>;
def HasStdExtZihintntl : Predicate<"Subtarget->hasStdExtZihintntl()">,
                         AssemblerPredicate<(all_of FeatureStdExtZihintntl),
                             "'Zihintntl' (Non-Temporal Locality Hints)">;

def FeatureStdExtZihpm
    : YSXExtension<2, 0, "Hardware Performance Counters",
                     [FeatureStdExtZicsr]>;

def FeatureStdExtZimop : YSXExtension<1, 0, "May-Be-Operations">,
                         YSXExtensionBitmask<1, 1>;
def HasStdExtZimop : Predicate<"Subtarget->hasStdExtZimop()">,
                     AssemblerPredicate<(all_of FeatureStdExtZimop),
                                        "'Zimop' (May-Be-Operations)">;

def FeatureStdExtZicfilp
    : YSXExperimentalExtension<1, 0, "Landing pad",
                                 [FeatureStdExtZicsr]>;
def HasStdExtZicfilp : Predicate<"Subtarget->hasStdExtZicfilp()">,
                       AssemblerPredicate<(all_of FeatureStdExtZicfilp),
                                          "'Zicfilp' (Landing pad)">;
def NoStdExtZicfilp : Predicate<"!Subtarget->hasStdExtZicfilp()">,
                      AssemblerPredicate<(all_of (not FeatureStdExtZicfilp))>;

def FeatureStdExtZicfiss
    : YSXExperimentalExtension<1, 0, "Shadow stack",
                                 [FeatureStdExtZicsr, FeatureStdExtZimop]>;
def HasStdExtZicfiss : Predicate<"Subtarget->hasStdExtZicfiss()">,
                       AssemblerPredicate<(all_of FeatureStdExtZicfiss),
                                          "'Zicfiss' (Shadow stack)">;

def FeatureStdExtZilsd
    : YSXExtension<1, 0,
                     "Load/Store Pair Instructions">,
      YSXExtensionBitmask<1, 8>;
def HasStdExtZilsd : Predicate<"Subtarget->hasStdExtZilsd()">,
                       AssemblerPredicate<(all_of FeatureStdExtZilsd),
                                          "'Zilsd' (Load/Store pair instructions)">;

def FeatureZilsd4ByteAlign
    : SubtargetFeature<"zilsd-4byte-align", "AllowZilsd4ByteAlign", "true",
                       "Allow 4-byte alignment for Zilsd LD/SD instructions">;

// Multiply Extensions

def FeatureStdExtZmmul
    : YSXExtension<1, 0, "Integer Multiplication">;
def HasStdExtZmmul : Predicate<"Subtarget->hasStdExtZmmul()">,
                     AssemblerPredicate<(all_of FeatureStdExtZmmul),
                     "'Zmmul' (Integer Multiplication)">;

def FeatureStdExtM
    : YSXExtension<2, 0, "Integer Multiplication and Division",
                     [FeatureStdExtZmmul]>,
      YSXExtensionBitmask<0, 12>;
def HasStdExtM : Predicate<"Subtarget->hasStdExtM()">,
                 AssemblerPredicate<(all_of FeatureStdExtM),
                     "'M' (Integer Multiplication and Division)">;

// Atomic Extensions

def FeatureStdExtZaamo
    : YSXExtension<1, 0, "Atomic Memory Operations">;
def HasStdExtZaamo
    : Predicate<"Subtarget->hasStdExtZaamo()">,
      AssemblerPredicate<(any_of FeatureStdExtZaamo),
                         "'Zaamo' (Atomic Memory Operations)">;
def NoStdExtZaamo : Predicate<"!Subtarget->hasStdExtZaamo()">;

def FeatureStdExtZalrsc
    : YSXExtension<1, 0, "Load-Reserved/Store-Conditional">;
def HasStdExtZalrsc
    : Predicate<"Subtarget->hasStdExtZalrsc()">,
      AssemblerPredicate<(any_of FeatureStdExtZalrsc),
                         "'Zalrsc' (Load-Reserved/Store-Conditional)">;

def FeatureStdExtA
    : YSXExtension<2, 1, "Atomic Instructions",
                     [FeatureStdExtZaamo, FeatureStdExtZalrsc]>,
      YSXExtensionBitmask<0, 0>;
def HasStdExtA : Predicate<"Subtarget->hasStdExtA()">,
                 AssemblerPredicate<(all_of FeatureStdExtA),
                                    "'A' (Atomic Instructions)">;

def FeatureStdExtZtso
    : YSXExtension<1, 0, "Memory Model - Total Store Order">,
      YSXExtensionBitmask<0, 47>;
def HasStdExtZtso : Predicate<"Subtarget->hasStdExtZtso()">,
                    AssemblerPredicate<(all_of FeatureStdExtZtso),
                        "'Ztso' (Memory Model - Total Store Order)">;
def NoStdExtZtso : Predicate<"!Subtarget->hasStdExtZtso()">;

def FeatureStdExtZa64rs
    : YSXExtension<1, 0, "Reservation Set Size of at Most 64 Bytes">;

def FeatureStdExtZa128rs
    : YSXExtension<1, 0, "Reservation Set Size of at Most 128 Bytes">;

def FeatureStdExtZabha
    : YSXExtension<1, 0, "Byte and Halfword Atomic Memory Operations",
                     [FeatureStdExtZaamo]>;
def HasStdExtZabha : Predicate<"Subtarget->hasStdExtZabha()">,
                     AssemblerPredicate<(all_of FeatureStdExtZabha),
                         "'Zabha' (Byte and Halfword Atomic Memory Operations)">;

def FeatureStdExtZacas
    : YSXExtension<1, 0, "Atomic Compare-And-Swap Instructions",
                     [FeatureStdExtZaamo]>,
      YSXExtensionBitmask<0, 26>;
def HasStdExtZacas : Predicate<"Subtarget->hasStdExtZacas()">,
                     AssemblerPredicate<(all_of FeatureStdExtZacas),
                         "'Zacas' (Atomic Compare-And-Swap Instructions)">;
def NoStdExtZacas : Predicate<"!Subtarget->hasStdExtZacas()">;

def FeatureStdExtZalasr
    : YSXExtension<1, 0, "Load-Acquire and Store-Release Instructions">;
def HasStdExtZalasr : Predicate<"Subtarget->hasStdExtZalasr()">,
                      AssemblerPredicate<(all_of FeatureStdExtZalasr),
                          "'Zalasr' (Load-Acquire and Store-Release Instructions)">;

def FeatureStdExtZama16b
    : YSXExtension<1, 0, "Atomic 16-byte misaligned loads, stores and AMOs">;

def FeatureStdExtZawrs : YSXExtension<1, 0, "Wait on Reservation Set">,
                         YSXExtensionBitmask<1, 7>;
def HasStdExtZawrs : Predicate<"Subtarget->hasStdExtZawrs()">,
                     AssemblerPredicate<(all_of FeatureStdExtZawrs),
                                        "'Zawrs' (Wait on Reservation Set)">;

// Floating Point Extensions

def YSXDisabledStdExtF
    : YSXExtension<2, 2, "Single-Precision Floating-Point",
                     [FeatureStdExtZicsr]>,
      YSXExtensionBitmask<0, 5>;
def HasStdExtF : Predicate<"Subtarget->hasStdExtF()">,
                 AssemblerPredicate<(all_of YSXDisabledStdExtF),
                                    "'F' (Single-Precision Floating-Point)">;

def FeatureStdExtD
    : YSXExtension<2, 2, "Double-Precision Floating-Point",
                     [YSXDisabledStdExtF]>,
      YSXExtensionBitmask<0, 3>;
def HasStdExtD : Predicate<"Subtarget->hasStdExtD()">,
                 AssemblerPredicate<(all_of FeatureStdExtD),
                                    "'D' (Double-Precision Floating-Point)">;

def FeatureStdExtQ
    : YSXExtension<2, 2, "Quad-Precision Floating-Point", [FeatureStdExtD]>,
      YSXExtensionBitmask<0, 16>;
def HasStdExtQ : Predicate<"Subtarget->hasStdExtQ()">,
                 AssemblerPredicate<(all_of FeatureStdExtQ),
                                    "'Q' (Quad-Precision Floating-Point)">;

def FeatureStdExtZfhmin
    : YSXExtension<1, 0, "Half-Precision Floating-Point Minimal",
                     [YSXDisabledStdExtF]>,
      YSXExtensionBitmask<0, 36>;
def HasStdExtZfhmin : Predicate<"Subtarget->hasStdExtZfhmin()">,
                      AssemblerPredicate<(all_of FeatureStdExtZfhmin),
                          "'Zfh' (Half-Precision Floating-Point) or "
                          "'Zfhmin' (Half-Precision Floating-Point Minimal)">;

def FeatureStdExtZfh
    : YSXExtension<1, 0, "Half-Precision Floating-Point",
                     [FeatureStdExtZfhmin]>,
      YSXExtensionBitmask<0, 35>;
def HasStdExtZfh : Predicate<"Subtarget->hasStdExtZfh()">,
                   AssemblerPredicate<(all_of FeatureStdExtZfh),
                       "'Zfh' (Half-Precision Floating-Point)">;
def NoStdExtZfh : Predicate<"!Subtarget->hasStdExtZfh()">;

def FeatureStdExtZfbfmin
    : YSXExtension<1, 0, "Scalar BF16 Converts", [YSXDisabledStdExtF]>;
def HasStdExtZfbfmin : Predicate<"Subtarget->hasStdExtZfbfmin()">,
                       AssemblerPredicate<(all_of FeatureStdExtZfbfmin),
                                          "'Zfbfmin' (Scalar BF16 Converts)">;

def HasHalfFPLoadStoreMove
    : Predicate<"Subtarget->hasHalfFPLoadStoreMove()">,
      AssemblerPredicate<(any_of FeatureStdExtZfh, FeatureStdExtZfhmin,
                                 FeatureStdExtZfbfmin),
                         "'Zfh' (Half-Precision Floating-Point) or "
                                    "'Zfhmin' (Half-Precision Floating-Point Minimal) or "
                                    "'Zfbfmin' (Scalar BF16 Converts)">;

def FeatureStdExtZfa
    : YSXExtension<1, 0, "Additional Floating-Point", [YSXDisabledStdExtF]>,
      YSXExtensionBitmask<0, 34>;
def HasStdExtZfa : Predicate<"Subtarget->hasStdExtZfa()">,
                   AssemblerPredicate<(all_of FeatureStdExtZfa),
                                      "'Zfa' (Additional Floating-Point)">;
def NoStdExtZfa : Predicate<"!Subtarget->hasStdExtZfa()">;

def FeatureStdExtZfinx
    : YSXExtension<1, 0, "Float in Integer", [FeatureStdExtZicsr]>;
def HasStdExtZfinx : Predicate<"Subtarget->hasStdExtZfinx()">,
                     AssemblerPredicate<(all_of FeatureStdExtZfinx),
                                        "'Zfinx' (Float in Integer)">;
def HasStdExtFOrZfinx : Predicate<"Subtarget->hasStdExtFOrZfinx()">,
                        AssemblerPredicate<(any_of YSXDisabledStdExtF,
                                                   FeatureStdExtZfinx),
                            "'F' (Single-Precision Floating-Point) or "
                            "'Zfinx' (Float in Integer)">;

def FeatureStdExtZdinx
    : YSXExtension<1, 0, "Double in Integer", [FeatureStdExtZfinx]>;
def HasStdExtZdinx : Predicate<"Subtarget->hasStdExtZdinx()">,
                     AssemblerPredicate<(all_of FeatureStdExtZdinx),
                                        "'Zdinx' (Double in Integer)">;

def FeatureStdExtZhinxmin
    : YSXExtension<1, 0, "Half Float in Integer Minimal",
                     [FeatureStdExtZfinx]>;
def HasStdExtZhinxmin : Predicate<"Subtarget->hasStdExtZhinxmin()">,
                        AssemblerPredicate<(all_of FeatureStdExtZhinxmin),
                            "'Zhinx' (Half Float in Integer) or "
                            "'Zhinxmin' (Half Float in Integer Minimal)">;

def FeatureStdExtZhinx
    : YSXExtension<1, 0, "Half Float in Integer", [FeatureStdExtZhinxmin]>;
def HasStdExtZhinx : Predicate<"Subtarget->hasStdExtZhinx()">,
                     AssemblerPredicate<(all_of FeatureStdExtZhinx),
                                        "'Zhinx' (Half Float in Integer)">;
def NoStdExtZhinx : Predicate<"!Subtarget->hasStdExtZhinx()">;

// Compressed Extensions
def FeatureStdExtZca
    : YSXExtension<1, 0,
                     "part of the C extension, excluding compressed "
                     "floating point loads/stores">,
      YSXExtensionBitmask<1, 2>;
// Note: Zca conditionally implies C (see YSXISAInfo::updateImplication).
def HasStdExtZca
    : Predicate<"Subtarget->hasStdExtZca()">,
      AssemblerPredicate<(any_of FeatureStdExtZca),
                         "'C' (Compressed Instructions) or "
                         "'Zca' (part of the C extension, excluding "
                         "compressed floating point loads/stores)">;

def YSXDisabledStdExtC
    : YSXExtension<2, 0, "Compressed Instructions", [FeatureStdExtZca]>,
      YSXExtensionBitmask<0, 2>;
def HasStdExtC : Predicate<"Subtarget->hasStdExtC()">,
                 AssemblerPredicate<(all_of YSXDisabledStdExtC),
                                    "'C' (Compressed Instructions)">;

def FeatureStdExtZcb
    : YSXExtension<1, 0, "Compressed basic bit manipulation instructions",
                     [FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 3>;
def HasStdExtZcb : Predicate<"Subtarget->hasStdExtZcb()">,
                   AssemblerPredicate<(all_of FeatureStdExtZcb),
                       "'Zcb' (Compressed basic bit manipulation instructions)">;

def FeatureStdExtZcd
    : YSXExtension<1, 0,
                     "Compressed Double-Precision Floating-Point Instructions",
                     [FeatureStdExtD, FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 4>;

def HasStdExtCOrZcd
    : Predicate<"Subtarget->hasStdExtCOrZcd()">,
      AssemblerPredicate<(any_of YSXDisabledStdExtC, FeatureStdExtZcd),
                         "'C' (Compressed Instructions) or "
                         "'Zcd' (Compressed Double-Precision Floating-Point Instructions)">;

def FeatureStdExtZcf
    : YSXExtension<1, 0,
                     "Compressed Single-Precision Floating-Point Instructions",
                     [YSXDisabledStdExtF, FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 5>;

def FeatureStdExtZclsd
    : YSXExtension<1, 0,
                     "Compressed Load/Store Pair Instructions",
                     [FeatureStdExtZilsd, FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 9>;
def HasStdExtZclsd : Predicate<"Subtarget->hasStdExtZclsd()">,
                    AssemblerPredicate<(all_of FeatureStdExtZclsd),
                        "'Zclsd' (Compressed Load/Store pair instructions)">;

def FeatureStdExtZcmp
    : YSXExtension<1, 0,
                     "sequenced instructions for code-size reduction",
                     [FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 10>;
def HasStdExtZcmp : Predicate<"Subtarget->hasStdExtZcmp() && !Subtarget->hasStdExtC()">,
                    AssemblerPredicate<(all_of FeatureStdExtZcmp),
                        "'Zcmp' (sequenced instructions for code-size reduction)">;

def FeatureStdExtZcmt
    : YSXExtension<1, 0,
                     "table jump instructions for code-size reduction",
                     [FeatureStdExtZca, FeatureStdExtZicsr]>;
def HasStdExtZcmt : Predicate<"Subtarget->hasStdExtZcmt()">,
                           AssemblerPredicate<(all_of FeatureStdExtZcmt),
                           "'Zcmt' (table jump instructions for code-size reduction)">;

def FeatureStdExtZce
    : YSXExtension<1, 0,
                     "Compressed extensions for microcontrollers",
                     [FeatureStdExtZca, FeatureStdExtZcb, FeatureStdExtZcmp,
                      FeatureStdExtZcmt]>;

def HasStdExtCOrZcfOrZce
    : Predicate<"Subtarget->hasStdExtC() || Subtarget->hasStdExtZcf() ||"
                "Subtarget->hasStdExtZce()">,
      AssemblerPredicate<(any_of YSXDisabledStdExtC, FeatureStdExtZcf,
                                 FeatureStdExtZce),
                         "'C' (Compressed Instructions) or "
                         "'Zcf' (Compressed Single-Precision Floating-Point Instructions)">;

def FeatureStdExtZcmop
    : YSXExtension<1, 0, "Compressed May-Be-Operations",
                     [FeatureStdExtZca]>,
      YSXExtensionBitmask<1, 6>;
def HasStdExtZcmop : Predicate<"Subtarget->hasStdExtZcmop()">,
                     AssemblerPredicate<(all_of FeatureStdExtZcmop),
                         "'Zcmop' (Compressed May-Be-Operations)">;

// Bitmanip Extensions

def FeatureStdExtZba
    : YSXExtension<1, 0, "Address Generation Instructions">,
      YSXExtensionBitmask<0, 27>;
def HasStdExtZba : Predicate<"Subtarget->hasStdExtZba()">,
                   AssemblerPredicate<(all_of FeatureStdExtZba),
                                      "'Zba' (Address Generation Instructions)">;
def NoStdExtZba : Predicate<"!Subtarget->hasStdExtZba()">;

def FeatureStdExtZbb
    : YSXExtension<1, 0, "Basic Bit-Manipulation">,
      YSXExtensionBitmask<0, 28>;
def HasStdExtZbb : Predicate<"Subtarget->hasStdExtZbb()">,
                   AssemblerPredicate<(all_of FeatureStdExtZbb),
                                      "'Zbb' (Basic Bit-Manipulation)">;
def NoStdExtZbb : Predicate<"!Subtarget->hasStdExtZbb()">,
                  AssemblerPredicate<(all_of (not FeatureStdExtZbb))>;

def FeatureStdExtZbc
    : YSXExtension<1, 0, "Carry-Less Multiplication">,
      YSXExtensionBitmask<0, 29>;
def HasStdExtZbc : Predicate<"Subtarget->hasStdExtZbc()">,
                   AssemblerPredicate<(all_of FeatureStdExtZbc),
                                      "'Zbc' (Carry-Less Multiplication)">;

def FeatureStdExtZbs
    : YSXExtension<1, 0, "Single-Bit Instructions">,
      YSXExtensionBitmask<0, 33>;
def HasStdExtZbs : Predicate<"Subtarget->hasStdExtZbs()">,
                   AssemblerPredicate<(all_of FeatureStdExtZbs),
                                      "'Zbs' (Single-Bit Instructions)">;

// Bitmanip Extensions for Cryptography Extensions

def FeatureStdExtB
    : YSXExtension<1, 0, "the collection of the Zba, Zbb, Zbs extensions",
                     [FeatureStdExtZba, FeatureStdExtZbb, FeatureStdExtZbs]>,
      YSXExtensionBitmask<0, 1>;

def FeatureStdExtZbkb
    : YSXExtension<1, 0, "Bitmanip instructions for Cryptography">,
      YSXExtensionBitmask<0, 30>;
def HasStdExtZbkb : Predicate<"Subtarget->hasStdExtZbkb()">,
                    AssemblerPredicate<(all_of FeatureStdExtZbkb),
                        "'Zbkb' (Bitmanip instructions for Cryptography)">;
def NoStdExtZbkb : Predicate<"!Subtarget->hasStdExtZbkb()">,
                   AssemblerPredicate<(all_of (not FeatureStdExtZbkb))>;

def FeatureStdExtZbkx
    : YSXExtension<1, 0, "Crossbar permutation instructions">,
      YSXExtensionBitmask<0, 32>;
def HasStdExtZbkx : Predicate<"Subtarget->hasStdExtZbkx()">,
                    AssemblerPredicate<(all_of FeatureStdExtZbkx),
                        "'Zbkx' (Crossbar permutation instructions)">;

def HasStdExtZbbOrZbkb
    : Predicate<"Subtarget->hasStdExtZbb() || Subtarget->hasStdExtZbkb()">,
      AssemblerPredicate<(any_of FeatureStdExtZbb, FeatureStdExtZbkb),
                         "'Zbb' (Basic Bit-Manipulation) or "
                         "'Zbkb' (Bitmanip instructions for Cryptography)">;

// The Carry-less multiply subextension for cryptography is a subset of basic
// carry-less multiply subextension. The former should be enabled if the latter
// is enabled.
def FeatureStdExtZbkc
    : YSXExtension<1, 0,
                     "Carry-less multiply instructions for Cryptography">,
      YSXExtensionBitmask<0, 31>;
def HasStdExtZbkc
    : Predicate<"Subtarget->hasStdExtZbkc()">,
      AssemblerPredicate<(all_of FeatureStdExtZbkc),
          "'Zbkc' (Carry-less multiply instructions for Cryptography)">;

def HasStdExtZbcOrZbkc
    : Predicate<"Subtarget->hasStdExtZbc() || Subtarget->hasStdExtZbkc()">,
      AssemblerPredicate<(any_of FeatureStdExtZbc, FeatureStdExtZbkc),
                         "'Zbc' (Carry-Less Multiplication) or "
                         "'Zbkc' (Carry-less multiply instructions "
                         "for Cryptography)">;

// Cryptography Extensions

def FeatureStdExtZknd
    : YSXExtension<1, 0, "NIST Suite: AES Decryption">,
      YSXExtensionBitmask<0, 41>;
def HasStdExtZknd : Predicate<"Subtarget->hasStdExtZknd()">,
                    AssemblerPredicate<(all_of FeatureStdExtZknd),
                                       "'Zknd' (NIST Suite: AES Decryption)">;

def FeatureStdExtZkne
    : YSXExtension<1, 0, "NIST Suite: AES Encryption">,
      YSXExtensionBitmask<0, 42>;
def HasStdExtZkne : Predicate<"Subtarget->hasStdExtZkne()">,
                    AssemblerPredicate<(all_of FeatureStdExtZkne),
                                       "'Zkne' (NIST Suite: AES Encryption)">;

// Some instructions belong to both Zknd and Zkne subextensions.
// They should be enabled if either has been specified.
def HasStdExtZkndOrZkne
    : Predicate<"Subtarget->hasStdExtZknd() || Subtarget->hasStdExtZkne()">,
      AssemblerPredicate<(any_of FeatureStdExtZknd, FeatureStdExtZkne),
                         "'Zknd' (NIST Suite: AES Decryption) or "
                         "'Zkne' (NIST Suite: AES Encryption)">;

def FeatureStdExtZknh
    : YSXExtension<1, 0, "NIST Suite: Hash Function Instructions">,
      YSXExtensionBitmask<0, 43>;
def HasStdExtZknh : Predicate<"Subtarget->hasStdExtZknh()">,
                    AssemblerPredicate<(all_of FeatureStdExtZknh),
                        "'Zknh' (NIST Suite: Hash Function Instructions)">;

def FeatureStdExtZksed
    : YSXExtension<1, 0, "ShangMi Suite: SM4 Block Cipher Instructions">,
      YSXExtensionBitmask<0, 44>;
def HasStdExtZksed : Predicate<"Subtarget->hasStdExtZksed()">,
                     AssemblerPredicate<(all_of FeatureStdExtZksed),
                         "'Zksed' (ShangMi Suite: SM4 Block Cipher Instructions)">;

def FeatureStdExtZksh
    : YSXExtension<1, 0, "ShangMi Suite: SM3 Hash Function Instructions">,
      YSXExtensionBitmask<0, 45>;
def HasStdExtZksh : Predicate<"Subtarget->hasStdExtZksh()">,
                    AssemblerPredicate<(all_of FeatureStdExtZksh),
                        "'Zksh' (ShangMi Suite: SM3 Hash Function Instructions)">;

def FeatureStdExtZkr
    : YSXExtension<1, 0, "Entropy Source Extension">;
def HasStdExtZkr : Predicate<"Subtarget->hasStdExtZkr()">,
                   AssemblerPredicate<(all_of FeatureStdExtZkr),
                                      "'Zkr' (Entropy Source Extension)">;

def FeatureStdExtZkn
    : YSXExtension<1, 0, "NIST Algorithm Suite",
                     [FeatureStdExtZbkb,
                      FeatureStdExtZbkc,
                      FeatureStdExtZbkx,
                      FeatureStdExtZkne,
                      FeatureStdExtZknd,
                      FeatureStdExtZknh]>;

def FeatureStdExtZks
    : YSXExtension<1, 0, "ShangMi Algorithm Suite",
                     [FeatureStdExtZbkb,
                      FeatureStdExtZbkc,
                      FeatureStdExtZbkx,
                      FeatureStdExtZksed,
                      FeatureStdExtZksh]>;

def FeatureStdExtZkt
    : YSXExtension<1, 0, "Data Independent Execution Latency">,
      YSXExtensionBitmask<0, 46>;

def FeatureStdExtZk
    : YSXExtension<1, 0, "Standard scalar cryptography extension",
                     [FeatureStdExtZkn,
                      FeatureStdExtZkr,
                      FeatureStdExtZkt]>;

// Vector Extensions

def FeatureStdExtZvl32b : YSXExtension<1, 0,
                                         "Minimum Vector Length 32", [],
                                         "ZvlLen", "32">;

foreach i = { 6-16 } in {
  defvar I = !shl(1, i);
  def FeatureStdExtZvl#I#b :
      YSXExtension<1, 0, "Minimum Vector Length "#I,
                     [!cast<YSXExtension>("FeatureStdExtZvl"#!srl(I, 1)#"b")],
                     "ZvlLen", !cast<string>(I)>;
}

def FeatureStdExtZve32x
    : YSXExtension<1, 0,
                     "Vector Extensions for Embedded Processors "
                     "with maximal 32 EEW",
                     [FeatureStdExtZicsr, FeatureStdExtZvl32b]>,
      YSXExtensionBitmask<0, 60>;


def FeatureStdExtZve32f
    : YSXExtension<1, 0,
                     "Vector Extensions for Embedded Processors "
                     "with maximal 32 EEW and F extension",
                     [FeatureStdExtZve32x, YSXDisabledStdExtF]>,
      YSXExtensionBitmask<0, 61>;

def FeatureStdExtZve64x
    : YSXExtension<1, 0,
                     "Vector Extensions for Embedded Processors "
                     "with maximal 64 EEW",
                     [FeatureStdExtZve32x, FeatureStdExtZvl64b]>,
      YSXExtensionBitmask<0, 62>;

def FeatureStdExtZve64f
    : YSXExtension<1, 0,
                     "Vector Extensions for Embedded Processors "
                     "with maximal 64 EEW and F extension",
                     [FeatureStdExtZve32f, FeatureStdExtZve64x]>,
      YSXExtensionBitmask<0, 63>;

def FeatureStdExtZve64d
    : YSXExtension<1, 0,
                     "Vector Extensions for Embedded Processors "
                     "with maximal 64 EEW, F and D extension",
                     [FeatureStdExtZve64f, FeatureStdExtD]>,
      YSXExtensionBitmask<1, 0>;

def YSXDisabledStdExtV
    : YSXExtension<1, 0,
                     "Vector Extension for Application Processors",
                     [FeatureStdExtZvl128b, FeatureStdExtZve64d]>,
      YSXExtensionBitmask<0, 21>;

def FeatureStdExtZvfbfa
    : YSXExperimentalExtension<0, 1, "Additional BF16 vector compute support",
                                 [FeatureStdExtZve32f, FeatureStdExtZfbfmin]>;
def HasStdExtZvfbfa : Predicate<"Subtarget->hasStdExtZvfbfa()">,
                      AssemblerPredicate<(all_of FeatureStdExtZvfbfa),
                          "'Zvfbfa' (Additional BF16 vector compute support)">;

def FeatureStdExtZvfbfmin
    : YSXExtension<1, 0, "Vector BF16 Converts", [FeatureStdExtZve32f]>;
def HasStdExtZvfbfmin : Predicate<"Subtarget->hasStdExtZvfbfmin()">,
                        AssemblerPredicate<(all_of FeatureStdExtZvfbfmin),
                            "'Zvfbfmin' (Vector BF16 Converts)">;

def FeatureStdExtZvfbfwma
    : YSXExtension<1, 0, "Vector BF16 widening mul-add",
                     [FeatureStdExtZvfbfmin, FeatureStdExtZfbfmin]>;
def HasStdExtZvfbfwma : Predicate<"Subtarget->hasStdExtZvfbfwma()">,
                        AssemblerPredicate<(all_of FeatureStdExtZvfbfwma),
                            "'Zvfbfwma' (Vector BF16 widening mul-add)">;

def FeatureStdExtZvfhmin
    : YSXExtension<1, 0,
                     "Vector Half-Precision Floating-Point Minimal",
                     [FeatureStdExtZve32f]>,
      YSXExtensionBitmask<0, 51>;

def FeatureStdExtZvfh
    : YSXExtension<1, 0, "Vector Half-Precision Floating-Point",
                     [FeatureStdExtZvfhmin, FeatureStdExtZfhmin]>,
      YSXExtensionBitmask<0, 50>;

def HasStdExtZfhOrZvfh
    : Predicate<"Subtarget->hasStdExtZfh() || Subtarget->hasStdExtZvfh()">,
      AssemblerPredicate<(any_of FeatureStdExtZfh, FeatureStdExtZvfh),
                         "'Zfh' (Half-Precision Floating-Point) or "
                         "'Zvfh' (Vector Half-Precision Floating-Point)">;

def FeatureStdExtZvfofp8min
    : YSXExperimentalExtension<0, 2,
                                 "Vector OFP8 Converts", [FeatureStdExtZve32f]>;
def HasStdExtZvfofp8min
    : Predicate<"Subtarget->hasStdExtZvfofp8min()">,
      AssemblerPredicate<(all_of FeatureStdExtZvfofp8min),
                         "'Zvfofp8min' (Vector OFP8 Converts)">;

def HasStdExtZvfbfminOrZvfofp8min
    : Predicate<"Subtarget->hasStdExtZvfbfmin() ||"
                "Subtarget->hasStdExtZvfofp8min()">,
      AssemblerPredicate<(any_of FeatureStdExtZvfbfmin,
                             FeatureStdExtZvfofp8min),
                         "'Zvfbfmin' (Vector BF16 Converts) or "
                         "'Zvfofp8min' (Vector OFP8 Converts)">;

// Vector Cryptography and Bitmanip Extensions

def FeatureStdExtZvkb
    : YSXExtension<1, 0, "Vector Bit-manipulation used in Cryptography",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 52>;
def HasStdExtZvkb : Predicate<"Subtarget->hasStdExtZvkb()">,
                    AssemblerPredicate<(all_of FeatureStdExtZvkb),
                        "'Zvkb' (Vector Bit-manipulation used in Cryptography)">;

def FeatureStdExtZvbb
    : YSXExtension<1, 0, "Vector basic bit-manipulation instructions",
                     [FeatureStdExtZvkb]>,
      YSXExtensionBitmask<0, 48>;
def HasStdExtZvbb : Predicate<"Subtarget->hasStdExtZvbb()">,
                    AssemblerPredicate<(all_of FeatureStdExtZvbb),
                        "'Zvbb' (Vector basic bit-manipulation instructions)">;

def FeatureStdExtZvbc
    : YSXExtension<1, 0, "Vector Carryless Multiplication",
                     [FeatureStdExtZve64x]>,
      YSXExtensionBitmask<0, 49>;
def HasStdExtZvbc : Predicate<"Subtarget->hasStdExtZvbc()">,
                    AssemblerPredicate<(all_of FeatureStdExtZvbc),
                        "'Zvbc' (Vector Carryless Multiplication)">;

def FeatureStdExtZvbc32e
    : YSXExperimentalExtension<0, 7,
                                 "Vector Carryless Multiplication with 32-bits elements",
                                 [FeatureStdExtZve32x]>;

def HasStdExtZvbcOrZvbc32e : Predicate<"Subtarget->hasStdExtZvbc() || Subtarget->hasStdExtZvbc32e()">,
                             AssemblerPredicate<(any_of FeatureStdExtZvbc, FeatureStdExtZvbc32e),
                             "'Zvbc' or 'Zvbc32e' (Vector Carryless Multiplication)">;

def FeatureStdExtZvkg
    : YSXExtension<1, 0, "Vector GCM instructions for Cryptography",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 53>;
def HasStdExtZvkg : Predicate<"Subtarget->hasStdExtZvkg()">,
                    AssemblerPredicate<(all_of FeatureStdExtZvkg),
                        "'Zvkg' (Vector GCM instructions for Cryptography)">;

def FeatureStdExtZvkgs
    : YSXExperimentalExtension<0, 7,
                                 "Vector-Scalar GCM instructions for Cryptography",
                                 [FeatureStdExtZvkg]>;
def HasStdExtZvkgs : Predicate<"Subtarget->hasStdExtZvkgs()">,
                     AssemblerPredicate<(all_of FeatureStdExtZvkgs),
                        "'Zvkgs' (Vector-Scalar GCM instructions for Cryptography)">;

def FeatureStdExtZvkned
    : YSXExtension<1, 0,
                     "Vector AES Encryption & Decryption (Single Round)",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 54>;
def HasStdExtZvkned : Predicate<"Subtarget->hasStdExtZvkned()">,
                      AssemblerPredicate<(all_of FeatureStdExtZvkned),
                          "'Zvkned' (Vector AES Encryption & Decryption (Single Round))">;

def FeatureStdExtZvknha
    : YSXExtension<1, 0, "Vector SHA-2 (SHA-256 only)",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 55>;
def HasStdExtZvknha : Predicate<"Subtarget->hasStdExtZvknha()">,
                      AssemblerPredicate<(all_of FeatureStdExtZvknha),
                          "'Zvknha' (Vector SHA-2 (SHA-256 only))">;

def FeatureStdExtZvknhb
    : YSXExtension<1, 0, "Vector SHA-2 (SHA-256 and SHA-512)",
                     [FeatureStdExtZve64x]>,
      YSXExtensionBitmask<0, 56>;
def HasStdExtZvknhb : Predicate<"Subtarget->hasStdExtZvknhb()">,
                      AssemblerPredicate<(all_of FeatureStdExtZvknhb),
                          "'Zvknhb' (Vector SHA-2 (SHA-256 and SHA-512))">;

def HasStdExtZvknhaOrZvknhb : Predicate<"Subtarget->hasStdExtZvknha() || Subtarget->hasStdExtZvknhb()">,
                              AssemblerPredicate<(any_of FeatureStdExtZvknha, FeatureStdExtZvknhb),
                                  "'Zvknha' or 'Zvknhb' (Vector SHA-2)">;

def FeatureStdExtZvksed
    : YSXExtension<1, 0, "SM4 Block Cipher Instructions",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 57>;
def HasStdExtZvksed : Predicate<"Subtarget->hasStdExtZvksed()">,
                      AssemblerPredicate<(all_of FeatureStdExtZvksed),
                          "'Zvksed' (SM4 Block Cipher Instructions)">;

def FeatureStdExtZvksh
    : YSXExtension<1, 0, "SM3 Hash Function Instructions",
                     [FeatureStdExtZve32x]>,
      YSXExtensionBitmask<0, 58>;
def HasStdExtZvksh : Predicate<"Subtarget->hasStdExtZvksh()">,
                     AssemblerPredicate<(all_of FeatureStdExtZvksh),
                         "'Zvksh' (SM3 Hash Function Instructions)">;

def FeatureStdExtZvkt
    : YSXExtension<1, 0, "Vector Data-Independent Execution Latency">,
      YSXExtensionBitmask<0, 59>;

// Zvk short-hand extensions

def FeatureStdExtZvkn
    : YSXExtension<1, 0,
                     "shorthand for 'Zvkned', 'Zvknhb', 'Zvkb', and 'Zvkt'",
                     [FeatureStdExtZvkned, FeatureStdExtZvknhb,
                      FeatureStdExtZvkb, FeatureStdExtZvkt]>;

def FeatureStdExtZvknc
    : YSXExtension<1, 0, "shorthand for 'Zvknc' and 'Zvbc'",
                     [FeatureStdExtZvkn, FeatureStdExtZvbc]>;

def FeatureStdExtZvkng
    : YSXExtension<1, 0, "shorthand for 'Zvkn' and 'Zvkg'",
                     [FeatureStdExtZvkn, FeatureStdExtZvkg]>;

def FeatureStdExtZvks
    : YSXExtension<1, 0,
                     "shorthand for 'Zvksed', 'Zvksh', 'Zvkb', and 'Zvkt'",
                     [FeatureStdExtZvksed, FeatureStdExtZvksh,
                      FeatureStdExtZvkb, FeatureStdExtZvkt]>;

def FeatureStdExtZvksc
    : YSXExtension<1, 0, "shorthand for 'Zvks' and 'Zvbc'",
                     [FeatureStdExtZvks, FeatureStdExtZvbc]>;

def FeatureStdExtZvksg
    : YSXExtension<1, 0, "shorthand for 'Zvks' and 'Zvkg'",
                     [FeatureStdExtZvks, FeatureStdExtZvkg]>;

// Vector quad widening dot product

def FeatureStdExtZvqdotq
    : YSXExperimentalExtension<0, 0, "Vector quad widening 4D Dot Product",
                                 [FeatureStdExtZve32x]>;
def HasStdExtZvqdotq : Predicate<"Subtarget->hasStdExtZvqdotq()">,
                       AssemblerPredicate<(all_of FeatureStdExtZvqdotq),
                           "'Zvqdotq' (Vector quad widening 4D Dot Product)">;

// Vector instruction predicates

def HasVInstructions    : Predicate<"Subtarget->hasVInstructions()">,
      AssemblerPredicate<
          (any_of FeatureStdExtZve32x),
          "'V' (Vector Extension for Application Processors), 'Zve32x' "
          "(Vector Extensions for Embedded Processors)">;
def HasVInstructionsI64 : Predicate<"Subtarget->hasVInstructionsI64()">,
      AssemblerPredicate<
          (any_of FeatureStdExtZve64x),
          "'V' (Vector Extension for Application Processors) or 'Zve64x' "
          "(Vector Extensions for Embedded Processors)">;
def HasVInstructionsAnyF : Predicate<"Subtarget->hasVInstructionsAnyF()">,
      AssemblerPredicate<
          (any_of FeatureStdExtZve32f),
          "'V' (Vector Extension for Application Processors), 'Zve32f' "
          "(Vector Extensions for Embedded Processors)">;

def HasVInstructionsF16Minimal : Predicate<"Subtarget->hasVInstructionsF16Minimal()">,
      AssemblerPredicate<(any_of FeatureStdExtZvfhmin, FeatureStdExtZvfh),
                         "'Zvfhmin' (Vector Half-Precision Floating-Point Minimal) or "
                         "'Zvfh' (Vector Half-Precision Floating-Point)">;

def HasVInstructionsBF16Minimal : Predicate<"Subtarget->hasVInstructionsBF16Minimal()">;
def HasVInstructionsF16 : Predicate<"Subtarget->hasVInstructionsF16()">;
def HasVInstructionsBF16 : Predicate<"Subtarget->hasVInstructionsBF16()">;
def HasVInstructionsF64 : Predicate<"Subtarget->hasVInstructionsF64()">;

def HasVInstructionsFullMultiply : Predicate<"Subtarget->hasVInstructionsFullMultiply()">;

// Hypervisor Extensions

def FeatureStdExtH : YSXExtension<1, 0, "Hypervisor">,
                     YSXExtensionBitmask<0, 7>;
def HasStdExtH : Predicate<"Subtarget->hasStdExtH()">,
                 AssemblerPredicate<(all_of FeatureStdExtH),
                                    "'H' (Hypervisor)">;

// Supervisor extensions

def FeatureStdExtSdext : YSXExtension<1, 0, "External debugger">;

def FeatureStdExtSdtrig : YSXExtension<1, 0, "Debugger triggers">;

def FeatureStdExtShgatpa
    : YSXExtension<1, 0,
                     "SvNNx4 mode supported for all modes supported by satp, as well as Bare">;
def FeatureStdExtShvsatpa
    : YSXExtension<1, 0,
                     "vsatp supports all modes supported by satp">;

def FeatureStdExtShlcofideleg
    : YSXExtension<1, 0,
                     "Delegating LCOFI Interrupts to VS-mode">;

def FeatureStdExtSmaia
    : YSXExtension<1, 0,
                     "Advanced Interrupt Architecture Machine Level">;
def FeatureStdExtSsaia
    : YSXExtension<1, 0,
                     "Advanced Interrupt Architecture Supervisor Level">;

def FeatureStdExtSmcntrpmf
    : YSXExtension<1, 0, "Cycle and Instret Privilege Mode Filtering">;

def FeatureStdExtSmcsrind
    : YSXExtension<1, 0, "Indirect CSR Access Machine Level">;
def FeatureStdExtSscsrind
    : YSXExtension<1, 0, "Indirect CSR Access Supervisor Level">;

def FeatureStdExtSmdbltrp
    : YSXExtension<1, 0, "Double Trap Machine Level", [FeatureStdExtZicsr]>;
def FeatureStdExtSsdbltrp
    : YSXExtension<1, 0, "Double Trap Supervisor Level", [FeatureStdExtZicsr]>;

def FeatureStdExtSmepmp
    : YSXExtension<1, 0, "Enhanced Physical Memory Protection">;

def FeatureStdExtSmpmpmt
    : YSXExperimentalExtension<0, 6, "PMP-based Memory Types Extension">;

def FeatureStdExtSmrnmi
    : YSXExtension<1, 0, "Resumable Non-Maskable Interrupts">;
def HasStdExtSmrnmi : Predicate<"Subtarget->hasStdExtSmrnmi()">,
                       AssemblerPredicate<(all_of FeatureStdExtSmrnmi),
                           "'Smrnmi' (Resumable Non-Maskable Interrupts)">;

def FeatureStdExtSmcdeleg
    : YSXExtension<1, 0, "Counter Delegation Machine Level">;
def FeatureStdExtSsccfg
    : YSXExtension<1, 0, "Counter Configuration Supervisor Level">;

def FeatureStdExtSsccptr
    : YSXExtension<1, 0, "Main memory supports page table reads">;

def FeatureStdExtSscofpmf
    : YSXExtension<1, 0, "Count Overflow and Mode-Based Filtering">;

def FeatureStdExtShcounterenw
    : YSXExtension<1, 0,
                     "Support writeable hcounteren enable "
                     "bit for any hpmcounter that is not read-only zero">;
def FeatureStdExtSscounterenw
    : YSXExtension<1, 0,
                     "Support writeable scounteren enable "
                     "bit for any hpmcounter that is not read-only zero">;

def FeatureStdExtSmstateen
    : YSXExtension<1, 0,
                     "Machine-mode view of the state-enable extension">;
def FeatureStdExtSsstateen
    : YSXExtension<1, 0,
                     "Supervisor-mode view of the state-enable extension">;

def FeatureStdExtSsstrict
    : YSXExtension<1, 0,
                     "No non-conforming extensions are present">;

def FeatureStdExtSstc
    : YSXExtension<1, 0, "Supervisor-mode timer interrupts">;

def FeatureStdExtSsqosid
    : YSXExtension<1, 0, "Quality-of-Service (QoS) Identifiers">;

def FeatureStdExtShtvala
    : YSXExtension<1, 0, "htval provides all needed values">;
def FeatureStdExtShvstvala
    : YSXExtension<1, 0, "vstval provides all needed values">;
def FeatureStdExtSstvala
    : YSXExtension<1, 0, "stval provides all needed values">;

def FeatureStdExtShvstvecd
    : YSXExtension<1, 0, "vstvec supports Direct mode">;
def FeatureStdExtSstvecd
    : YSXExtension<1, 0, "stvec supports Direct mode">;

def FeatureStdExtSsu64xl
    : YSXExtension<1, 0, "UXLEN=64 supported">;

def FeatureStdExtSvade
    : YSXExtension<1, 0, "Raise exceptions on improper A/D bits">;

def FeatureStdExtSvadu
    : YSXExtension<1, 0, "Hardware A/D updates">;

def FeatureStdExtSvbare
    : YSXExtension<1, 0, "satp mode Bare supported">;

def FeatureStdExtSvinval
    : YSXExtension<1, 0,
                     "Fine-Grained Address-Translation Cache Invalidation">;
def HasStdExtSvinval : Predicate<"Subtarget->hasStdExtSvinval()">,
                       AssemblerPredicate<(all_of FeatureStdExtSvinval),
                           "'Svinval' (Fine-Grained Address-Translation Cache Invalidation)">;

def FeatureStdExtSvnapot
    : YSXExtension<1, 0, "NAPOT Translation Contiguity">;

def FeatureStdExtSvpbmt
    : YSXExtension<1, 0, "Page-Based Memory Types">;

def FeatureStdExtSvvptc
    : YSXExtension<1, 0,
                     "Obviating Memory-Management Instructions after Marking PTEs Valid">;

def FeatureStdExtSha
    : YSXExtension<1, 0, "Augmented Hypervisor",
                     [FeatureStdExtH, FeatureStdExtSsstateen, FeatureStdExtShcounterenw,
                      FeatureStdExtShvstvala, FeatureStdExtShtvala, FeatureStdExtShvstvecd,
                      FeatureStdExtShvsatpa, FeatureStdExtShgatpa]>;

def FeatureStdExtSvukte
    : YSXExperimentalExtension<0, 3,
                                 "Address-Independent Latency of User-Mode Faults to Supervisor Addresses">;

// Pointer Masking extensions

// A supervisor-level extension that provides pointer masking for the next lower
// privilege mode (U-mode), and for VS- and VU-modes if the H extension is
// present.
def FeatureStdExtSsnpm
    : YSXExtension<1, 0,
                     "Supervisor-level Pointer Masking for next lower privilege mode">;

// A machine-level extension that provides pointer masking for the next lower
// privilege mode (S/HS if S-mode is implemented, or U-mode otherwise).
def FeatureStdExtSmnpm
    : YSXExtension<1, 0,
                     "Machine-level Pointer Masking for next lower privilege mode">;

// A machine-level extension that provides pointer masking for M-mode.
def FeatureStdExtSmmpm
    : YSXExtension<1, 0,
                     "Machine-level Pointer Masking for M-mode">;

// An extension that indicates that there is pointer-masking support available
// in supervisor mode, with some facility provided in the supervisor execution
// environment to control pointer masking.
def FeatureStdExtSspm
    : YSXExtension<1, 0,
                     "Indicates Supervisor-mode Pointer Masking">;

// An extension that indicates that there is pointer-masking support available
// in user mode, with some facility provided in the application execution
// environment to control pointer masking.
def FeatureStdExtSupm
    : YSXExtension<1, 0,
                     "Indicates User-mode Pointer Masking">;

def FeatureStdExtSmctr
    : YSXExtension<1, 0,
                     "Control Transfer Records Machine Level",
                     [FeatureStdExtSscsrind]>;
def FeatureStdExtSsctr
    : YSXExtension<1, 0,
                     "Control Transfer Records Supervisor Level",
                     [FeatureStdExtSscsrind]>;
def HasStdExtSmctrOrSsctr : Predicate<"Subtarget->hasStdExtSmctrOrSsctr()">,
                            AssemblerPredicate<(any_of FeatureStdExtSmctr, FeatureStdExtSsctr),
                               "'Smctr' (Control Transfer Records Machine Level) or "
                               "'Ssctr' (Control Transfer Records Supervisor Level)">;

// Packed SIMD Extensions
def FeatureStdExtP
    : YSXExperimentalExtension<0, 18,
                                 "'Base P' (Packed SIMD)">;
def HasStdExtP : Predicate<"Subtarget->hasStdExtP()">,
                 AssemblerPredicate<(all_of FeatureStdExtP),
                                    "'Base P' (Packed SIMD)">;

def HasStdExtZbkbOrP
    : Predicate<"Subtarget->hasStdExtZbkb() || Subtarget->hasStdExtP()">,
      AssemblerPredicate<(any_of FeatureStdExtZbkb, FeatureStdExtP),
                         "'Zbkb' (Bitmanip instructions for Cryptography) or "
                         "'Base P' (Packed-SIMD)">;

//===----------------------------------------------------------------------===//
// Vendor extensions
//===----------------------------------------------------------------------===//

// Ventana Extensions

def YSXDisabledVendorFeatureXVentanaCondOps
    : YSXExtension<1, 0, "Ventana Conditional Ops">;
def HasVendorXVentanaCondOps : Predicate<"Subtarget->hasVendorXVentanaCondOps()">,
                               AssemblerPredicate<(all_of YSXDisabledVendorFeatureXVentanaCondOps),
                                   "'XVentanaCondOps' (Ventana Conditional Ops)">;

// T-Head Extensions

def YSXDisabledVendorFeatureXRemovedTHeadBa
    : YSXExtension<1, 0,
                     "T-Head address calculation instructions">;
def HasVendorXRemovedTHeadBa : Predicate<"Subtarget->hasVendorXRemovedTHeadBa()">,
                        AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadBa),
                            "'XRemovedTHeadBa' (T-Head address calculation instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadBb
    : YSXExtension<1, 0,
                     "T-Head basic bit-manipulation instructions">;
def HasVendorXRemovedTHeadBb : Predicate<"Subtarget->hasVendorXRemovedTHeadBb()">,
                        AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadBb),
                            "'XRemovedTHeadBb' (T-Head basic bit-manipulation instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadBs
    : YSXExtension<1, 0,
                     "T-Head single-bit instructions">;
def HasVendorXRemovedTHeadBs : Predicate<"Subtarget->hasVendorXRemovedTHeadBs()">,
                        AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadBs),
                            "'XRemovedTHeadBs' (T-Head single-bit instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadCondMov
    : YSXExtension<1, 0,
                     "T-Head conditional move instructions">;
def HasVendorXRemovedTHeadCondMov : Predicate<"Subtarget->hasVendorXRemovedTHeadCondMov()">,
                             AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadCondMov),
                                 "'XRemovedTHeadCondMov' (T-Head conditional move instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadCmo
    : YSXExtension<1, 0,
                     "T-Head cache management instructions">;
def HasVendorXRemovedTHeadCmo : Predicate<"Subtarget->hasVendorXRemovedTHeadCmo()">,
                         AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadCmo),
                             "'XRemovedTHeadCmo' (T-Head cache management instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadFMemIdx
    : YSXExtension<1, 0,
                     "T-Head FP Indexed Memory Operations">;
def HasVendorXRemovedTHeadFMemIdx : Predicate<"Subtarget->hasVendorXRemovedTHeadFMemIdx()">,
                             AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadFMemIdx),
                                 "'XRemovedTHeadFMemIdx' (T-Head FP Indexed Memory Operations)">;

def YSXDisabledVendorFeatureXRemovedTHeadMac
    : YSXExtension<1, 0,
                     "T-Head Multiply-Accumulate Instructions">;
def HasVendorXRemovedTHeadMac : Predicate<"Subtarget->hasVendorXRemovedTHeadMac()">,
                         AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadMac),
                             "'XRemovedTHeadMac' (T-Head Multiply-Accumulate Instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadMemIdx
    : YSXExtension<1, 0,
                     "T-Head Indexed Memory Operations">;
def HasVendorXRemovedTHeadMemIdx : Predicate<"Subtarget->hasVendorXRemovedTHeadMemIdx()">,
                            AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadMemIdx),
                                "'XRemovedTHeadMemIdx' (T-Head Indexed Memory Operations)">;

def YSXDisabledVendorFeatureXRemovedTHeadMemPair
    : YSXExtension<1, 0,
                     "T-Head two-GPR Memory Operations">;
def HasVendorXRemovedTHeadMemPair : Predicate<"Subtarget->hasVendorXRemovedTHeadMemPair()">,
                             AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadMemPair),
                                 "'XRemovedTHeadMemPair' (T-Head two-GPR Memory Operations)">;

def YSXDisabledVendorFeatureXRemovedTHeadSync
    : YSXExtension<1, 0,
                     "T-Head multicore synchronization instructions">;
def HasVendorXRemovedTHeadSync : Predicate<"Subtarget->hasVendorXRemovedTHeadSync()">,
                          AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadSync),
                              "'XRemovedTHeadSync' (T-Head multicore synchronization instructions)">;

def YSXDisabledVendorFeatureXRemovedTHeadVdot
    : YSXExtension<1, 0,
                     "T-Head Vector Extensions for Dot",
                     [YSXDisabledStdExtV]>;
def HasVendorXRemovedTHeadVdot : Predicate<"Subtarget->hasVendorXRemovedTHeadVdot()">,
                          AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedTHeadVdot),
                              "'XRemovedTHeadVdot' (T-Head Vector Extensions for Dot)">;

// SiFive Extensions

def YSXDisabledVendorFeatureXRemovedSfvcp
    : YSXExtension<1, 0,
                     "SiFive Custom Vector Coprocessor Interface Instructions",
                     [FeatureStdExtZve32x]>;
def HasVendorXRemovedSfvcp : Predicate<"Subtarget->hasVendorXRemovedSfvcp()">,
                      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvcp),
                          "'XRemovedSfvcp' (SiFive Custom Vector Coprocessor Interface Instructions)">;

def YSXDisabledVendorFeatureXRemovedSfmmbase
    : YSXExtension<0, 6,
                     "All non arithmetic instructions for all TEWs and sf.vtzero",
                     [FeatureStdExtZve32x]>;
def HasVendorXRemovedSfmmbase : Predicate<"Subtarget->hasVendorXRemovedSfmmbase()">,
                         AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfmmbase),
                             "'XRemovedSfmmbase' (All non arithmetic instructions for all TEWs and sf.vtzero)">;

def YSXDisabledVendorFeatureXRemovedSfmm32a8f
    : YSXExtension<0, 6,
                     "TEW=32-bit accumulation, operands - float: fp8",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZve32f]>;
def HasVendorXRemovedSfmm32a8f : Predicate<"Subtarget->hasVendorXRemovedSfmm32a8f()">,
                          AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfmm32a8f),
                              "'XRemovedSfmm32a8f' (TEW=32-bit accumulation, operands - float: fp8)">;

def YSXDisabledVendorFeatureXRemovedSfmm32a16f
    : YSXExtension<0, 6,
                     "TEW=32-bit accumulation, operands - float: 16b, widen=2 (IEEE, BF)",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZve32f]>;

def YSXDisabledVendorFeatureXRemovedSfmm32a32f
    : YSXExtension<0, 6,
                     "TEW=32-bit accumulation, operands - float: 32b",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZve32f]>;

def YSXDisabledVendorFeatureXRemovedSfmm32a8i
    : YSXExtension<0, 6,
                     "TEW=32-bit accumulation, operands - int: 8b",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase]>;
def HasVendorXRemovedSfmm32a8i : Predicate<"Subtarget->hasVendorXRemovedSfmm32a8i()">,
                          AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfmm32a8i),
                              "'XRemovedSfmm32a8i' (TEW=32-bit accumulation, operands - int: 8b)">;

def YSXDisabledVendorFeatureXRemovedSfmm64a64f
    : YSXExtension<0, 6,
                     "TEW=64-bit accumulation, operands - float: fp64",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZve64d]>;
def HasVendorXRemovedSfmm32a16fOrXRemovedSfmm32a32fOrXRemovedSfmm64a64f
    : Predicate<"Subtarget->hasVendorXRemovedSfmm32a16fOrXRemovedSfmm32a32fOrXRemovedSfmm64a64f()">,
                AssemblerPredicate<(any_of YSXDisabledVendorFeatureXRemovedSfmm32a16f,
                                           YSXDisabledVendorFeatureXRemovedSfmm32a32f,
                                           YSXDisabledVendorFeatureXRemovedSfmm64a64f),
                    "'XRemovedSfmm32a16f' (TEW=32-bit accumulation, operands - float: 16b, widen=2 (IEEE, BF)), or "
                    "'XRemovedSfmm32a32f' (TEW=32-bit accumulation, operands - float: 32b), or "
                    "'XRemovedSfmm64a64f' (TEW=64-bit accumulation, operands - float: fp64)">;

def YSXDisabledVendorFeatureXRemovedSfmm16t
    : YSXExtension<0, 6,
                     "TE=16 configuration",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZvl64b], "XRemovedSfmmTE", "16">;

def YSXDisabledVendorFeatureXRemovedSfmm32t
    : YSXExtension<0, 6,
                     "TE=32 configuration",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZvl128b], "XRemovedSfmmTE", "32">;

def YSXDisabledVendorFeatureXRemovedSfmm64t
    : YSXExtension<0, 6,
                     "TE=64 configuration",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZvl256b], "XRemovedSfmmTE", "64">;

def YSXDisabledVendorFeatureXRemovedSfmm128t
    : YSXExtension<0, 6,
                     "TE=128 configuration",
                     [YSXDisabledVendorFeatureXRemovedSfmmbase, FeatureStdExtZvl512b], "XRemovedSfmmTE", "128">;

def YSXDisabledVendorFeatureXRemovedSfvqmaccdod
    : YSXExtension<1, 0,
                     "SiFive Int8 Matrix Multiplication Instructions (2-by-8 and 8-by-2)",
                     [FeatureStdExtZve32x, FeatureStdExtZvl128b]>;
def HasVendorXRemovedSfvqmaccdod
    : Predicate<"Subtarget->hasVendorXRemovedSfvqmaccdod()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvqmaccdod),
                         "'XRemovedSfvqmaccdod' (SiFive Int8 Matrix Multiplication Instructions (2-by-8 and 8-by-2))">;

def YSXDisabledVendorFeatureXRemovedSfvqmaccqoq
    : YSXExtension<1, 0,
                     "SiFive Int8 Matrix Multiplication Instructions (4-by-8 and 8-by-4)",
                     [FeatureStdExtZve32x, FeatureStdExtZvl256b]>;
def HasVendorXRemovedSfvqmaccqoq
    : Predicate<"Subtarget->hasVendorXRemovedSfvqmaccqoq()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvqmaccqoq),
                         "'XRemovedSfvqmaccqoq' (SiFive Int8 Matrix Multiplication Instructions (4-by-8 and 8-by-4))">;

def YSXDisabledVendorFeatureXRemovedSfvfwmaccqqq
    : YSXExtension<1, 0,
                     "SiFive Matrix Multiply Accumulate Instruction (4-by-4)",
                     [FeatureStdExtZvfbfmin, FeatureStdExtZvl128b]>;
def HasVendorXRemovedSfvfwmaccqqq
    : Predicate<"Subtarget->hasVendorXRemovedSfvfwmaccqqq()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvfwmaccqqq),
                         "'XRemovedSfvfwmaccqqq' (SiFive Matrix Multiply Accumulate Instruction (4-by-4))">;

def YSXDisabledVendorFeatureXRemovedSfvfnrclipxfqf
    : YSXExtension<1, 0,
                     "SiFive FP32-to-int8 Ranged Clip Instructions",
                     [FeatureStdExtZve32f]>;
def HasVendorXRemovedSfvfnrclipxfqf
    : Predicate<"Subtarget->hasVendorXRemovedSfvfnrclipxfqf()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvfnrclipxfqf),
                         "'XRemovedSfvfnrclipxfqf' (SiFive FP32-to-int8 Ranged Clip Instructions)">;

// Note: XRemovedSfvfbfexp16e depends on either Zvfbfmin _or_ Zvfbfa, which cannot be expressed here in
// TableGen. Instead, we check that in YSXISAInfo.
def YSXDisabledVendorFeatureXRemovedSfvfbfexp16e
    : YSXExtension<0, 5,
                     "SiFive Vector Floating-Point Exponential Function Instruction, BFloat16">;
def HasVendorXRemovedSfvfbfexp16e : Predicate<"Subtarget->hasVendorXRemovedSfvfbfexp16e()">;

def YSXDisabledVendorFeatureXRemovedSfvfexp16e
    : YSXExtension<0, 5,
                     "SiFive Vector Floating-Point Exponential Function Instruction, Half Precision",
                     [FeatureStdExtZvfh]>;
def HasVendorXRemovedSfvfexp16e : Predicate<"Subtarget->hasVendorXRemovedSfvfexp16e()">;

def YSXDisabledVendorFeatureXRemovedSfvfexp32e
    : YSXExtension<0, 5,
                     "SiFive Vector Floating-Point Exponential Function Instruction, Single Precision",
                     [FeatureStdExtZve32f]>;
def HasVendorXRemovedSfvfexp32e : Predicate<"Subtarget->hasVendorXRemovedSfvfexp32e()">;

def HasVendorXRemovedSfvfexpAnyFloat : Predicate<"Subtarget->hasVendorXRemovedSfvfexp16e() || Subtarget->hasVendorXRemovedSfvfexp32e()">;
def HasVendorXRemovedSfvfexpAny : Predicate<"Subtarget->hasVendorXRemovedSfvfbfexp16e() || Subtarget->hasVendorXRemovedSfvfexp16e() || Subtarget->hasVendorXRemovedSfvfexp32e()">,
                           AssemblerPredicate<(any_of YSXDisabledVendorFeatureXRemovedSfvfbfexp16e, YSXDisabledVendorFeatureXRemovedSfvfexp16e, YSXDisabledVendorFeatureXRemovedSfvfexp32e),
                           "'Xsfvfbfexp16e', 'Xsfvfexp16e', or 'Xsfvfexp32e' (SiFive Vector Floating-Point Exponential Function Instruction)">;

def YSXDisabledVendorFeatureXRemovedSfvfexpa
    : YSXExtension<0, 2,
                     "SiFive Vector Floating-Point Exponential Approximation Instruction",
                     [FeatureStdExtZve32f]>;
def HasVendorXRemovedSfvfexpa : Predicate<"Subtarget->hasVendorXRemovedSfvfexpa()">,
                         AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfvfexpa),
                         "'Xsfvfexpa' (SiFive Vector Floating-Point Exponential Approximation Instruction)">;

def YSXDisabledVendorFeatureXRemovedSfvfexpa64e
    : YSXExtension<0, 2,
                     "SiFive Vector Floating-Point Exponential Approximation Instruction with Double-Precision",
                     [YSXDisabledVendorFeatureXRemovedSfvfexpa, FeatureStdExtZve64d]>;
def HasVendorXRemovedSfvfexpa64e : Predicate<"Subtarget->hasVendorXRemovedSfvfexpa64e()">;

def YSXDisabledVendorFeatureXSiFivecdiscarddlone
    : YSXExtension<1, 0,
                     "SiFive sf.cdiscard.d.l1 Instruction", []>;
def HasVendorXSiFivecdiscarddlone
    : Predicate<"Subtarget->hasVendorXSiFivecdiscarddlone()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXSiFivecdiscarddlone),
                         "'XSiFivecdiscarddlone' (SiFive sf.cdiscard.d.l1 Instruction)">;

def YSXDisabledVendorFeatureXSiFivecflushdlone
    : YSXExtension<1, 0,
                     "SiFive sf.cflush.d.l1 Instruction", []>;
def HasVendorXSiFivecflushdlone
    : Predicate<"Subtarget->hasVendorXSiFivecflushdlone()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXSiFivecflushdlone),
                         "'XSiFivecflushdlone' (SiFive sf.cflush.d.l1 Instruction)">;

def YSXDisabledVendorFeatureXRemovedSfcease
    : YSXExtension<1, 0,
                     "SiFive sf.cease Instruction", []>;
def HasVendorXRemovedSfcease
    : Predicate<"Subtarget->hasVendorXRemovedSfcease()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedSfcease),
                         "'XRemovedSfcease' (SiFive sf.cease Instruction)">;

def YSXDisabledVendorFeatureXRemovedSfmclic
    : YSXExperimentalExtension<0, 1,
                                 "SiFive CLIC Machine-mode CSRs">;

def YSXDisabledVendorFeatureXRemovedSfsclic
    : YSXExperimentalExtension<0, 1,
                                 "SiFive CLIC Supervisor-mode CSRs">;

// Core-V Extensions

def YSXDisabledVendorFeatureXCVelw
   : YSXExtension<1, 0, "CORE-V Event Load Word">;
def HasVendorXCVelw
   : Predicate<"Subtarget->hasVendorXCVelw()">,
     AssemblerPredicate<(any_of YSXDisabledVendorFeatureXCVelw),
                        "'XCVelw' (CORE-V Event Load Word)">;

def YSXDisabledVendorFeatureXCVbitmanip
    : YSXExtension<1, 0, "CORE-V Bit Manipulation">;
def HasVendorXCVbitmanip
    : Predicate<"Subtarget->hasVendorXCVbitmanip()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXCVbitmanip),
                         "'XCVbitmanip' (CORE-V Bit Manipulation)">;

def YSXDisabledVendorFeatureXCVmac
    : YSXExtension<1, 0, "CORE-V Multiply-Accumulate">;
def HasVendorXCVmac
    : Predicate<"Subtarget->hasVendorXCVmac()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXCVmac),
                         "'XCVmac' (CORE-V Multiply-Accumulate)">;

def YSXDisabledVendorFeatureXCVmem
    : YSXExtension<1, 0, "CORE-V Post-incrementing Load & Store">;
def HasVendorXCVmem
    : Predicate<"Subtarget->hasVendorXCVmem()">,
      AssemblerPredicate<(any_of YSXDisabledVendorFeatureXCVmem),
                         "'XCVmem' (CORE-V Post-incrementing Load & Store)">;

def YSXDisabledVendorFeatureXCValu
    : YSXExtension<1, 0, "CORE-V ALU Operations">;
def HasVendorXCValu
    : Predicate<"Subtarget->hasVendorXCValu()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXCValu),
                         "'XCValu' (CORE-V ALU Operations)">;

def YSXDisabledVendorFeatureXCVsimd
    : YSXExtension<1, 0, "CORE-V SIMD ALU">;
def HasVendorXCVsimd
    : Predicate<"Subtarget->hasVendorXCVsimd()">,
      AssemblerPredicate<(any_of YSXDisabledVendorFeatureXCVsimd),
                         "'XCVsimd' (CORE-V SIMD ALU)">;

def YSXDisabledVendorFeatureXCVbi
    : YSXExtension<1, 0, "CORE-V Immediate Branching">;
def HasVendorXCVbi
    : Predicate<"Subtarget->hasVendorXCVbi()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXCVbi),
                         "'XCVbi' (CORE-V Immediate Branching)">;
// MIPS Extensions

def YSXDisabledVendorFeatureXMIPSCMov
    : YSXExtension<1, 0, "MIPS conditional move instruction (mips.ccmov)">;
def HasVendorXMIPSCMov
    : Predicate<"Subtarget->hasVendorXMIPSCMov()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXMIPSCMov),
                         "'Xmipscmov' ('mips.ccmov' instruction)">;
def UseMIPSCCMovInsn : Predicate<"Subtarget->useMIPSCCMovInsn()">;

def YSXDisabledVendorFeatureXMIPSLSP
    : YSXExtension<1, 0, "MIPS optimization for hardware load-store bonding">;
def HasVendorXMIPSLSP
    : Predicate<"Subtarget->hasVendorXMIPSLSP()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXMIPSLSP),
                         "'Xmipslsp' (load and store pair instructions)">;

def YSXDisabledVendorFeatureXMIPSCBOP : YSXExtension<1, 0, "MIPS Software Prefetch">;
def HasVendorXMIPSCBOP
    : Predicate<"Subtarget->hasVendorXMIPSCBOP()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXMIPSCBOP),
                         "'Xmipscbop' (MIPS hardware prefetch)">;
def NoVendorXMIPSCBOP : Predicate<"!Subtarget->hasVendorXMIPSCBOP()">;

def YSXDisabledVendorFeatureXMIPSEXECTL : YSXExtension<1, 0, "MIPS execution control">;
def HasVendorXMIPSEXECTL
    : Predicate<"Subtarget->hasVendorXMIPSEXECTL()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXMIPSEXECTL),
                         "'Xmipsexectl' (MIPS execution control)">;

// WCH / Nanjing Qinheng Microelectronics Extension(s)

def YSXDisabledVendorFeatureXwchc
    : YSXExtension<2, 2,
                     "WCH/QingKe additional compressed opcodes",
                     [FeatureStdExtZca]>;
def HasVendorXwchc
    : Predicate<"Subtarget->hasVendorXwchc()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXwchc),
                         "'Xwchc' (WCH/QingKe additional compressed opcodes)">;

// Qualcomm Extensions

def YSXDisabledVendorFeatureXqccmp
    : YSXExtension<0, 3, "Qualcomm 16-bit Push/Pop and Double Moves",
                     [FeatureStdExtZca]>;
def HasVendorXqccmp
    : Predicate<"Subtarget->hasVendorXqccmp()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXqccmp),
                         "'Xqccmp' (Qualcomm 16-bit Push/Pop and Double Moves)">;

def YSXDisabledVendorFeatureXRemovedQcia
    : YSXExtension<0, 7, "Qualcomm uC Arithmetic Extension">;
def HasVendorXRemovedQcia
    : Predicate<"Subtarget->hasVendorXRemovedQcia()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcia),
                         "'XRemovedQcia' (Qualcomm uC Arithmetic Extension)">;

def YSXDisabledVendorFeatureXRemovedQciac
    : YSXExtension<0, 3, "Qualcomm uC Load-Store Address Calculation Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQciac
    : Predicate<"Subtarget->hasVendorXRemovedQciac()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQciac),
                         "'XRemovedQciac' (Qualcomm uC Load-Store Address Calculation Extension)">;

def YSXDisabledVendorFeatureXRemovedQcibi
    : YSXExtension<0, 2, "Qualcomm uC Branch Immediate Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcibi
    : Predicate<"Subtarget->hasVendorXRemovedQcibi()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcibi),
                         "'XRemovedQcibi' (Qualcomm uC Branch Immediate Extension)">;

def YSXDisabledVendorFeatureXRemovedQcibm
    : YSXExtension<0, 8, "Qualcomm uC Bit Manipulation Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcibm
    : Predicate<"Subtarget->hasVendorXRemovedQcibm()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcibm),
                         "'XRemovedQcibm' (Qualcomm uC Bit Manipulation Extension)">;

def YSXDisabledVendorFeatureXRemovedQcicli
    : YSXExtension<0, 3, "Qualcomm uC Conditional Load Immediate Extension">;
def HasVendorXRemovedQcicli
    : Predicate<"Subtarget->hasVendorXRemovedQcicli()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcicli),
                         "'XRemovedQcicli' (Qualcomm uC Conditional Load Immediate Extension)">;

def YSXDisabledVendorFeatureXRemovedQcicm
    : YSXExtension<0, 2, "Qualcomm uC Conditional Move Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcicm
    : Predicate<"Subtarget->hasVendorXRemovedQcicm()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcicm),
                         "'XRemovedQcicm' (Qualcomm uC Conditional Move Extension)">;
def NoVendorXRemovedQcicm
    : Predicate<"!Subtarget->hasVendorXRemovedQcicm()">;

def YSXDisabledVendorFeatureXRemovedQcics
    : YSXExtension<0, 2, "Qualcomm uC Conditional Select Extension">;
def HasVendorXRemovedQcics
    : Predicate<"Subtarget->hasVendorXRemovedQcics()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcics),
                         "'XRemovedQcics' (Qualcomm uC Conditional Select Extension)">;
def NoVendorXRemovedQcics
    : Predicate<"!Subtarget->hasVendorXRemovedQcics()">;

def YSXDisabledVendorFeatureXRemovedQcicsr
    : YSXExtension<0, 4, "Qualcomm uC CSR Extension">;
def HasVendorXRemovedQcicsr
    : Predicate<"Subtarget->hasVendorXRemovedQcicsr()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcicsr),
                         "'XRemovedQcicsr' (Qualcomm uC CSR Extension)">;

def YSXDisabledVendorFeatureXRemovedQciint
    : YSXExtension<0, 10, "Qualcomm uC Interrupts Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQciint
    : Predicate<"Subtarget->hasVendorXRemovedQciint()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQciint),
                         "'XRemovedQciint' (Qualcomm uC Interrupts Extension)">;

def YSXDisabledVendorFeatureXRemovedQciio
    : YSXExtension<0, 1, "Qualcomm uC External Input Output Extension">;
def HasVendorXRemovedQciio
    : Predicate<"Subtarget->hasVendorXRemovedQciio()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQciio),
                         "'XRemovedQciio' (Qualcomm uC External Input Output Extension)">;

def YSXDisabledVendorFeatureXRemovedQcilb
    : YSXExtension<0, 2, "Qualcomm uC Long Branch Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcilb
    : Predicate<"Subtarget->hasVendorXRemovedQcilb()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcilb),
                         "'XRemovedQcilb' (Qualcomm uC Long Branch Extension)">;

def YSXDisabledVendorFeatureXRemovedQcili
    : YSXExtension<0, 2, "Qualcomm uC Load Large Immediate Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcili
    : Predicate<"Subtarget->hasVendorXRemovedQcili()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcili),
                         "'XRemovedQcili' (Qualcomm uC Load Large Immediate Extension)">;

def YSXDisabledVendorFeatureXRemovedQcilia
    : YSXExtension<0, 2, "Qualcomm uC Large Immediate Arithmetic Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcilia
    : Predicate<"Subtarget->hasVendorXRemovedQcilia()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcilia),
                         "'XRemovedQcilia' (Qualcomm uC Large Immediate Arithmetic Extension)">;

def YSXDisabledVendorFeatureXRemovedQcilo
    : YSXExtension<0, 3, "Qualcomm uC Large Offset Load Store Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcilo
    : Predicate<"Subtarget->hasVendorXRemovedQcilo()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcilo),
                         "'XRemovedQcilo' (Qualcomm uC Large Offset Load Store Extension)">;

def YSXDisabledVendorFeatureXRemovedQcilsm
    : YSXExtension<0, 6, "Qualcomm uC Load Store Multiple Extension">;
def HasVendorXRemovedQcilsm
    : Predicate<"Subtarget->hasVendorXRemovedQcilsm()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcilsm),
                         "'XRemovedQcilsm' (Qualcomm uC Load Store Multiple Extension)">;

def YSXDisabledVendorFeatureXRemovedQcisim
    : YSXExtension<0, 2, "Qualcomm uC Simulation Hint Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcisim
    : Predicate<"Subtarget->hasVendorXRemovedQcisim()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcisim),
                         "'XRemovedQcisim' (Qualcomm uC Simulation Hint Extension)">;

def YSXDisabledVendorFeatureXRemovedQcisls
    : YSXExtension<0, 2, "Qualcomm uC Scaled Load Store Extension">;
def HasVendorXRemovedQcisls
    : Predicate<"Subtarget->hasVendorXRemovedQcisls()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcisls),
                         "'XRemovedQcisls' (Qualcomm uC Scaled Load Store Extension)">;

def YSXDisabledVendorFeatureXRemovedQcisync
    : YSXExtension<0, 3, "Qualcomm uC Sync Delay Extension",
                     [FeatureStdExtZca]>;
def HasVendorXRemovedQcisync
    : Predicate<"Subtarget->hasVendorXRemovedQcisync()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRemovedQcisync),
                         "'XRemovedQcisync' (Qualcomm uC Sync Delay Extension)">;

def YSXDisabledVendorFeatureXRemovedQci
    : YSXExtension<0, 13, "Qualcomm uC Extension",
                     [YSXDisabledVendorFeatureXRemovedQcia, YSXDisabledVendorFeatureXRemovedQciac,
                      YSXDisabledVendorFeatureXRemovedQcibi, YSXDisabledVendorFeatureXRemovedQcibm,
                      YSXDisabledVendorFeatureXRemovedQcicli, YSXDisabledVendorFeatureXRemovedQcicm,
                      YSXDisabledVendorFeatureXRemovedQcics, YSXDisabledVendorFeatureXRemovedQcicsr,
                      YSXDisabledVendorFeatureXRemovedQciint, YSXDisabledVendorFeatureXRemovedQciio,
                      YSXDisabledVendorFeatureXRemovedQcilb, YSXDisabledVendorFeatureXRemovedQcili,
                      YSXDisabledVendorFeatureXRemovedQcilia, YSXDisabledVendorFeatureXRemovedQcilo,
                      YSXDisabledVendorFeatureXRemovedQcilsm, YSXDisabledVendorFeatureXRemovedQcisim,
                      YSXDisabledVendorFeatureXRemovedQcisls, YSXDisabledVendorFeatureXRemovedQcisync]>;

// Rivos Extension(s)

def YSXDisabledVendorFeatureXRivosVisni
    :  YSXExperimentalExtension<0, 1, "Rivos Vector Integer Small New">;
def HasVendorXRivosVisni
    : Predicate<"Subtarget->hasVendorXRivosVisni()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRivosVisni),
                         "'XRivosVisni' (Rivos Vector Integer Small New)">;

def YSXDisabledVendorFeatureXRivosVizip
    :  YSXExperimentalExtension<0, 1, "Rivos Vector Register Zips">;
def HasVendorXRivosVizip
    : Predicate<"Subtarget->hasVendorXRivosVizip()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXRivosVizip),
                         "'XRivosVizip' (Rivos Vector Register Zips)">;

// Andes Extension(s)

def YSXDisabledVendorFeatureXAndesPerf
    : YSXExtension<5, 0, "Andes Performance Extension">;
def HasVendorXAndesPerf
    : Predicate<"Subtarget->hasVendorXAndesPerf()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesPerf),
                         "'XAndesPerf' (Andes Performance Extension)">;

def YSXDisabledVendorFeatureXAndesBFHCvt
    : YSXExtension<5, 0, "Andes Scalar BFLOAT16 Conversion Extension",
                     [YSXDisabledStdExtF]>;
def HasVendorXAndesBFHCvt
    : Predicate<"Subtarget->hasVendorXAndesBFHCvt()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesBFHCvt),
                         "'XAndesBFHCvt' (Andes Scalar BFLOAT16 Conversion Extension)">;

def YSXDisabledVendorFeatureXAndesVBFHCvt
    : YSXExtension<5, 0, "Andes Vector BFLOAT16 Conversion Extension",
                     [FeatureStdExtZve32f]>;
def HasVendorXAndesVBFHCvt
    : Predicate<"Subtarget->hasVendorXAndesVBFHCvt()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesVBFHCvt),
                         "'XAndesVBFHCvt' (Andes Vector BFLOAT16 Conversion Extension)">;

def YSXDisabledVendorFeatureXAndesVSIntH
    : YSXExtension<5, 0, "Andes Vector Small INT Handling Extension",
                     [FeatureStdExtZve32x]>;
def HasVendorXAndesVSIntH
    : Predicate<"Subtarget->hasVendorXAndesVSIntH()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesVSIntH),
                         "'XAndesVSIntH' (Andes Vector Small INT Handling Extension)">;

def YSXDisabledVendorFeatureXAndesVSIntLoad
    : YSXExtension<5, 0, "Andes Vector INT4 Load Extension",
                     [FeatureStdExtZve32x]>;
def HasVendorXAndesVSIntLoad
    : Predicate<"Subtarget->hasVendorXAndesVSIntLoad()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesVSIntLoad),
                         "'XAndesVSIntLoad' (Andes Vector INT4 Load Extension)">;

def YSXDisabledVendorFeatureXAndesVPackFPH
    : YSXExtension<5, 0,
                     "Andes Vector Packed FP16 Extension", [YSXDisabledStdExtF]>;
def HasVendorXAndesVPackFPH
    : Predicate<"Subtarget->hasVendorXAndesVPackFPH()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesVPackFPH),
                         "'XAndesVPackFPH' (Andes Vector Packed FP16 Extension)">;

def YSXDisabledVendorFeatureXAndesVDot
    : YSXExtension<5, 0, "Andes Vector Dot Product Extension",
                     [FeatureStdExtZve32x]>;
def HasVendorXAndesVDot
    : Predicate<"Subtarget->hasVendorXAndesVDot()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXAndesVDot),
                         "'XAndesVDot' (Andes Vector Dot Product Extension)">;

def YSXDisabledVendorFeatureXSMTVDot
    : YSXExtension<1, 0, "SpacemiT Vector Dot Product Extension",
                     [FeatureStdExtZve32f]>;
def HasVendorXSMTVDot
    : Predicate<"Subtarget->hasVendorXSMTVDot()">,
      AssemblerPredicate<(all_of YSXDisabledVendorFeatureXSMTVDot),
                         "'XSMTVDot' (SpacemiT Vector Dot Product Extension)">;

//===----------------------------------------------------------------------===//
// LLVM specific features and extensions
//===----------------------------------------------------------------------===//

// Feature32Bit exists to mark CPUs that support RV32 to distinguish them from
// tuning CPU names.
def Feature32Bit
    : SubtargetFeature<"32bit", "IsRV32", "true", "Implements RV32">;
def Feature64Bit
    : SubtargetFeature<"64bit", "IsRV64", "true", "Implements RV64">;
def IsRV64 : Predicate<"Subtarget->is64Bit()">,
             AssemblerPredicate<(all_of Feature64Bit),
                                "RV64I Base Instruction Set">;
def IsRV32 : Predicate<"!Subtarget->is64Bit()">,
             AssemblerPredicate<(all_of (not Feature64Bit)),
                                "RV32I Base Instruction Set">;

defvar RV32 = DefaultMode;
def RV64 : HwMode<[IsRV64]>;

def FeatureRelax
    : SubtargetFeature<"relax", "EnableLinkerRelax", "true",
                       "Enable Linker relaxation.">;

def FeatureExactAssembly
    : SubtargetFeature<"exact-asm", "EnableExactAssembly", "true",
                       "Enable Exact Assembly (Disables Compression and Relaxation)">;

foreach i = {1-31} in
  def FeatureReserveX#i :
      SubtargetFeature<"reserve-x"#i, "UserReservedRegister[YSX::X"#i#"]",
                       "true", "Reserve X"#i>;

def FeatureSaveRestore : SubtargetFeature<"save-restore", "EnableSaveRestore",
                                          "true", "Enable save/restore.">;

def FeatureNoTrailingSeqCstFence : SubtargetFeature<"no-trailing-seq-cst-fence",
                                          "EnableTrailingSeqCstFence",
                                          "false",
                                          "Disable trailing fence for seq-cst store.">;

def FeatureUnalignedScalarMem
   : SubtargetFeature<"unaligned-scalar-mem", "EnableUnalignedScalarMem",
                      "true", "Has reasonably performant unaligned scalar "
                      "loads and stores">;

def FeatureUnalignedVectorMem
   : SubtargetFeature<"unaligned-vector-mem", "EnableUnalignedVectorMem",
                      "true", "Has reasonably performant unaligned vector "
                      "loads and stores">;

// Assume that lock-free native-width atomics are available, even if the target
// and operating system combination would not usually provide them. The user
// is responsible for providing any necessary __sync implementations. Code
// built with this feature is not ABI-compatible with code built without this
// feature, if atomic variables are exposed across the ABI boundary.
def FeatureForcedAtomics : SubtargetFeature<
    "forced-atomics", "HasForcedAtomics", "true",
    "Assume that lock-free native-width atomics are available">;
def HasAtomicLdSt
    : Predicate<"Subtarget->hasStdExtZalrsc() || Subtarget->hasForcedAtomics()">;

// The RISC-V Unprivileged Architecture - ISA Volume 1 (Version: 20250508)
// [https://docs.ysx.org/reference/isa/_attachments/ysx-unprivileged.pdf]
// in section 13.3. Eventual Success of Store-Conditional Instructions, defines
// _constrained_ LR/SC loops:
//   The dynamic code executed between the LR and SC instructions can only
//   contain instructions from the base ''I'' instruction set, excluding loads,
//   stores, backward jumps, taken backward branches, JALR, FENCE, and SYSTEM
//   instructions. Compressed forms of the aforementioned ''I'' instructions in
//   the Zca and Zcb extensions are also permitted.
// LR/SC loops that do not adhere to the above are _unconstrained_ LR/SC loops,
// and success is implementation specific. For implementations which know that
// non-base instructions (such as the ''B'' extension) will not violate any
// forward progress guarantees, using these instructions to reduce the LR/SC
// sequence length is desirable.
def FeaturePermissiveZalrsc
    : SubtargetFeature<
          "permissive-zalrsc", "HasPermissiveZalrsc", "true",
          "Implementation permits non-base instructions between LR/SC pairs">;

def FeatureTaggedGlobals : SubtargetFeature<"tagged-globals",
    "AllowTaggedGlobals",
    "true", "Use an instruction sequence for taking the address of a global "
    "that allows a memory tag in the upper address bits">;

//===----------------------------------------------------------------------===//
// Tuning features
//===----------------------------------------------------------------------===//

def TuneNLogNVRGather
   : SubtargetFeature<"log-vrgather", "YSXVRGatherCostModel", "NLog2N",
                      "Has vrgather.vv with LMUL*log2(LMUL) latency">;

def TunePostRAScheduler : SubtargetFeature<"use-postra-scheduler",
    "UsePostRAScheduler", "true", "Schedule again after register allocation">;

def TuneDisableMISchedLoadClustering : SubtargetFeature<"disable-misched-load-clustering",
    "EnableMISchedLoadClustering", "false", "Disable load clustering in the machine scheduler">;

def TuneDisableMISchedStoreClustering : SubtargetFeature<"disable-misched-store-clustering",
    "EnableMISchedStoreClustering", "false", "Disable store clustering in the machine scheduler">;

def TuneDisablePostMISchedLoadClustering : SubtargetFeature<"disable-postmisched-load-clustering",
    "EnablePostMISchedLoadClustering", "false", "Disable PostRA load clustering in the machine scheduler">;

def TuneDisablePostMISchedStoreClustering : SubtargetFeature<"disable-postmisched-store-clustering",
    "EnablePostMISchedStoreClustering", "false", "Disable PostRA store clustering in the machine scheduler">;

def TuneDisableLatencySchedHeuristic
    : SubtargetFeature<"disable-latency-sched-heuristic", "DisableLatencySchedHeuristic", "true",
                       "Disable latency scheduling heuristic">;

def TuneEnableVsetvliSchedHeuristic
    : SubtargetFeature<"enable-vsetvli-sched-heuristic", "EnableVsetvliSchedHeuristic", "true",
                       "Enable vsetvli-based scheduling heuristic">;

def TunePredictableSelectIsExpensive
    : SubtargetFeature<"predictable-select-expensive", "PredictableSelectIsExpensive", "true",
                       "Prefer likely predicted branches over selects">;

def TuneOptimizedZeroStrideLoad
   : SubtargetFeature<"optimized-zero-stride-load", "HasOptimizedZeroStrideLoad",
                      "true", "Optimized (perform fewer memory operations)"
                      "zero-stride vector load">;

foreach nf = {2-8} in
  def TuneOptimizedNF#nf#SegmentLoadStore :
      SubtargetFeature<"optimized-nf"#nf#"-segment-load-store",
                       "HasOptimizedNF"#nf#"SegmentLoadStore",
                       "true", "vlseg"#nf#"eN.v and vsseg"#nf#"eN.v are "
                       "implemented as a wide memory op and shuffle">;

def TuneVLDependentLatency
    : SubtargetFeature<"vl-dependent-latency", "HasVLDependentLatency", "true",
                       "Latency of vector instructions is dependent on the "
                       "dynamic value of vl">;

def Experimental
   : SubtargetFeature<"experimental", "HasExperimental",
                      "true", "Experimental intrinsics">;

// Some vector hardware implementations do not process all VLEN bits in parallel
// and instead split over multiple cycles. DLEN refers to the datapath width
// that can be done in parallel.
def TuneDLenFactor2
   : SubtargetFeature<"dlen-factor-2", "DLenFactor2", "true",
                      "Vector unit DLEN(data path width) is half of VLEN">;

def TuneNoDefaultUnroll
    : SubtargetFeature<"no-default-unroll", "EnableDefaultUnroll", "false",
                       "Disable default unroll preference.">;

// Many Microarchitectures are able to fuse a branch over a single instruction
// with the branched-over instruction. We call this fusion "short forward
// branches".
//
// We can do this for a variety of instruction groups, depending on the
// microarch. We broadly group these by their scheduler class:
// - IALU: RVI Integer instructions, plus ANDN/ORN/XNOR (Zbb/Zbkb)
// - IMinMax: Zbb MIN(U)/MAX(U)
// - IMul: MUL
// - ILoad: LB(U)/LH(U)/LW(U)/LD
//
// We make the simplifying assumption that any microarches that implement
// any "short forward branches" can do the IALU fusions, and can opt into
// the other fusions they implement.
//
// The important Pseudo used by all these instructions requires the IALU
// short forward branches.
//
// Vendor-specific short-forward-branch opts may be added under IALU, as
// the vendor-specific instructions should only be enabled for vendor
// cores.
def TuneShortForwardBranchIALU
    : SubtargetFeature<"short-forward-branch-ialu", "HasShortForwardBranchIALU",
                       "true", "Enable short forward branch optimization for RVI base instructions">;
def HasShortForwardBranchIALU : Predicate<"Subtarget->hasShortForwardBranchIALU()">;
def NoShortForwardBranch : Predicate<"!Subtarget->hasShortForwardBranchIALU()">;

def TuneShortForwardBranchIMinMax
    : SubtargetFeature<"short-forward-branch-iminmax", "HasShortForwardBranchIMinMax",
                       "true", "Enable short forward branch optimization for MIN,MAX instructions in Zbb",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchIMinMax : Predicate<"Subtarget->hasShortForwardBranchIMinMax()">;

def TuneShortForwardBranchIMul
    : SubtargetFeature<"short-forward-branch-imul", "HasShortForwardBranchIMul",
                       "true", "Enable short forward branch optimization for MUL instruction",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchIMul : Predicate<"Subtarget->hasShortForwardBranchIMul()">;

def TuneShortForwardBranchILoad
    : SubtargetFeature<"short-forward-branch-iload", "HasShortForwardBranchILoad",
                       "true", "Enable short forward branch optimization for load instructions",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchILoad : Predicate<"Subtarget->hasShortForwardBranchILoad()">;

// Some subtargets require a S2V transfer buffer to move scalars into vectors.
// FIXME: Forming .vx/.vf/.wx/.wf can reduce register pressure.
def TuneNoSinkSplatOperands
    : SubtargetFeature<"no-sink-splat-operands", "SinkSplatOperands",
                       "false", "Disable sink splat operands to enable .vx, .vf,"
                       ".wx, and .wf instructions">;

def TunePreferWInst
    : SubtargetFeature<"prefer-w-inst", "PreferWInst", "true",
                       "Prefer instructions with W suffix">;

def TuneConditionalCompressedMoveFusion
    : SubtargetFeature<"conditional-cmv-fusion", "HasConditionalCompressedMoveFusion",
                       "true", "Enable branch+c.mv fusion">;
def HasConditionalMoveFusion : Predicate<"Subtarget->hasConditionalMoveFusion()">;
def NoConditionalMoveFusion  : Predicate<"!Subtarget->hasConditionalMoveFusion()">;

def TuneHasSingleElementVecFP64
    : SubtargetFeature<"single-element-vec-fp64", "HasSingleElementVectorFP64", "true",
                       "Certain vector FP64 operations produce a single result "
                       "element per cycle">;

def TuneVXRMPipelineFlush : SubtargetFeature<"vxrm-pipeline-flush", "HasVXRMPipelineFlush",
                                             "true", "VXRM writes causes pipeline flush">;

def TunePreferVsetvliOverReadVLENB
    : SubtargetFeature<"prefer-vsetvli-over-read-vlenb",
                       "PreferVsetvliOverReadVLENB",
                       "true",
                       "Prefer vsetvli over read vlenb CSR to calculate VLEN">;

//===----------------------------------------------------------------------===//
// CPU Families (alphabetized by vendor).
//===----------------------------------------------------------------------===//

def TuneAndes45 : SubtargetFeature<"andes45", "YSXProcFamily", "Andes45",
                                   "Andes 45-Series processors">;

def TuneMIPSP8700
    : SubtargetFeature<"mips-p8700", "YSXProcFamily", "MIPSP8700",
                       "MIPS p8700 processor">;

def TuneSiFive7 : SubtargetFeature<"sifive7", "YSXProcFamily", "SiFive7",
                                   "SiFive 7-Series processors">;

def TuneVentanaVeyron : SubtargetFeature<"ventana-veyron", "YSXProcFamily", "VentanaVeyron",
                                         "Ventana Veyron-Series processors">;

