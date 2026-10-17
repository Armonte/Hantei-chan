// HAN2RBO container types, GOF2 layout (.DT2, header +0x08 kind 3, +0x18 sub 2). Same 0x40-byte file header as RBO (rbo_container_types.h); the pattern area differs: 12 lead dwords + 9 section sizes + 3 tail dwords = 0x60 bytes.
// Evidence: Gof2Han2_OpenPreferDt2ReadHeader32 0x4469F0, Gof2Han2_ReadAreaRefs32 0x4469B0, CharacterLoadStateMachine 0x4477F0 (State0_0..State0_4), Gof2PatternArea_ComputeSectionPointers 0x446920, Gof2PatternArea_DecryptSections0to5 0x446860. Prefix T = reader traced, E = editor-only (checked, never read by the loader), V = validated by the loader (a mismatch aborts the load).
enum Gof2Han2Kind : unsigned int { G2HAN2_KIND_FULL_DAT=0, G2HAN2_KIND_PATTERN_ONLY_DT2=3 };
struct Gof2Han2AreaRef {
 unsigned int offset;                  // +0x0 absolute file offset (a .DT2 keeps the .DAT's offsets for areas it does not carry)
 unsigned int size;                    // +0x4 0 = area absent
};
struct Gof2Han2FileHeader {            // 0x40 bytes at file start, read in two 32-byte reads (Gof2Han2_OpenPreferDt2ReadHeader32 reads 0..0x1F, Gof2Han2_ReadAreaRefs32 seeks to 0x20 and reads 0x20..0x3F)
 char signature[8];                    // +0x00 "HAN2RBO " (never checked by the game)
 Gof2Han2Kind kind;                    // +0x08 E: not read; 3 in all 68 GOF2 files
 unsigned int unused_0C;               // +0x0C E: zero in all 68 files, not read
 unsigned int version;                 // +0x10 E: 2 in all files, not read
 unsigned int unused_14;               // +0x14 E: zero in all files, not read
 unsigned int subVersion;              // +0x18 E: 2 = GOF2 layout (404-byte frames, 9 sections); not read by the game
 unsigned int xorFlag;                 // +0x1C T: non-zero = header + sections 0..5 are XOR-obfuscated (State0_4_NextFile -> Gof2_XorDecryptBuffer); 0 in every shipped file
 Gof2Han2AreaRef patternArea;          // +0x20 T: read by State0_1 into a1[11..12], the area is loaded by State0_2 (AllocateAndReadFileData(path, offset, size))
 Gof2Han2AreaRef partsPat;             // +0x28 T: PAT parts bank, always from the .DAT (a1[13..14] -> chara block +52/+56)
 Gof2Han2AreaRef cgBank;               // +0x30 T: BMP Cutter3 bank, always from the .DAT
 Gof2Han2AreaRef patternNames;         // +0x38 E: 256 x 64-byte CP932 pattern names; editor data, not read
};
struct Gof2PatternAreaHeader {         // 0x60 bytes at patternArea.offset; the sections follow at +0x60 (Gof2PatternArea_ComputeSectionPointers: base+96)
 unsigned int mustBeZero_00;           // +0x00 V: must be 0 (State0_4_NextFile aborts the load otherwise)
 unsigned int mustBeEight_04;          // +0x04 V: must be 8 (RBO stores 3 here)
 unsigned int mustBeZero_08;           // +0x08 V: must be 0
 unsigned int mustBeFive_0C;           // +0x0C V: must be 5 (RBO stores 1)
 unsigned int mustBeZero_10[4];        // +0x10 V: +0x10..0x1F must be 0
 unsigned int editorMark_20;           // +0x20 E: 1 in all 68 files, not validated or read
 unsigned int unused_24[3];            // +0x24 E: zero in all files, not read
 unsigned int sectionSize[9];          // +0x30 T: section byte sizes (Gof2PatternArea_ComputeSectionPointers reads a2[12..20]); 0 = absent
 unsigned int unused_54[3];            // +0x54 E: zero in all files, not read
};
struct Gof2PatternAreaView {           // 40 bytes at chara block +8; anime +0x14 (CHanteiAnime view) and Obj+636 point here
 void *base;                           // +0x00 T: GlobalAlloc block holding the whole pattern area
 struct Gof2PatternEntry *patterns;    // +0x04 T: section 0, 256 x 12 bytes (CHanteiAnime_CacheNextFrame: 12*pattern + patterns)
 struct Gof2FrameRecord *frames;       // +0x08 T: section 1, 404-byte records (frames + 404*(first+frameNo))
 struct Gof2BoxRect *boxes;            // +0x0C T: section 2, 8-byte rectangles (CAppHanteiKougeki_BuildFromFrame: boxes + 8*idx)
 void *attackRecords;                  // +0x10 T: section 3, 236-byte attack records (boxes' owner data: attackRecords + 236*frame.attackRecordIdx)
 void *section4;                       // +0x14 E: section 4 (28-byte records inferred from sizes 28/56/112), no reader of view+20
 void *section5;                       // +0x18 E: section 5 (20-byte records inferred from sizes 20/40), no reader of view+24
 void *scriptListsA;                   // +0x1C T: section 6, 20-byte lists of 5 script ids, ScriptVm kind 0 (CHanteiAnime_GetFrameScriptLists)
 void *scriptListsB;                   // +0x20 T: section 7, 20-byte lists of 5 script ids, ScriptVm kind 1
 void *effectRecords;                  // +0x24 T: section 8, 96-byte records (Obj_SetActionAndRunScript: view+36 + 96*frame.effectRecordIdx -> Obj_SpawnFrameEffectRecord)
};
struct Gof2CharaDataBlock {            // what Obj+640 points to; sits at +4 of a 32292-byte chara record (Obj_BindCharaRecord 0x4316A0: container = base + 32292*charaId + 4)
 unsigned int reserved_00;             // +0x00 T: record+4, not touched by the pattern-area loader
 unsigned int xorFlag;                 // +0x04 T: copy of file header xorFlag (State0_1_LoadPATandCHPFiles)
 struct Gof2PatternAreaView view;      // +0x08 T: filled by Gof2PatternArea_ComputeSectionPointers(&container+8, header)
 unsigned int reserved_30;             // +0x30 T: not touched by the pattern-area loader
 unsigned int partsPatOffset;          // +0x34 T: file offset of the PAT parts bank (sub_447B70 stores header partsPat.offset)
 unsigned int partsPatSize;            // +0x38 T: PAT bank size; the PAT section is outside this task (poseIndex table starts at +0x44)
};
