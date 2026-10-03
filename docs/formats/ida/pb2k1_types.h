// PB2K1 (Queen of Heart 2001 ~Party's Breaker~, pb2k1.exe) character .DAT types. PARTIAL: written from the loader and
// frame-pointer code; NOT yet applied to the IDB (the IDB session was closed early; see pb2k1.md "Status").
// Evidence: T = traced in pb2k1.exe.i64, D = observed in decrypted ASAHI.DAT, ? = unproven.
struct Pb2PatternHeader {              // 20 bytes at fileOffset[pattern]; frames follow at +0x14, 96 bytes each
 unsigned char frameCount;             // +0x00 D
 unsigned char unknown_01;             // +0x01 ? (0 in pattern 0)
 unsigned char unknown_02;             // +0x02 ? (0 in pattern 0)
 unsigned char unknown_03;             // +0x03 ? (0xC7 in pattern 0 of ASAHI)
 int boxTableOffset;                   // +0x04 T: PB_ObjResolveFramePointers 0x424780 -> obj+548 (-1 = none)
 int atTableOffset;                    // +0x08 T: read by 0x4382C0/0x438680 (-1 = none)
 int ifTableOffset;                    // +0x0C T: -> obj+572, 20-byte records (PB_ObjRunFrameEventsForPhase 0x4280E0)
 int efTableOffset;                    // +0x10 T: -> obj+576, 12-byte records (PB_ObjRunFrameEntryEffects 0x413220)
};
struct Pb2FrameRecord {                // 96 bytes: obj+564 = frame (anim part), obj+568 = frame+24 (state part), obj+580 = frame+76 (box indices)
 unsigned char animPart[24];           // +0x00 T (fields: u16 sprite +0, u16 +2, u16 +4, u16 duration +6, u8 drawMode +7(?), u8 blend +8, u8 alpha +9, u8 aniFlag +10, u8 jump +11, u8 +12, u8 +13, u16 zoom +14, u8 loopCount +16, u8 loopEnd +17; see pb2k1.md)
 unsigned char statePart[38];          // +0x18 T (movement/cancel flags, see pb2k1.md)
 short ifIndex[3];                     // +0x3E T: PB_ObjRunFrameEventsForPhase loop i=62..66 step 2, -1 = empty
 short efIndex[4];                     // +0x44 T: PB_ObjRunFrameEntryEffects loop i=68..74 step 2, -1 = empty
 short boxIndex[10];                   // +0x4C T: obj+580, 10 slots (indices 0,6,8,9 read by 0x430760/0x438A10/0x4382C0)
};
struct Pb2IfRecord {                   // 20 bytes; dispatcher sub_428160 (type switch 1..28, 50 = same set as GOF1)
 unsigned char type; unsigned char pad[3]; int param[4];
};
struct Pb2EfRecord {                   // 12 bytes; dispatcher PB_ObjRunFrameEntryEffects: type 1..9, 30, 50
 unsigned char type; unsigned char subType; unsigned char rest[10];
};
