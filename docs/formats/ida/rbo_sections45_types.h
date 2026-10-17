// Pattern-area sections 4 and 5 (RBO). NO reader in rbo.exe or the Ex exes (docs/formats/ida/sections45_ex.md); names are data-derived.
struct RboSection4Record {   // 28 bytes, one per sousai box slot (frame +0xD0 counts)
 int event_type; // +0x0 D: record kind.
 int param0; // +0x4 D: first parameter, 0..512, meaning depends on event_type
 int param1; // +0x8 D: second parameter, 0..10011 (type 1 holds CG-image-id style values 10008/10011 = 10000+n like frame spriteId)
 int param2; // +0xC D: third parameter, 0..75
 int param3; // +0x10 D: fourth parameter, only {0:2948, 3:114, 10:47, 1:6}: type 1:0..3, 6:1..3, 7:3, 14:0..3, 15:0 or 10, 249:0..3; others always 0.
 int unused_14; // +0x14 Z: 0 in 3115/3115 records; no reader in any of the 4 executables.
 int unused_18; // +0x18 Z: 0 in 3115/3115 records; no reader in any of the 4 executables.
};
struct RboSection5Record {   // 20 bytes, one per tobi box slot (frame +0xE0 counts)
 unsigned char event_type; // +0x0 D: record kind {1:826, 3:685, 249:356, 6:319, 4:252, 9:46, 2:29, 254:4, 235:3, 250:2}; 249 = "none" (all fields 0 except event_id 0/1 in a few).
 unsigned char event_id; // +0x1 D: 0..126, 53 distinct
 __int16 value0; // +0x2 D: signed
 __int16 value1; // +0x4 D: signed
 __int16 value2; // +0x6 D: 0..309, 16 distinct, 0 in 1980/2522 (type 1:0..64, 3:0..30, 6:0..30, 2:0..309).
 __int16 value3; // +0x8 D: 0..445, 13 distinct, 0 in 2281/2522 (type 3:0..50, 6:0..30, 2:0..445, 1:0..4).
 __int16 value4; // +0xA D: 0..15, 8 distinct, 0 in 2296/2522 (type 3:0..15, 4:0..10, 6:0..10, 1:0..3).
 int unused_0C; // +0xC Z: 0 in 2522/2522 records; no reader in any of the 4 executables.
 int unused_10; // +0x10 Z: 0 in 2522/2522 records; no reader in any of the 4 executables.
};
