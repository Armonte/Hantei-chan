# HA6 Format Support Comparison: Hantei-chan vs sosfiro_hantei

**Analysis Date:** 2025-10-20  
**Goal:** Identify missing HA6 parameters in our fork compared to sosfiro's reference implementation

---

## Executive Summary

**Hantei-chan** currently lacks support for several **UNI/Dengeki-specific** parameters that sosfiro_hantei implements. Our fork is optimized for MBAACC compatibility, while sosfiro targets UNI/Dengeki. To support the full HA6 format, we need to add the missing UNI parameters while maintaining our MBAACC support.

**Critical Missing Features:**
- UNI Frame IDs and Parameters (AFID, AFPA, AFJH)
- UNI Advanced Damage System (ATV2, ATHH, ATAM, ATC0, ATSA, ATSH)
- UNI Counter/Cancel Flags (ASCF)

---

## Animation Frame (AF) Parameters

### ✅ Both Forks Support
- **AFGP** - MBAACC single-layer graphics
- **AFGX** - UNI multi-layer graphics  
- **AFOF** - Layer offset (X, Y)
- **AFD[X]/AFDL** - Frame duration
- **AFY[X]** - Deprecated offset format
- **AFF[X]/AFFE** - Animation type and flags
- **AFAL** - Blend mode and alpha
- **AFRG** - RGB color tint
- **AFAZ/AFAY/AFAX** - XYZ rotation
- **AFZM** - Scale (X, Y)
- **AFJP** - Jump target
- **AFHK** - Interpolation type
- **AFPR** - Frame priority
- **AFCT** - Loop count
- **AFLP** - Loop end frame
- **AFJC** - Landing jump
- **AFTN** - Flip flags (deprecated)
- **AFPL** - Layer priority (UNI)
- **AFRT** - Rotation/scale interaction flag

### ❌ Missing in Hantei-chan (sosfiro has these)

| Tag | Type | Description | Game | Priority |
|-----|------|-------------|------|----------|
| **AFID** | 1 int32 | Frame ID for code reference | UNI | HIGH |
| **AFPA** | 4 bytes | 4 separate parameter values (0-255 each) | UNI | HIGH |
| **AFJH** | boolean | Unknown UNI jump helper | UNI | MEDIUM |

**Impact:** Cannot properly load/edit UNI character files that use frame IDs or parameters.

**Implementation Notes:**
- Add to `Frame_AF` structure: `int frameId = 0;`, `uint8_t param[4] = {0,0,0,0};`, `bool afjh = false;`
- Add parsing in `fd_frame_AF_load()`
- Add saving in `framedata_save.cpp`

---

## Animation State (AS) Parameters

### ✅ Both Forks Support
- **ASV0** - Vector movement initialization
- **ASVX** - Stop vector movement
- **ASMV** - Can move flag
- **ASS1/ASS2** - Stance state (airborne/crouching)
- **ASCN** - Cancel with normal attacks
- **ASCS** - Cancel with special attacks  
- **ASCT** - Counter hit type
- **AST0** - Sinewave movement (cursed, prefer ASV0)
- **ASMX** - Max X speed
- **ASAA** - Number of hits
- **ASYS** - Invincibility
- **ASF0/ASF1** - Status flags

### ❌ Missing in Hantei-chan (sosfiro has these)

| Tag | Type | Description | Game | Priority |
|-----|------|-------------|------|----------|
| **ASCF** | 1 int32 | Counter hit or cancel related flag | UNI | MEDIUM |

**Impact:** Minor - only affects UNI files that use this flag (rare).

**Implementation Notes:**
- Add to `Frame_AS` structure: `int ascf = 0;`
- Add parsing in `fd_frame_AS_load()`

---

## Attack (AT) Parameters

### ✅ Both Forks Support
- **ATGD** - Guard flags (bitwise)
- **ATF1** - Attack flags (bitwise)
- **ATHE** - Hit effect and sound effect
- **ATKK** - Added effect (lasting visual)
- **ATNG** - Grab/hitgrab flags
- **ATUH** - Extra gravity (float)
- **ATBT** - Break time
- **ATSN** - Hit stop time override
- **ATSU** - Untech time
- **ATSP** - Hit stop (from preset list)
- **ATGN** - Block stop time / Guard stun
- **ATHT** - Correction type (0/1/2)

### ❌ Missing in Hantei-chan (sosfiro has these)

| Tag | Type | Description | Game | Priority |
|-----|------|-------------|------|----------|
| **ATV2** | 14 int32 | UNI vector format (standing/air/crouch HIT+GUARD vectors with flags) | UNI | **CRITICAL** |
| **ATHH** | 1 int32 | Damage proration percentage (95 = 5% reduction) | UNI | HIGH |
| **ATAT** | 1 int32 | Base damage (standalone, not in ATVV) | UNI | HIGH |
| **ATAM** | 1 int32 | Minimum damage percentage | UNI | HIGH |
| **ATC0** | 3 int32 | Hit stun time proration (reduction, combopoint, SMP modifier) | UNI | HIGH |
| **ATSA** | 1 int32 | Add hit stun (player stun time) | UNI | MEDIUM |
| **ATSH** | 1 int32 | Starter correction | UNI | MEDIUM |

### ❌ Missing in sosfiro_hantei (Hantei-chan has these)

| Tag | Type | Description | Game | Priority |
|-----|------|-------------|------|----------|
| **ATVV** | 4 int16 | MBAACC vector format (red_damage, damage, guard_damage, meter_gain as shorts) | MBAACC | CRITICAL |
| **ATHV** | Variable | MBAACC HIT vector (old format, variable length) | MBAACC | CRITICAL |
| **ATGV** | Variable | MBAACC GUARD vector (old format, variable length) | MBAACC | CRITICAL |

**Key Insight:** UNI uses **ATV2** (combined hit+guard vectors), MBAACC uses **ATVV + ATHV + ATGV** (separate formats). sosfiro's parser has ATHV/ATGV code but asserts them as "Melty only" and never executes. Our fork properly handles them.

**Impact:** 
- **CRITICAL:** Cannot load UNI files properly without ATV2 support
- Cannot edit damage proration, min damage, or hit stun decay for UNI characters
- Our MBAACC support is actually BETTER than sosfiro's for ATVV/ATHV/ATGV

**Implementation Notes:**
```cpp
// Add to Frame_AT structure:
int addHitStun = 0;           // ATSA
int starterCorrection = 0;    // ATSH  
int hitStunDecay[3] = {0};    // ATC0
int correction2 = 100;        // ATHH (proration)
int minDamage = 0;            // ATAM

// ATAT handling: currently we use ATVV for damage
// Need to support both formats:
// - MBAACC: ATVV (4 shorts)
// - UNI: ATAT (1 int32) + ATCA (meter) separate
```

---

## Data Structure Comparison

### Frame_AF Differences

#### sosfiro_hantei has:
```cpp
struct Frame_AF_T {
    std::vector<Layer> layers;
    int duration;
    int aniType;
    unsigned int aniFlag;
    int jump;
    int landJump;
    int interpolationType;
    int priority;
    int loopCount;
    int loopEnd;
    bool AFRT;
    bool afjh;          // ⬅️ UNI
    uint8_t param[4];   // ⬅️ UNI (AFPA)
    int frameId;        // ⬅️ UNI (AFID)
};
```

#### Hantei-chan has:
```cpp
struct Frame_AF_T {
    std::vector<Layer> layers;
    int jump = 0;
    int duration = 0;
    int aniType = 0;
    unsigned int aniFlag = 0;
    int landJump = 0;
    int interpolationType = 0;
    int priority = 0;
    int loopCount = 0;
    int loopEnd = 0;
    bool AFRT = false;
    // ❌ Missing: afjh, param[4], frameId
};
```

**Recommendation:** Add the 3 missing UNI fields to our Frame_AF struct.

---

### Frame_AT Differences

#### sosfiro_hantei has:
```cpp
struct Frame_AT {
    unsigned int guard_flags;
    unsigned int otherFlags;
    int hitStun;              // ⬅️ From ATHS (UNI calls this "correction"?)
    int correction_type;      
    int damage;               // ⬅️ From ATAT (UNI standalone)
    int meter_gain;           // ⬅️ From ATCA (UNI standalone)
    int guardVector[3];
    int hitVector[3];
    int gVFlags[3];
    int hVFlags[3];
    int hitEffect;
    int soundEffect;
    int addedEffect;
    unsigned int hitgrab;     // ⬅️ Bitfield (UNI)
    float extraGravity;
    int breakTime;
    int untechTime;
    int hitStopTime;
    int hitStop;
    int blockStopTime;
    // UNI-specific:
    int addHitStun;           // ⬅️ ATSA
    int starterCorrection;    // ⬅️ ATSH
    int hitStunDecay[3];      // ⬅️ ATC0
    int correction2;          // ⬅️ ATHH (damage proration)
    int minDamage;            // ⬅️ ATAM
};
```

#### Hantei-chan has:
```cpp
struct Frame_AT {
    unsigned int guard_flags;
    unsigned int otherFlags;
    int correction;           // ⬅️ From ATHS (MBAACC)
    int correction_type;
    int damage;               // ⬅️ From ATVV (MBAACC)
    int red_damage;           // ⬅️ From ATVV (MBAACC-specific)
    int guard_damage;         // ⬅️ From ATVV (MBAACC-specific)
    int meter_gain;           // ⬅️ From ATVV (MBAACC)
    int guardVector[3];
    int hitVector[3];
    int gVFlags[3];
    int hVFlags[3];
    int hitEffect;
    int soundEffect;
    int addedEffect;
    bool hitgrab;             // ⬅️ Simple bool (MBAACC uses 0 or 1)
    float extraGravity;
    int breakTime;
    int untechTime;
    int hitStopTime;
    int hitStop;
    int blockStopTime;
    // ❌ Missing all UNI-specific damage fields
};
```

**Key Differences:**
1. **hitgrab:** We use `bool`, sosfiro uses `unsigned int` (UNI bitfield)
2. **red_damage/guard_damage:** We have explicit MBAACC fields they don't  
3. **UNI damage system:** We're missing 5 fields for UNI's advanced damage mechanics

**Recommendation:** 
- Keep our MBAACC-specific fields
- Add UNI-specific damage fields
- Change `hitgrab` to `unsigned int` and handle both simple bool (MBAACC) and bitfield (UNI) cases

---

## Tag Parsing Comparison

### Tags We Support That sosfiro Doesn't (MBAACC Advantage)

| Tag | What We Do | What sosfiro Does |
|-----|-----------|-------------------|
| **ATVV** | Properly parse 4 shorts (red_damage, damage, guard_damage, meter_gain) | Asserts "deprecated?" and never uses |
| **ATHV** | Properly parse variable-length HIT vectors | Asserts "Melty only?" but parses |
| **ATGV** | Properly parse variable-length GUARD vectors | Asserts "Melty only?" but parses |
| **AFGP** | Properly parse MBAACC single-layer format | Asserts failure with "AFGP" message |

**Insight:** sosfiro's fork was designed for UNI first, with MBAACC/Melty as an afterthought. Our fork is MBAACC-first.

### Tags sosfiro Supports That We Don't (UNI Gap)

| Tag | What sosfiro Does | What We Do |
|-----|------------------|-----------|
| **ATV2** | Parse 14 int32s for UNI combined vector format | ❌ Don't parse at all |
| **ATHH** | Store damage proration percentage in `correction2` | ❌ Don't parse |
| **ATAT** | Store standalone damage value | ❌ Don't parse (we use ATVV) |
| **ATAM** | Store minimum damage | ❌ Don't parse |
| **ATC0** | Store 3 int32s for hit stun decay | ❌ Don't parse |
| **ATSA** | Store add hit stun | ❌ Don't parse |
| **ATSH** | Store starter correction | ❌ Don't parse |
| **AFID** | Store frame ID | ❌ Don't parse |
| **AFPA** | Store 4-byte param array | ❌ Don't parse |
| **AFJH** | Store UNI jump helper bool | ❌ Don't parse |
| **ASCF** | Store UNI counter/cancel flag | ❌ Don't parse |

---

## Sequence-Level Parameters

### ✅ Both Support
- **PTCN** - Code name (unused in MBAACC)
- **PTT2** - Display name
- **PTIT** - Fixed-length name (deprecated)
- **PSTS** - Skill type classification
- **PLVL** - Skill level (rebeat)
- **PFLG** - Flag (always 1)
- **PDST** - Old allocation format (G_CHAOS only)
- **PDS2** - Allocation header (8 int32s)

### ⚠️ Difference
| Tag | Hantei-chan | sosfiro_hantei |
|-----|------------|----------------|
| **PUPS** | ❌ Don't parse | ✅ Parse and store palette index |

**Impact:** Cannot load UNI files that use palette switching. MBAACC doesn't use PUPS.

**Recommendation:** Add PUPS support:
```cpp
// In Sequence_T structure:
int pups = 0;  // Palette index (0 = default)

// In fd_sequence_load():
} else if (!memcmp(buf, "PUPS", 4)) {
    pups = *data;
    ++data;
}
```

---

## Implementation Priority

### 🔴 CRITICAL (Blocks UNI file loading)
1. **ATV2** - UNI vector format (AT)
2. **ATAT** - Standalone damage (AT)  
3. **ATCA** handling when not in ATVV context (AT)

### 🟡 HIGH (Affects gameplay/editing accuracy)
4. **AFID** - Frame IDs (AF)
5. **AFPA** - Frame parameters (AF)
6. **ATHH** - Damage proration (AT)
7. **ATAM** - Min damage (AT)
8. **ATC0** - Hit stun decay (AT)
9. **PUPS** - Palette switching (Sequence)

### 🟢 MEDIUM (Nice to have for full UNI support)
10. **ATSA** - Add hit stun (AT)
11. **ATSH** - Starter correction (AT)
12. **AFJH** - Jump helper (AF)
13. **ASCF** - Counter/cancel flag (AS)

### 🔵 LOW (Can defer)
14. Better error handling for unknown tags
15. Cross-allocator template improvements

---

## Recommended Implementation Plan

### Phase 1: Data Structures (Non-Breaking)
```cpp
// Frame_AF additions:
bool afjh = false;
uint8_t param[4] = {0,0,0,0};
int frameId = 0;

// Frame_AS additions:
int ascf = 0;

// Frame_AT additions:
int addHitStun = 0;
int starterCorrection = 0;
int hitStunDecay[3] = {0,0,0};
int correction2 = 100;
int minDamage = 0;
// Change: bool hitgrab → unsigned int hitgrab

// Sequence additions:
int pups = 0;
```

### Phase 2: Loading (framedata_load.cpp)
Add parsing for all missing tags listed above. Use sosfiro's code as reference but adapt for our structure.

### Phase 3: Saving (framedata_save.cpp)
Add writing for all new tags. Need to detect UNI vs MBAACC format:
- If `usedAFGX` flag is set → write UNI format (ATV2, ATAT, etc.)
- Else → write MBAACC format (ATVV, ATHV, ATGV)

### Phase 4: Testing
1. Load UNI character files
2. Load MBAACC character files  
3. Round-trip test (load → save → load → compare)
4. Verify no data loss

---

## Format Detection Strategy

We should detect which format a file uses and preserve it on save:

```cpp
// In Sequence_T:
bool usedAFGX = false;  // Already exists
bool usedATV2 = false;  // New: track if UNI attack vectors used

// During load:
if (ATV2 detected) seq->usedATV2 = true;
if (ATVV detected) seq->usedATV2 = false;

// During save:
if (seq->usedATV2) {
    // Write UNI format: ATV2, ATAT, ATHH, etc.
} else {
    // Write MBAACC format: ATVV, ATHV, ATGV
}
```

---

## Notes on Compatibility

### Tags Both Parsers Assert/Skip
- **AFTN** - Deprecated rotation flip (both parse but commented as deprecated)
- **AST0** - Cursed preset vector (both parse with warnings)
- **ATVV** - sosfiro asserts as deprecated, we handle properly

### Tags Neither Parser Supports
According to HA6_Parameters.md, these exist but neither fork handles:
- Many AT tags: ATBC, ATRF, ATAB, ATGS, ATKZ
- Many AS tags: ASV1, ASVA, ASVC, ASDF, ASCL, ASSS, ASKV, ASSE
- These are marked as "unused" or "unknown" in the docs

**Decision:** Skip these for now. Focus on known, documented tags that are actually used in game files.

---

## Code Quality Observations

### Hantei-chan Advantages
1. ✅ Better template support with cross-allocator assignment operators
2. ✅ Proper MBAACC format handling (ATVV, ATHV, ATGV, AFGP)
3. ✅ More explicit field names (`red_damage`, `guard_damage`)
4. ✅ Better initialization (default values in declarations)
5. ✅ Modified/usedAFGX tracking for selective saves

### sosfiro_hantei Advantages
1. ✅ Complete UNI format support
2. ✅ More extensive tag parsing (12 additional tags)
3. ✅ Better documentation in comments
4. ✅ PUPS palette switching support

### Neutral Differences
- **hitgrab:** We use bool (simpler), they use unsigned int (more flexible)
- **Naming:** We use `correction`, they use `hitStun` for same field (ATHS tag)
- **Error handling:** We silently skip unknown AF tags, they print warnings

---

## Files to Modify

1. **src/framedata.h** - Add missing struct fields
2. **src/framedata_load.cpp** - Add tag parsing for UNI parameters
3. **src/framedata_save.cpp** - Add tag writing with format detection
4. **src/framestate.cpp** - Update any code using Frame_AT hitgrab (bool → unsigned int)

---

## Estimated Effort

- **Data Structure Changes:** 1-2 hours
- **Loading Implementation:** 4-6 hours (12 new tags)
- **Saving Implementation:** 3-4 hours (format detection + new tags)
- **Testing:** 2-3 hours (UNI + MBAACC files)
- **Total:** ~10-15 hours

---

## Open Questions

1. **ATHS vs correction2 vs hitStun:** sosfiro uses `hitStun` for field that stores ATHS, but also has `correction2` for ATHH. Need to clarify naming.

2. **ATAT placement:** When using ATAT (UNI), do we still need separate red_damage/guard_damage fields? Or does UNI handle this differently?

3. **ATNG bitfield:** What do the bits beyond bit 0 mean in UNI? Need to document.

4. **AFPA use cases:** What does character script code use these 4 parameter bytes for? Need examples from UNI files.

5. **Format mixing:** Can a file use both ATVV and ATV2? Or are they mutually exclusive? (Likely exclusive)

---

## Conclusion

**Hantei-chan** is a **solid MBAACC-focused implementation** with better handling of MBAACC-specific formats than sosfiro's fork. However, we are **missing critical UNI support** for:

1. Frame IDs and parameters (AFID, AFPA)
2. Advanced damage system (ATV2, ATHH, ATC0, ATAM, ATSA, ATSH)
3. Palette switching (PUPS)

**To achieve full HA6 format support**, we need to add ~12 UNI-specific tags while maintaining our superior MBAACC compatibility. The implementation is straightforward - mostly adding struct fields and tag parsing - and should take ~10-15 hours total.

**Strategic Advantage:** By combining our strong MBAACC support with sosfiro's UNI knowledge, we can create the most complete HA6 editor supporting both game formats.

