# Pattern-area sections 4 and 5, and the unread AT flag bits, across rbo.exe and the three Ex executables

Scope: `rbo.exe` (IDB instance fabfe2d0c6b3 at the time of writing; re-opened as aaeb7cdf8cad), `rbo_ex1.exe`, `rbo_ex2.exe`, `rbo_ex3.exe`.
All four are 32-bit x86; Hex-Rays offsets in this document are the DISASSEMBLY (hex) offsets unless written in decimal with a leading `+`.
Tooling (kept in this directory, rerunnable): `sections45_scan2.py` (section reader scan), `sections45_at_scan2.py` (AT reader scan).
Field tables: `sections45_fields.py` (`SEC4`, `SEC5`, `AT_BITS`).

## 0. Result in one paragraph

* Sections 4 and 5: **no reader exists in any of the four executables.** The only code that touches the section 4/5 pointer slots is
  `Han2Dat_ComputeSectionPointers` (writer) and `Han2Dat_DecryptPatternSections` (XOR only if the header XOR flag is set; no shipped file sets it).
  Data shows what indexes them: **not an index field**, but a running count: `sum(frame[+0xD0]) * 28 == section 4 size` and
  `sum(frame[+0xE0]) * 20 == section 5 size` in 346/346 files. (This corrects `rbo_scripts_pat.md` 1.2, which called +0xD0/+0xE0 1-based indices.)
* AT flag bits: the three Ex executables read exactly **two** bits that rbo.exe does not: `RboAtFlags76 0x80` (SELF_ONLY_TARGET) and
  `RboAtFlags80 0x2000` (UNEVADABLE). All other listed bits (4 + 6) and all fields `unused_14`, `reserved_flag_24`, `unused_60/64/68/74` are
  read by **no** executable.

## 1. Runtime layout recovered per executable (needed to make the scans meaningful)

`RboPatternAreaView` (9 dwords: base, patterns, frames, boxes, attackRecords, section4 (+0x14), section5 (+0x18), scriptListsA, scriptListsB) lives at
CharData+0x1C; the actor holds that pointer. Slot size of `g_CharData` is 0x734C in all four exes (`imul 734Ch` in the slot getter).

| | rbo.exe | rbo_ex1 | rbo_ex2 | rbo_ex3 |
|---|---|---|---|---|
| `Han2Dat_ComputeSectionPointers` (writes view +4..+0x20) | 0x403550 | 0x405BF0 | 0x405C10 | 0x405C10 |
| `Han2Dat_DecryptPatternSections` | 0x403480 | 0x405B20 | 0x405B40 | 0x405B40 |
| `Xor_StringKeyedInPlace` (decrypt worker) | 0x422970 | 0x4257A0 | 0x425C20 | 0x425C20 |
| `g_CharData` / `CharData_GetSlot` | 0x2433188 / 0x43D470 | 0x244B760 / 0x442EC0 | 0x22CE3B0 / 0x4444D0 | 0x22EBDA0 / 0x444C70 |
| actor -> CharData ptr / patternArea ptr | +0x7EC / +0x7F0 | +0x7F8 / +0x7FC | +0x808 / +0x80C | +0x808 / +0x80C |
| actor -> AT ptr / attack state | +0x750 / +0x714 | +0x75C / +0x720 | +0x76C / +0x72C | +0x76C / +0x72C |
| AT ptr offset inside attack state | +0x3C | +0x3C | +0x40 | +0x40 |
| AT stride (`lea x3, x5, x8` in `Actor_CollectAttackBoxes2`) | 120 | 120 | 120 | 120 |

The Ex ComputeSectionPointers were found by shape (stores to view +4..+0x20 conditioned on header dwords +0x2C..+0x48, `add eax,58h`); each exe has exactly one
(the 0x421B40/0x4248B0/0x424CD0/0x424CD0 second matches of my first signature are unrelated `SetEnemyParam` script helpers). Single caller chain in all four:
`CharData_LoadDatAndFob -> Han2Dat_Load -> Han2Dat_LoadPatternSectionPreferDt2 -> Han2Dat_LoadPatternAreaAndResolve -> ComputeSectionPointers`.
Only one store of CharData+0x1C into an actor exists per exe (`Actor_Init`: rbo `[ebx+7F0h]`, ex1 `sub_447670 [ebx+7FCh]`, ex2/ex3 `[ebx+80Ch]`).

## 2. Task 1: readers of sections 4 and 5

### 2.1 Scan description (`sections45_scan2.py`)

Flow-sensitive forward taint (block-level join, per-block visit cap 6) over EVERY function in each exe (rbo 2989, ex1 3212, ex2 3271, ex3 3284 functions;
0 analysis errors in all four), followed by queue-based interprocedural passes for every direct call that receives a tainted stack/ECX/EDX argument.
Taint elements: pointer to a CharData slot (+offset), pointer into `g_CharData` (slot unknown, offset taken modulo 0x734C), pointer to section n (n = (CharData off - 0x20)/4),
and the view base pointer (`[view+0]`). Sources: `mov r,[x+patternAreaOff]`, `mov r,[x+charDataOff]`, return of `CharData_GetSlot`, immediate / `lea` / `add` forms of `g_CharData`.
Recorded as hits: any load of a section pointer slot (view +4..+0x20 = CharData +0x20..0x3C), any dereference of a section or base pointer, absolute `g_CharData+0x1C..0x3F`
operands, indexed accesses near the slots, stores of tainted pointers to memory (escapes, incl. `movs`/`rep`), and tainted arguments to indirect calls.

Positive control (the same scan, every exe): sections 0..3, 6, 7 are found exactly in their known consumers, so the scan works:

| Section | rbo.exe consumers (the Ex exes have the identical set at the shifted addresses, see section 5) |
|---|---|
| 0 | `Actor_GetFrameRecord`, `Actor_CacheCurrentFrame`, `Actor_GetFrameSpriteId` |
| 1 | `Actor_GetFrameRecord`, `Actor_GetFrameSpriteId`, `Actor_FindFlowTargetFrame`, `Actor_StartKnockbackMotion`, (+ Cache/ResolveNextFrame stores) |
| 2 | `sub_41EE10`, `BoxRect_ApplyFacing`, `Actor_CollectAttackBoxes2`, `Actor_CollectHurtBoxes3`, `Actor_CollectBoxes2_272`, `Actor_PickBoxRect_244` |
| 3 | `Actor_CollectAttackBoxes2` only (`attackRecords + 120*frame[+0xC8]` stored at actor+AT_OFF) |
| 4 | **`Han2Dat_ComputeSectionPointers` (store), `Han2Dat_DecryptPatternSections` (load, XOR decrypt call). Nothing else.** |
| 5 | same as 4 |
| 6 / 7 | `Actor_RunFrameScriptList6` / `Actor_RunFrameScriptList7` |

Results for the target slots, identical in all four exes:
* `SEC4_PTR_ACCESS`: 3 hits = {ComputeSectionPointers (2 stores: zero and the computed pointer), DecryptPatternSections (`mov ecx,[edi+14h]`)}; `SEC5_PTR_ACCESS`: same 3 with `+18h`.
* `DEREF_S4` / `DEREF_S5`: only in `Xor_StringKeyedInPlace`, the XOR worker (called from the decrypt function; XOR flag header dword +0x1C is 0 in every shipped file).
* View base pointer (`[view+0]`) loads / dereferences: only `Han2Dat_UpdateLoadedFlag`, `GlobalBlock_FreeIfNull`, `Han2Dat_LoadPatternAreaAndResolve`, `File_LoadRangeAllocEx`,
  `Han2Dat_DecryptPatternSections`, `Xor_StringKeyedInPlace`, and the PAC entry reader (`Pac_ReadEntryXorDecrypt`): no code can reach sections 4/5 by `base + 0x58 + sum(sizes)` arithmetic.
* Absolute `g_CharData+0x1C..0x3F` operands: 0. `movs`/`rep` copies of a tainted pointer: 0. Stores of the view/CharData pointer to memory other than the actor init: 0
  (the only escapes are the actor-init stores and the known frame/box/AT cache stores, each listed per exe in the scan output).
* Tainted arguments to indirect calls that carry section/base pointers: only `GlobalFree` and `ReadFile` (loader/free code).
* Residual: the pointers could in theory be copied by a struct memcpy of the whole 0x734C slot; the scan has zero tainted `memcpy`-like calls and zero `movs`.

Conclusion: **proved by exhaustive scan of all four exes: no reader of pattern-area sections 4 and 5.** They are loaded, optionally XOR-decrypted and freed, never interpreted.

### 2.2 Data evidence: what could index them

(All 346 RBO HAN2RBO files (215 .DAT + 131 .DT2), frame stride 300; `han2lib.py`.)

* Section sizes: 3115 section-4 records (28 B, 162 files have any), 2522 section-5 records (20 B, 144 files), no remainder in any file.
* Candidate search over EVERY frame field at 1/2/4-byte granularity and every AT (section 3) field at 1/2/4-byte granularity: a field qualifies as an index only if
  all its values are `<= record count` in every file and non-zero somewhere. Qualifying fields: frame +0xD0 (section 4) and +0xE0 (section 5) (plus +0x48 `hitLimitFlags`/byte +0x40,
  small flag values, coincidence) and AT +0x24 (24 records with value 1, 0/1 flag, in player files, none of which owns a sousai/tobi box).
* **Exact invariant**: for each of the 346 files `sum over frames of int32@+0xD0 == section4_size/28` and `sum over frames of int32@+0xE0 == section5_size/20` (0 mismatches).
  Values of +0xD0 are 0..3 (1:2437, 2:324, 3:10 frames) and +0xE0 are 0..4 (1:1497, 2:249, 3:81, 4:71), i.e. they are per-frame group COUNTS (`sousaiBoxCount`, `tobiBoxCount`).
  So record k of section 4 belongs to the k-th sousai box slot (frames in order, slots in order); section 5 likewise for tobi slots. There is no explicit index field: the engine
  lineage that used them would need a running counter. This also explains why files with no section-4 data have +0xD0 == 0 in all frames.
* Box rectangles of those slots live in section 2 (frame +0xD4.. / +0xE4..); the section-4/5 record is therefore per-box metadata (e.g. frame (2,0) of ARGIOPE.DAT has one tobi slot -> section-5 record `{type 6, id 10, 0, 200}`).
* GOF2 files (68 .DT2) have both sections empty while frames keep sousai/tobi boxes, so the successor engine moved or dropped this metadata.

Full field tables (data-derived, with proof grade): `sections45_fields.py` -> `SEC4` (7 fields, 28/28 bytes) and `SEC5` (9 fields, 20/20 bytes).
Named from behaviour in data: SEC4 `event_type`, `param0..3`; SEC5 `event_type`, `event_id`, `value0..4`. Not named from behaviour (no consumer exists): the parameters keep neutral
names; semantic names cannot be proved from any exe. `unused_14/18` (SEC4) and `unused_0C/10` (SEC5): zero in 100% of records AND no reader in four exes.

## 3. Task 2: AT record readers in the Ex exes (`sections45_at_scan2.py`)

Scan: same engine; sources `mov r,[x+AT_OFF]` (AT pointer), `lea/add r, ST_OFF` (attack state), `[state+0x3C/0x40]` (AT inside state); loaded field values stay tainted so that
`test/and/shr/cmp` immediates on `+0x4C`/`+0x50` are recorded (high byte regs `ah/ch` shifted by 8; `not/shr 18h/and 1` forms kept). Every read is recorded as (function, address, field offset);
indexed (VARIDX) accesses, writes and `lea` of AT fields: 0 in every exe. No AT access beyond +0x74 in any exe. The AT pointer is only ever built in `Actor_CollectAttackBoxes2`
(`sec3 + 120*frame[+0xC8]`, no field read there) and only ever reached through actor+AT_OFF / attack-state+0x3C/0x40.

Fields READ (dword granularity) are identical in all four executables: 0x00, 0x04, 0x08, 0x0C, 0x10, 0x18, 0x1C, 0x20, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x6C, 0x70.
**Fields read by nothing (0 reads in rbo.exe, ex1, ex2, ex3): 0x14 `unused_14`, 0x24 `reserved_flag_24`, 0x60 `unused_60`, 0x64 `unused_64`, 0x68 `unused_68`, 0x74 `unused_74`.**
(Note 0x0C `hit_gate_mode` is read in all four, in `Combat_ResolveGuardContacts`.)

Bits tested (immediates recovered by the scan; function addresses in rbo / ex1 / ex2 / ex3):

| Word | Bit | Reader | Executables |
|---|---|---|---|
| +0x4C RboAtFlags76 | 0x1 PLAY_EXTRA_SFX | `Combat_ResolveDamageHit` 0x444530 / 0x44A230 / 0x44BB70 / 0x44C310 | all |
| | 0x10, 0x20, 0x40 | `Hit_TeamTargetTest` 0x443960 / 0x449590 / 0x44AEA0 / 0x44B640 (0x20 also `Combat_ResolveGuardContacts`) | all |
| | **0x80 SELF_ONLY_TARGET** | `Hit_TeamTargetTest` at 0x4495B6 (ex1), 0x44AEC6 (ex2), 0x44B666 (ex3): `test al,80h` -> `return attacker == victim` (other target tests skipped) | **ex1, ex2, ex3 only** |
| | 0x400 | `Hit_PushbackAllowed` 0x4426D0 / 0x4482C0 / 0x449BC0 / 0x44A360 (`test ch,4`) | all |
| | 0x10000 | `Hit_PlayHitSfx`, `Hit_PlayGuardHitSfx` | all |
| | 0x2, 0x100, 0x2000, 0x100000 | none | **no exe** |
| +0x50 RboAtFlags80 | 0x2 NON_LETHAL | `Hit_ApplyHpDamage`, `Hit_SetVictimReactionAnim` (`test [esi+50h], bl` with `bl = 2`) | all |
| | 0x1000 NO_JUST_GUARD | `Hit_ClassifyGuardOutcome` (`test ch,10h`) | all |
| | **0x2000 UNEVADABLE** | `Hit_RollEvade` at 0x448A89 (ex1), 0x44A393 (ex2), 0x44AB33 (ex3): `test ch,20h` -> `return 0` before the evade roll | **ex1, ex2, ex3 only** (rbo.exe's `Hit_RollEvade` 0x442E50 does not read the AT) |
| | 0x1000000 NO_DAMAGE_POPUP | `Hit_ApplyDamageAndSkillMods` (`not; shr 18h; and 1`) | all |
| | 0x1, 0x10, 0x20, 0x200, 0x20000000, 0x40000000 | none | **no exe** |

Data cross-check (8612 AT records): bit counts: flags76 0x100000: 439, 0x2: 47, 0x100: 15, 0x2000: 3, 0x80: 1; flags80 0x2000: 99, 0x40000000: 41, 0x10: 8, 0x20: 8, 0x20000000: 7, 0x1: 5, 0x200: 2.

Residual (cannot be closed from the exes): `Actor_RunConditionScriptList` (rbo `sub_41E810`, called from the box-overlap condition search) hands the AT pointer to the FOB condition script as
script argument 1 (`Script_SetArg(1, actor+AT_OFF)`, case 4 of its switch). Any read of AT fields through that pointer happens in FOB bytecode via script commands; the executables themselves do not.
No exe code derefs it (the scan has no AT reads in script command handlers). **Closed by `fob_vm.md`**: the FOB corpus was disassembled and abstractly interpreted; the only script that receives the AT pointer (ACOLYTE_F/M `idx2[9]`) passes it to `Actor_Query` (query 63), which reads `AT+0x4C & 0x100000` only. All other unread bits/fields are proved unread by scripts too.

## 4. Renamed functions (Ex IDBs only; rbo.exe untouched)

Method: exact mnemonic-sequence identity (>= 12 instructions, unique in both binaries) with the already-named rbo.exe function, restricted to the families
`Han2Dat_/CharData_/Hit_/Combat_/Actor_/BoxRect_/Rect_/Xor_/File_LoadRange/GlobalBlock_/Script_Resolve/CgBlock_/CgEffect_`; manual by address order for the pairs whose
mnemonic sequences are identical in rbo.exe (`Actor_CollectHurtBoxes3` vs `Actor_CollectBoxes2_272`, distinguished by their store displacement, base 0x774 resp. 0x7DC + the exe shift);
and the three rbo.exe functions that were still sub_: `sub_41EE10` -> `Actor_FindBoxOverlapPassingConditions` (box overlap loop using `Actor_GetKasanariBoxWorldRect`,
`Rect_Intersects` and the script condition list), `sub_41E810` -> `Actor_RunConditionScriptList`, `sub_422C00` -> `Pac_ReadEntryXorDecrypt` (seek+read of a PAC entry with the key-string XOR for the first 0x2173 bytes).
Corrected wrong prior guess in ex3: `Spawn_ClampChildToWall` 0x44A3A0 -> `Hit_ApplyWallPushback` (same body as rbo 0x442710: runs the SetYAraRemoved / SetGuardMoved script then `Velocity_Init`).

| IDB (instance id) | functions renamed |
|---|---|
| rbo_ex1.exe (eec53cb464d8) | 152 (150 mapped + `Actor_RunConditionScriptList` 0x421450 + `Actor_GetKasanariBoxWorldRect` 0x446910; the 150 include `Actor_FindBoxOverlapPassingConditions` 0x421A50, `Pac_ReadEntryXorDecrypt` 0x425A30, `Script_ResolveIndexedFunction` 0x427FE0, `Actor_CollectHurtBoxes3` 0x446CE0, `Actor_CollectBoxes2_272` 0x446DA0) |
| rbo_ex2.exe (2254bc71b1f3) | 146 (144 + 0x421740 `Actor_RunConditionScriptList`, 0x4481C0 `Actor_GetKasanariBoxWorldRect`; `Actor_FindBoxOverlapPassingConditions` 0x421D50, `Pac_ReadEntryXorDecrypt` 0x425EB0, `Script_ResolveIndexedFunction` 0x428500, `Actor_CollectHurtBoxes3` 0x448590, `Actor_CollectBoxes2_272` 0x448650) |
| rbo_ex3.exe (be700e734dba, was 48ae28ac9a0e) | 142 (139 + 0x421780 `Actor_RunConditionScriptList`, 0x448960 `Actor_GetKasanariBoxWorldRect`, and the `Hit_ApplyWallPushback` correction; `Actor_FindBoxOverlapPassingConditions` 0x421D90, `Pac_ReadEntryXorDecrypt` 0x425EB0, `Script_ResolveIndexedFunction` 0x428500, `Actor_CollectHurtBoxes3` 0x448D30, `Actor_CollectBoxes2_272` 0x448DF0) |

Section consumers per exe (name: rbo / ex1 / ex2 / ex3):
`Han2Dat_LoadPatternAreaAndResolve` 0x403630 / 0x405CD0 / 0x405CF0 / 0x405CF0; `Actor_GetFrameRecord` 0x4408F0 / 0x4464B0 / 0x447D60 / 0x448500; `Actor_CacheCurrentFrame` 0x440920 / 0x4464E0 / 0x447D90 / 0x448530;
`Actor_GetFrameSpriteId` 0x440B10 / 0x4466D0 / 0x447F80 / 0x448720; `BoxRect_ApplyFacing` 0x440BB0 / 0x446770 / 0x448020 / 0x4487C0; `Actor_CollectAttackBoxes2` 0x440F40 / 0x446B00 / 0x4483B0 / 0x448B50;
`Actor_PickBoxRect_244` 0x442190 / 0x447D80 / 0x449680 / 0x449E20; `Actor_RunFrameScriptList6` 0x41DA50 / 0x420660 / 0x420950 / 0x420990; `Actor_RunFrameScriptList7` 0x41F900 / 0x422680 / 0x422A00 / 0x422A40.
AT consumers: see the table in section 3 plus `Hit_ApplyHpDamage` 0x443CF0 / 0x449950 / 0x44B260 / 0x44BA00, `Hit_ApplyDamageAndSkillMods` 0x443DA0 / 0x4499D0 / 0x44B2E0 / 0x44BA80,
`Hit_ClassifyGuardOutcome` 0x443330 / 0x448F30 / 0x44A840 / 0x44AFE0, `Combat_ResolveGuardContacts` 0x4433E0 / 0x448FE0 / 0x44A8F0 / 0x44B090, `Hit_SetVictimReactionAnim` 0x444340 / 0x44A020 / 0x44B960 / 0x44C100,
`Hit_RollEvade` (Ex-only AT-aware) 0x448A40 / 0x44A340 / 0x44AAE0 (ex1/ex2/ex3).
Instruction comments added in ex1 (0x448A89, 0x4495B6) and ex2 (0x44A393, 0x44AEC6) for the two Ex-only bits (ex3 already had them). No database was saved.

## 5. Counts

* Sections 4/5: readers found 0 (4/4 exes proved); record fields: SEC4 7 (5 named from data, 2 `unused` with proof), SEC5 9 (7 named from data, 2 `unused` with proof); 0 unresolved bytes.
  The 12 data-named fields are inferred from values only (no consumer exists), so their behaviour is not provable; the byte layout is.
* AT: 24 of 30 dword fields read in all four exes; 6 fields read nowhere (0x14, 0x24, 0x60, 0x64, 0x68, 0x74); flag bits: 2 newly named (Flags76 0x80, Flags80 0x2000), 9 remain unread (4 + 5).
