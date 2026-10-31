#ifndef PAT_LIMITS_H_GUARD
#define PAT_LIMITS_H_GUARD

// What UNI2 / MBTL can load from a native .pat (PAniDataFile) and the HA6 frames that draw it. The loaders do not check
// any of these: a file past them corrupts the game's heap instead of failing. Traced 2026-10-30 in uni2.exe 1.42
// (uni2_142_4ebed985) and MBTL 25646393; the layouts are the same in both.
//   part set ids   the part-set vector starts at 4000 entries and grows to id + 1 (PatFile_LoadByPath 0x4A64F0 /
//                  MBTL PatFile_Load 0x487A40, PatFile_LoadPartSet 0x4A7620); HA6 AFGX keeps the id in 16 bits
//                  (Han6_LoadFrameAF), so frames reach 0..65535.
//   cut-outs       a fixed table of 2000 slots indexed by the PPST id (PatFile_LoadCutOut_PPST 0x4A7190).
//   parts per set  80 part pointers per part set (PatPartSet_Ctor, 0x1B4-byte PatPartSet; MBTL 0x4889F0).
//   layers         AFGX layer ids >= 5 are read into a scratch layer and dropped (Han6_LoadFrameAF).
// MBAACC's .pat loader is a different one (fixed tables); these limits are not applied to it.

#include <cstddef>
#include <string>
#include <vector>

namespace patlimits {

constexpr int kMaxPartSetId = 65535;
constexpr int kMaxCutOuts = 2000;
constexpr int kMaxPartsPerSet = 80;
constexpr int kMaxLayers = 5;

// One message per broken limit (empty = the file fits). partsPerSet[i] = slot count of part set i (highest part id + 1).
std::vector<std::string> Check(size_t partSets, size_t cutOuts, const std::vector<size_t>& partsPerSet);

} // namespace patlimits

#endif /* PAT_LIMITS_H_GUARD */
