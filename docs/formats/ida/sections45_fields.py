"""Field maps for HAN2RBO pattern-area sections 4 and 5 and the RBO AT-flag bit results.
Evidence: docs/formats/ida/sections45_ex.md.  NOTHING in rbo.exe / rbo_ex1.exe / rbo_ex2.exe / rbo_ex3.exe reads sections 4 or 5 (proved by
sections45_scan2.py), so every name below is derived from data (all 346 RBO .DAT/.DT2 files, 3115 section-4 and 2522 section-5 records),
not from a consumer.  Confidence: D = data-derived (exact invariant or value statistics), Z = zero in every record AND no reader.
Format of each entry: (offset, size, 'c type', 'name', 'comment')."""

# ---------------------------------------------------------------------------------------------------------------------------
# Section 4: 28-byte records, one per frame "sousai" box slot (frame +0xD0 = box count, running count over frames in order).
# Exact invariant: sum(frame[+0xD0] over all frames) == section_size/28 in 346/346 files.
# ---------------------------------------------------------------------------------------------------------------------------
SEC4 = [
    (0x00, 4, 'int32_t', 'event_type',
     'D: record kind. Observed {1:394, 2:753, 3:473, 6:64, 7:28, 9:263, 10:251, 11:228, 12:20, 13:56, 14:237, 15:69, 18:54, 40:5, 249:208, 255:12}; '
     '249 = "none" record (188/208 have all params 0; same id as the unlinked type in g_TransitionEventCategory/g_FrameEnterEventCategory).'),
    (0x04, 4, 'int32_t', 'param0',
     'D: first parameter, 0..512, meaning depends on event_type. Per type (min..max): 1:0..255, 2:0..1, 3:2..19, 6:255 only, 7:13..255, 9:0..10, 10:0..44, '
     '11:61..98 (pattern-id like), 12:2..10, 13:10..91, 14:4..27, 15:5..26, 18:0..4, 249:0..90, 255:256..512.'),
    (0x08, 4, 'int32_t', 'param1',
     'D: second parameter, 0..10011 (type 1 holds CG-image-id style values 10008/10011 = 10000+n like frame spriteId). Per type: 2:0..1, 3:0..5, 6:1..3, 7:0..2, '
     '11:0..1, 12:5..120, 13:0..1, 14:0..2, 15:0..2, 18:36..256 (38 in 42 records), 255:256 only; 9/10/40: always 0.'),
    (0x0C, 4, 'int32_t', 'param2',
     'D: third parameter, 0..75. Per type: 1:0..71, 2:0..1, 3:0..1, 6:5..10, 7:65..75, 11:0..5, 13:0..6, 14:0..3, 15:1..2, 249:0..4, 255:1..30; others 0.'),
    (0x10, 4, 'int32_t', 'param3',
     'D: fourth parameter, only {0:2948, 3:114, 10:47, 1:6}: type 1:0..3, 6:1..3, 7:3, 14:0..3, 15:0 or 10, 249:0..3; others always 0.'),
    (0x14, 4, 'int32_t', 'unused_14', 'Z: 0 in 3115/3115 records; no reader in any of the 4 executables.'),
    (0x18, 4, 'int32_t', 'unused_18', 'Z: 0 in 3115/3115 records; no reader in any of the 4 executables.'),
]

# ---------------------------------------------------------------------------------------------------------------------------
# Section 5: 20-byte records, one per frame "tobi" box slot (frame +0xE0 = box count). sum(frame[+0xE0]) == section_size/20 in 346/346 files.
# dword0 is {u8,u8,s16}: byte2/3 of the dword are a signed 16-bit value (byte3 is 0,1,2,3,4 or 0xFC..0xFF = negative).
# ---------------------------------------------------------------------------------------------------------------------------
SEC5 = [
    (0x00, 1, 'uint8_t', 'event_type',
     'D: record kind {1:826, 3:685, 249:356, 6:319, 4:252, 9:46, 2:29, 254:4, 235:3, 250:2}; 249 = "none" (all fields 0 except event_id 0/1 in a few).'),
    (0x01, 1, 'uint8_t', 'event_id',
     'D: 0..126, 53 distinct. Per type: 1:55..126, 2:1..4, 3:0..24, 4:23..27, 6:0..22, 9:0..3, 250:11, 254:0.'),
    (0x02, 2, 'int16_t', 'value0',
     'D: signed. Per type (min..max): 1:-200..4155, 2:-1..1200, 3:0..702 (320 in 113 records), 4:195..736, 6:-800..500, 9:2..151, 254:5.'),
    (0x04, 2, 'int16_t', 'value1',
     'D: signed. Per type: 1:-184..500, 2:0..325, 3:0..3338 (450 typical), 4:98..567, 6:-500..600, 9:0..5, 254:130.'),
    (0x06, 2, 'int16_t', 'value2', 'D: 0..309, 16 distinct, 0 in 1980/2522 (type 1:0..64, 3:0..30, 6:0..30, 2:0..309).'),
    (0x08, 2, 'int16_t', 'value3', 'D: 0..445, 13 distinct, 0 in 2281/2522 (type 3:0..50, 6:0..30, 2:0..445, 1:0..4).'),
    (0x0A, 2, 'int16_t', 'value4', 'D: 0..15, 8 distinct, 0 in 2296/2522 (type 3:0..15, 4:0..10, 6:0..10, 1:0..3).'),
    (0x0C, 4, 'int32_t', 'unused_0C', 'Z: 0 in 2522/2522 records; no reader in any of the 4 executables.'),
    (0x10, 4, 'int32_t', 'unused_10', 'Z: 0 in 2522/2522 records; no reader in any of the 4 executables.'),
]

# ---------------------------------------------------------------------------------------------------------------------------
# AT record (RboAtRecord, 120 B) flag bits that rbo.exe (base) never reads. None = no reader in ANY of the four executables.
# Results of sections45_at_scan2.py over rbo.exe, rbo_ex1.exe, rbo_ex2.exe, rbo_ex3.exe.
# ---------------------------------------------------------------------------------------------------------------------------
AT_BITS = {
    'RboAtFlags76': {   # AT +0x4C target_fx_flags
        0x2: None,
        0x80: 'SELF_ONLY_TARGET',    # Ex1/Ex2/Ex3 only: Hit_TeamTargetTest returns (attacker == victim); never read by rbo.exe
        0x100: None,
        0x2000: None,
        0x100000: None,              # 439 records in data, still no reader
    },
    'RboAtFlags80': {   # AT +0x50 damage_rule_flags
        0x1: None,
        0x10: None,
        0x20: None,
        0x200: None,
        0x2000: 'UNEVADABLE',        # Ex1/Ex2/Ex3 only: Hit_RollEvade returns 0 (no evade roll); never read by rbo.exe
        0x20000000: None,
        0x40000000: None,
    },
}

# Bits that ARE read (already in rbo_at_record.md), listed per executable set for completeness.
AT_BITS_KNOWN = {
    'RboAtFlags76': {0x1: 'PLAY_EXTRA_SFX', 0x10: 'HIT_TEAMMATES', 0x20: 'CAN_HIT_SELF', 0x40: 'HIT_BOTH_TEAMS',
                     0x80: 'SELF_ONLY_TARGET (Ex only)', 0x400: 'PUSHBACK_GATE_INVERT', 0x10000: 'SUPPRESS_HIT_SFX'},
    'RboAtFlags80': {0x2: 'NON_LETHAL', 0x1000: 'NO_JUST_GUARD', 0x2000: 'UNEVADABLE (Ex only)', 0x1000000: 'NO_DAMAGE_POPUP'},
}

# AT fields with no reader in any of the four executables (offset, size, name).
AT_UNREAD_FIELDS = [(0x14, 4, 'unused_14'), (0x24, 4, 'reserved_flag_24'), (0x60, 4, 'unused_60'), (0x64, 4, 'unused_64'),
                    (0x68, 4, 'unused_68'), (0x74, 4, 'unused_74')]

if __name__ == '__main__':
    for nm, tab, sz in (('SEC4', SEC4, 28), ('SEC5', SEC5, 20)):
        cov = [0] * sz
        for off, n, ct, name, cm in tab:
            for i in range(off, off + n): cov[i] += 1
        assert all(c == 1 for c in cov), (nm, cov)
        print(nm, 'covers', sz, 'bytes,', len(tab), 'fields')
