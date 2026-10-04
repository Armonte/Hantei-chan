// HAN2RBO container types (RBO engine). Evidence: Han2Dat_* loaders 0x403420..0x403D10, see docs/formats/frenchbread_rbo_gof.md section 2.
enum RboHan2Kind : unsigned int { HAN2_KIND_FULL_DAT=0, HAN2_KIND_PATTERN_ONLY_DT2=3 };
struct RboHan2AreaRef {
 unsigned int offset;                  // +0x0 absolute file offset (a .DT2 keeps the .DAT's offsets for areas it does not carry)
 unsigned int size;                    // +0x4 0 = area absent
};
struct RboHan2FileHeader {             // 0x40 bytes at file start. The engine reads only bytes 0..0x3F in two 32-byte reads (Han2Dat_ReadFileHeader32 0x4035F0, Han2Dat_ReadPatternHeader32 0x403610)
 char signature[8];                    // +0x00 "HAN2RBO " (never checked by the RBO engine; tools check it)
 RboHan2Kind kind;                     // +0x08 not read by the engine; 0 = full .DAT, 3 = pattern-area-only .DT2
 unsigned int unused_0C;               // +0x0C zero in all 346 RBO / 68 GOF2 files, not read
 unsigned int version;                 // +0x10 always 2, not read by the RBO engine
 unsigned int unused_14;               // +0x14 zero in all files, not read
 unsigned int subVersion;              // +0x18 1 = RBO layout (300-byte frames, 8 sections), 2 = GOF2 layout; not read by the RBO engine
 unsigned int xorFlag;                 // +0x1C non-zero = pattern area is XOR-obfuscated (Han2Dat_DecryptBlockIfFlag 0x403430); 0 in every shipped file
 RboHan2AreaRef patternArea;           // +0x20 section table + sections (Han2Dat_LoadPatternAreaAndResolve 0x403630); taken from the .DT2 when it exists
 RboHan2AreaRef partsPat;              // +0x28 PAT v3 block (Han2Dat_LoadPartsPatBlock 0x4037D0), always from the .DAT
 RboHan2AreaRef cgBank;                // +0x30 BMP Cutter3 bank (Han2Dat_LoadCgBank 0x4038F0), always from the .DAT
 RboHan2AreaRef patternNames;          // +0x38 256 x 64-byte CP932 names; not read by the engine (editor data)
};
struct RboPatternAreaHeader {          // RBO sub 1: 0x58 bytes at patternArea.offset (Han2Dat_ComputeSectionPointers 0x403550 starts the sections at +88)
 unsigned int unused_00;               // +0x00 zero in all 346 files, not read
 unsigned int editorMark1;             // +0x04 3 in 345 files, 2 in one; not read by the engine (GOF2 stores 8)
 unsigned int unused_08;               // +0x08 zero in all files, not read
 unsigned int editorMark2;             // +0x0C 1 in all RBO files; not read by the engine (GOF2 stores 5)
 unsigned int unused_10[7];            // +0x10..0x2B zero in all files, not read
 unsigned int sectionSize[8];          // +0x2C section byte sizes (Han2Dat_ComputeSectionPointers a1[11..18]); 0 = absent
 unsigned int unused_4C[3];            // +0x4C zero in all files, not read
};
struct RboPatternAreaView {            // runtime pointers, actor+2032 points here (Actor_GetFrameRecord 0x4408F0 reads +4 and +8); filled by Han2Dat_ComputeSectionPointers a2[0..8]
 void *base;                           // +0x00 GlobalAlloc block holding the whole pattern area
 RboPatternEntry *patterns;            // +0x04 section 0: 256 x 12 bytes
 RboFrameRecord *frames;               // +0x08 section 1: 300-byte records
 void *boxes;                          // +0x0C section 2: 8-byte i16 x1,y1,x2,y2 rectangles
 void *attackRecords;                  // +0x10 section 3: 120-byte AT records
 void *section4;                       // +0x14 section 4 (196 bytes in ACOLYTE_F)
 void *section5;                       // +0x18 section 5 (40 bytes in ACOLYTE_F)
 void *scriptListsA;                   // +0x1C section 6: 20-byte script id lists
 void *scriptListsB;                   // +0x20 section 7: 20-byte script id lists
};
struct RboBoxRect {
 __int16 x1;                           // +0x0 Actor_PickBoxRect_244 0x442190 copies four shorts
 __int16 y1;                           // +0x2
 __int16 x2;                           // +0x4
 __int16 y2;                           // +0x6
};
