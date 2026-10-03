# GOF2 pattern-area sections 3..8, PAT v4 parts, CHP bank and the file lookup (GOF2.exe.i64)

Companion of `gof2_frame.md` (frame record, container, sections 0..2). Types: `ida/gof2_at_sections_types.h` (loaded into `GOF2.exe.i64` with `idc.parse_decls`, sizes verified in the IDB:
Gof2AtRecord 0xEC, Gof2Section4Record 0x1C, Gof2Section5Record 0x14, Gof2ScriptListEntry 0x14, Gof2EffectSpawnRecord 0x60, Gof2PatPart 0x5C, Gof2PatFileHead 0x1195C; machine check:
`python3 tools/gof2/check_gof2_types.py docs/formats/ida/gof2_at_sections_types.h`, 0 mismatches). Scans: `tools/gof2/at_sections_scan.py` (section value statistics), `tools/gof2/pat_part_scan.py` (PAT parts),
`tools/gof2/ida_at_record_scan.py` (IDAPython reader scan, reproduces the read-offset table below).

Evidence tags (same as `gof2_frame.md`): **T** reader traced and meaning proven by its behaviour, **I** reader traced but meaning inferred, **E** editor-only (nonzero in data, no reader), **U** unused (zero in every record, no reader).

## 0. Correction to the file counts in `gof2_frame.md`

`han2_files('gof')` yields every archive copy: data02.dat and data05.dat both hold 23 of the same `.DT2` names (data05 is the 2010 patch archive), so "68 files / 43 991 frames" counts those 23 twice.
The engine searches the archives from slot 5 down to slot 0 (section 5 below), so the **effective** set is 45 distinct `.DT2` (25 from data05, 20 from data02; KATSUKO is data05 only). The scans in this file default to that
effective set (`--all` gives the old numbers). Constant/zero fields are identical in both sets; only counts differ.

## 1. Section 3: attack record `Gof2AtRecord` (236 bytes)

Present in 15 of the 45 effective files, 997 records (the old doc: 29 files / 1895 records). Record index = frame `+0xC8`, numbered 0,1,2.. in frame order, no sharing.

### 1.1 How the record is reached (the read set)

`CAppHanteiKougeki_BuildFromFrame` 0x449250 stores `view.attackRecords + 236*frame.attackRecordIdx` into `CAppHanteiKougeki+0xC` (the kougeki object is embedded in `Obj` at +0x614, so the record pointer is `Obj+0x620`).
`imul 0xEC` exists at exactly two sites (BuildFromFrame, `ObjScan_StoreAttackGuardInfo` 0x432480); every operand `[reg+620h]` / `[reg+614h]` in .text (27 functions) was read, and the Hex-Rays ctree propagation
(`ida_at_record_scan.py`: variables assigned from `*(x+0x620)` or `*(K+0xC)`, followed into callees when passed as an argument) gives the complete reader table:

| Offset | Functions that read it |
|---|---|
| +0x00 | HitJudge_StartVictimReaction 0x435FA0 (via HitClass_ToStandReactionState 0x435E10), HitJudge_CollectPairs 0x4978B0, HitJudge_ApplyHit 0x4983A0, sub_495890 (damage scaling) |
| +0x04 | HitJudge_ApplyHitStop 0x496CE0 |
| +0x08 | HitJudge_CollectPairs, HitJudge_ApplyHit |
| +0x10 | HitJudge_ApplyHit, HitJudge_ComputeDamage 0x494FF0, sub_4954F0, sub_495B60 |
| +0x1C | HitJudge_ApplyHit |
| +0x20 | HitJudge_ResolveTick 0x4996A0 |
| +0x28 +0x2C +0x30 | HitJudge_RunHitSoundCommand 0x49D1A0 |
| +0x34 | HitJudge_ApplyKnockDirAndFacing 0x435B10 / ...NoFlipCheck 0x435C10 |
| +0x38 | HitJudge_ApplyKnockDirAndFacing |
| +0x4C | HitJudge_ApplyHit, HitJudge_ApplyHitStop, HitJudge_CollectPairs (bit 0x4000) |
| +0x50 | HitJudge_ApplyHit, HitJudge_StartVictimReaction, HitJudge_ApplyKnockDirAndFacing |
| +0x6C (+0x6E word) | HitJudge_ResolveSecondaryBoxClashes 0x49ACB0 (dword), CAppHanteiKougeki_BuildFromFrame (`cmp word [rec+6Eh],0`) |
| +0x78 | HitJudge_ApplyHit, HitJudge_CollectPairs |
| +0x7C (+0x7E) | HitJudge_DefenderGuardsAttack 0x496930, HitJudge_DefenderArmorAbsorbs 0x497180, ObjScan_StoreAttackGuardInfo, HitJudge_ApplyKnockDirAndFacing (stores it in victim obj+1876), sub_43BA00, sub_43C530 (script conditions) |
| +0x80 | HitJudge_ComputeDamage, sub_495890, HitJudge_DefenderGuardsAttack, HitJudge_CollectPairs, HitJudge_ApplyHit, HitJudge_CalcGuardGaugeLoss 0x4977B0, HitJudge_GaugeSlotFromAttackKind 0x494950, sub_497120, sub_43BA00, sub_43C530, sub_49BB70, sub_495B60 |
| +0x84 | HitJudge_ApplyHit |
| +0x90 | HitJudge_CollectPairs, HitJudge_ResolveSecondaryBoxClashes |
| +0xC4 | HitJudge_CollectPairs |

`HitJudge_ComputeDamage` and `sub_4954F0/sub_4952F0` take a 3-pointer struct whose first member is the kougeki pointer (read by hand: +0x10, +0x80). No other function receives or stores the record pointer
(`Obj_ExecCmdRecord` handlers only write `ctx+20`; the other hit helpers take the objects). The pass that runs the hits is `HitJudge_ResolveTick` -> `HitJudge_CollectPairs` (pair list) -> `HitJudge_ApplyHit` per pair.

### 1.2 Field map (byte count by class: **T 56, I 24, E 16, U 140** = 236)

| Off | Field | Tag | Evidence / behaviour |
|---|---|---|---|
| 0x00 | `hitClass` Gof2AtHitClass | T | selects the victim reaction pattern: `HitJudge_StartVictimReaction` maps class x victim stance to pattern 28..39 (table 1.3); `HitClass_ReactionPriority` 0x435D60 ranks classes so a weaker class cannot replace a stronger reaction (`prev = victim+1900`, new rank must be higher); class 17 = no reaction: `sub_495890` skips the combo damage scaling, `HitJudge_ApplyHit` skips combo/hit counters and sparks (`**(a2+1568) != 17`), `HitJudge_CollectPairs` skips the armor test. Data: 0..13, 15, 16, 17 (4 records), 14 never. |
| 0x04 | `hitStopLevel` | T | `HitJudge_ApplyHitStop`: index (0..6) into `g_SysData_HitStopAttackerTable` 0x739708 (frames the attacker freezes), `g_SysData_HitStopVictimTable` 0x73970C (victim), plus `g_SysData_HitStopCounterBonusTable` 0x739718 when the victim is in counter state. |
| 0x08 | `sparkKind` | I | only `== 8` is tested (HitJudge_ApplyHit, HitJudge_CollectPairs): spark kind 2 (`Effect_SpawnHitSpark` 0x49CC60 kind 2 = SystemEffect pattern 3, random rotation); otherwise kind `(attackKind != 0)` (pattern 45 or 0). Data: 0 (538), 1, 8 (81), 2, 11, 6. |
| 0x0C | `unused_0C` | U | zero in all 997 records, no reader (RBO: gate mode). |
| 0x10 | `damage` | T | `HitJudge_ComputeDamage`: `-damage` is the HP delta (then scaled by combo/rule tables); `HitJudge_ApplyHit`: `damage > 0` is added to the side score (rounded to a multiple of 100) and goes to the HUD trace `sub_4A05B0`. 0..1800 in data. |
| 0x14 | `editorOnly_14` | E | 0x64 in 6 records, no reader. |
| 0x18 | `editorOnly_18` | E | 590 of 997 records nonzero (0..800; RBO guard damage), **no reader in GOF2** (chip/guard loss is `HitJudge_CalcGuardGaugeLoss`, driven by damage tables and `attackKind`, not by this field). |
| 0x1C | `drawOrderMode` | I | `HitJudge_ApplyHit` tail: `!= -1` resets the victim link context (obj+520..540: pattern -1, offset 190) and calls `SideDrawOrder_BringToFront`: 0/1 victim in front, 2 attacker in front; `== 1` additionally anchors the victim to the attacker (obj+528 position sync). 0 in 995 records. |
| 0x20 | `hitEffectKind` | I | `HitJudge_ResolveTick`: `0` -> kind derived from attacker flag bits (1,2,4 -> 1,2,3); else 1..5 directly; `HitJudge_StartHitEffect` 0x4317D0 starts the effect objects 50000/50001/50002 (1..3) or the handlers sub_43F450/43EF50 (4/5). Data 0 (939), 1, 3, 5, 4. |
| 0x24 | `editorOnly_24` | E | 1 in 9 records, no reader. |
| 0x28 | `hitSoundBankSelect` | I | `HitJudge_RunHitSoundCommand`: 0 = SystemSE.Fob bank (`g_SystemSeScriptBank` 0x5A3A34), nonzero = the attacker's character bank. 0 in all data. |
| 0x2C | `hitSoundId` | I | `>= 0` runs ScriptVm kind 2 / id 2 with args (+0x2C,+0x30); the returned command record is executed on the attacker (`Obj_ExecCmdRecord`). -1 = none. |
| 0x30 | `hitSoundVariant` | I | second argument of that call (0 or 2). |
| 0x34 | `knockDirMode` Gof2AtKnockDirMode | T | `HitJudge_SetKnockDirection` 0x435A80: 0 = -sign(attacker facing), 1 = +sign (reversed), 2/3 = by relative x (3 reversed), 4 = 0. Same enumeration as RBO. |
| 0x38 | `victimFacingMode` Gof2AtFacingMode | T | `HitJudge_SetVictimFacing` 0x4359E0: 0/1 by position (1 reversed; victims in states 9/11/13 use the reversed sign), 2 = attacker facing sign, 3 = reversed, 4 = keep (returns without writing victim+1908). |
| 0x3C | `unused_3C[4]` | U | zero, no reader (RBO element/damage-type fields; GOF2 has no element system). |
| 0x4C | `hitFlags` Gof2AtHitFlags | T | 0x1: `HitJudge_ApplyHit` runs the ScreenYure script (`g_ScreenYureScriptBank` 0x739794, arg0 = `screenShakeId`) and `ScreenShake_Start`; 0x2: skips the HUD damage trace/statistics call `sub_4A05B0` (only reached when damage was dealt and the victim is a player fighter); 0x4000: `HitJudge_CollectPairs` lets the hit through victims that have hit protection (victim obj+4024 != 0 with obj+4040 <= 0 and obj+4028 <= `DefStatus+1052`); 0x10000: `HitJudge_ApplyHitStop` assigns no hit-stop. Data: 1 (643), 0 (314), 0x10000 (18), 0x4001 (10), 2, 3, 0x10001, 0x10003. |
| 0x50 | `reactionFlags` Gof2AtReactionFlags | T | 0x2: `HitJudge_ApplyHit` clears the "special hit" flag passed to `HitJudge_ComputeDamage` (v29 = special attack and not 0x2 ...) and `HitJudge_StartVictimReaction` refuses class 17 against victim pattern 26; 0x10 / 0x20: `HitJudge_StartVictimReaction` passes `!bit` to `Victim_EnterReactionState` -> `Victim_SetRecoverBlockFlags` 0x43DD00 which sets victim obj+1924 bits 0x10 / 0x20 (recovery eligibility test `sub_433990` requires `(obj+1924 & 0x22) == 2`); 0x1000000: `HitJudge_ApplyHit` suppresses the hit spark and the damage popups; 0x8000000: `HitJudge_ApplyKnockDirAndFacing` sets victim obj+1872 bit 0 (halves the knockback scale in `sub_43D9F0`, absent from the data); 0x200 occurs (0x230 in 12 records) but is read nowhere. Data: 0 (802), 0x30 (110), 2, 0x20, 0x1000000, 0x230, 0x1000030, 0x1000002. |
| 0x54 | `editorOnly_54` | E | 0x645 in 3 records, no reader (RBO hit-script enable). |
| 0x58 | `unused_58[5]` | U | zero, no reader (+0x58..+0x6B). |
| 0x6C | `clashRecoilState` | T | `HitJudge_ResolveSecondaryBoxClashes`: `value / 1000` -> victim-less attacker pending frame (obj+704), `value % 1000` -> pending pattern (obj+700), then `sub_4353C0` applies it (decimal packing). The WORD at +0x6E is tested by `CAppHanteiKougeki_BuildFromFrame`: nonzero moves all attack boxes of the frame to the secondary list. In data every nonzero value is 65 536..112 626 (hi word 1) so the two uses coincide; 42 distinct values, 62 records. Packed patterns are 599..650 (above the 256-entry pattern table), so the decoded target looks like an alternate id space; only the engine decode is proven. |
| 0x70 | `unused_70[2]` | U | zero, no reader (RBO hit skill id). |
| 0x78 | `hitScriptId` | T | `HitJudge_ApplyHit`: `!= 0` -> ScriptVm kind 4 / id -> list handed to `Obj_ExecCmdListBetween` 0x43AAF0. 1 in one record. |
| 0x7C | `guardMask` Gof2AtGuardMask | T | bit0 (1) stand guard can block (`HitJudge_DefenderGuardsAttack` guard mode 1), bit1 (2) crouch guard (mode 2); modes 3/4 ask the box set instead; `HitJudge_DefenderArmorAbsorbs`: armor mode 1 absorbs when `mask & 5`, mode 2 when `mask & 2`, mode 3 always; `ObjScan_StoreAttackGuardInfo`: `(mask&3)-1` -> guard hint 1/3/2 for the AI/HUD; `HitJudge_ApplyKnockDirAndFacing` copies it to victim obj+1876 which scales knockback in `sub_43D9F0` (mask&3 = 1/2/3 -> three factors); `sub_43BA00`/`sub_43C530` script conditions test `mask & arg`. 0x10000 (word +0x7E bit 0): modes 1/2 cannot guard it when `a3 == 0` (main pass), but can in the secondary pass. Data: 5 (365), 7 (269), 1, 6, 3, 2, 4, 0 and 0x1000x variants (16 with 0x10007). |
| 0x80 | `attackKind` Gof2AtKind | T | 0 normal, 1 special. `HitJudge_ComputeDamage` picks `DefStatus+576/580`; `sub_495890` indexes the combo scaling tables `g_ControllerManage[69+k]/[71+k]`; `HitJudge_DefenderGuardsAttack`: special attacks are guardable only if the defender state script (3/5) answers 2; `HitJudge_CollectPairs`: armor check only for special; `HitJudge_CalcGuardGaugeLoss`, `HitJudge_GaugeSlotFromAttackKind`, `sub_497120` select the special-column constants; script condition type 3 compares it. Data 1 (546), 0 (451). |
| 0x84 | `screenShakeId` | T | arg0 of the ScreenYure script when `hitFlags & 1` (100 (0x64), 102, 105 in data). |
| 0x88 | `unused_88[2]` | U | zero, no reader. |
| 0x90 | `targetStanceMask` Gof2AtTargetStance | T | `HitJudge_CollectPairs` and `HitJudge_ResolveSecondaryBoxClashes`: the hit needs `mask & victim.stanceClass (obj+660)`; bit 8 also allows victims that are already in hit-reaction states 28..39 (otherwise skipped, state 33 excepted). Data: 0xF (929), 0xB, 0xC, 7, 3. |
| 0x94 | `unused_94[12]` | U | zero, no reader (+0x94..+0xC3). |
| 0xC4 | `hitScriptGate` | T | read by `HitJudge_CollectPairs` (non-zero runs the kind-4 list of +0x78 and uses an entry of type 0 as a fixed damage override); zero in all 997 records, so the branch is dead in shipped data. |
| 0xC8 | `unused_C8[9]` | U | zero, no reader (+0xC8..+0xEB). |

### 1.3 Class -> victim reaction pattern (`HitJudge_StartVictimReaction`, `HitClass_ToStandReactionState`)

Victim stance (obj+660, forced to "air/dead" when the victim status object+28 == 1) selects the column; values are victim patterns:

| hitClass | stand (1) | crouch (2) | air / other |
|---|---|---|---|
| 0,1,2,16 | 28 | 30 | 8/32 (7/31 when dead) |
| 3,4,5,14 | 29 | 30 | 8/32 (7/31 when dead) |
| 6,7 | 31 | 31 | 31 |
| 8 | 32 | 32 | 8/32 |
| 9 | 38 | 38 | 38 |
| 10,15 | 36 | 36 | 36 |
| 11 | 37 | 37 | 37 |
| 12,13 | 39 | 39 | 39 |
| 17 | 31 | 31 | 8/32 (or no reaction vs victim pattern 26 with reactionFlags&2) |

Stance values other than 1/2/4 (e.g. 0 or 3) leave the pattern at 0 (no state change). Priority ranks (`HitClass_ReactionPriority`): -1: 17; 1: {1,4}; 2: {2,5}; 3: 8; 4: {14,16}; 5: 6; 6: 7; 7: 11; 8: {12,13}; 9: 10; 10: {9,15}; 11: 18; everything else 0.
The group names in `Gof2AtHitClass` (LIGHT/MEDIUM/HEAVY/CRUMPLE/SWEEP/LAUNCH) are labels derived from the table, not original names.

### 1.4 Comparison with RboAtRecord (120 bytes, `rbo_at_record.md`)

The first 0x78 bytes keep the RBO offsets for the shared concepts; GOF2 is 236 bytes because it appends the guard/attack-kind block.

| Off | RBO (RboAtRecord) | GOF2 (Gof2AtRecord) |
|---|---|---|
| 0x00 | hit_class (0..17) | hitClass, same 0..17 with 17 = no reaction |
| 0x04 | hit_sfx_set (sounds + shake length) | hitStopLevel (hit-stop tables) |
| 0x08 | hit_spark_kind (!=14 spawns) | sparkKind (==8 big spark) |
| 0x0C | hit_gate_mode (read) | unused (zero, unread) |
| 0x10 | base_power | damage |
| 0x18 | guard_damage (read) | editorOnly_18 (nonzero, unread) |
| 0x1C | status_duration | drawOrderMode |
| 0x20 | victim_shake_mode (==1) | hitEffectKind (1..5) |
| 0x28/0x2C/0x30 | sound bank / id / random range | same layout; sound command through ScriptVm kind 2 / id 2 |
| 0x34/0x38 | knock_dir_mode / victim_facing_mode | identical enumerations and consumers (HitJudge_SetKnockDirection / SetVictimFacing) |
| 0x3C..0x48 | element source / element / damage type / damage mode | zero, unread |
| 0x4C / 0x50 | target_fx_flags / damage_rule_flags | hitFlags / reactionFlags with different bits |
| 0x54..0x5C | hit script enable/type/id | 0x54 editor-only (0x645), 0x58/0x5C unused |
| 0x6C..0x70 | skill params / hit_skill_id | clashRecoilState / unused |
| 0x74 | unused | unused |
| 0x78..0x90 | (record ends at 0x78) | hitScriptId, guardMask, attackKind, screenShakeId, targetStanceMask |
| 0xC4 | - | hitScriptGate |

## 2. Sections 4 and 5: legacy tables (no reader)

* Section 4 (`Gof2Section4Record`, 28 bytes) is indexed 0-based by frame `+0xEC[0]` when frame `+0xE8` (the "legacy box group A" count) is 1; section 5 (`Gof2Section5Record`, 20 bytes) by frame `+0x108` (= `+0xFC[3]`) when
  frame `+0xF8` (group B count) is 1. The earlier note that these slots "do not take part in the box numbering" is explained by this: they are record indexes, not box indexes. Verified on the data: AMIY_CHR_SELECT (1 + 1 records, frame pattern 1/4),
  IMAGAWA_CHR_SELECT (2 + 2, pattern 1 frames 5 and 14), SYSTEMEFFECT (4 + 0, patterns 150/151 frames 0,1 -> records 0..3). Only these 3 effective files carry the sections (7 + 3 records; the old doc counted 5 + 4 files because of duplicates).
* Records: S4 `{kind 1 or 0xFF, valueA 0/0x100/0x200, valueB 0/0x100, ticks 0/1/30, 0,0,0}`, S5 `{1,0,0,0,0}`. SYSTEMEFFECT's `{255, 0x200, 0x100, 30}` / `{255, 0x100, 0x100, 1}` look like a zoom ramp (256 = 1.0) over 30 ticks, but nothing proves it. All fields E (nonzero) or U.
* **No reader, proof by scan**: (1) the view fields are `Gof2PatternAreaView.section4/5` at +0x14/+0x18. Every function in .text 0x401000..0x53C000 was run through a register taint (sources: `mov r,[x+27Ch]` = Obj+636, `mov r,[x+280h]` = chara block +8 -> view, `lea r,[x+268h]` + `[anime+14h]`,
  anime-typed functions seeded with all registers) flagging `[view+14h]`/`[view+18h]` (and `[block+1Ch]`/`[block+20h]`): zero hits outside the loader (`Gof2PatternArea_ComputeSectionPointers` writes them, `Gof2PatternArea_DecryptSections0to5` decrypts them when xorFlag is set; no shipped file sets it).
  Positive control: the same scan finds `[view+1Ch]`/`[view+20h]` in `CHanteiAnime_GetFrameScriptLists`. (2) The only fields that could index them, frame `+0xE8..+0x10B`, have no reader (`gof2_frame.md`). (3) The decompiled text of all 5000 functions contains `section4`/`section5` only in the two loader functions.

## 3. Sections 6 and 7: script id lists (`Gof2ScriptListEntry`, 20 bytes = 5 dwords)

* Section 6 `+28` view field, frame `+0xBC` (1-based index, 0 = none, record 0 all-zero dummy), section 7 `+32` view field, frame `+0xC0`. `CHanteiAnime_GetFrameScriptLists` 0x449A30 returns `{view.scriptListsA + 20*A, view.scriptListsB + 20*B}` and
  `frame.scriptOrderFlags & 1`; `Obj_RunFrameScriptLists` 0x4361E0 (called from `Obj_SetActionAndRunScript`) runs `Obj_CollectTransitionRuleEvents` 0x43E030 (section 7, ScriptVm kind 1) and `Obj_CollectFrameEnterEvents` 0x43E1B0 (section 6, kind 0) on the
  character script bank `*(obj+4320)+64600`, and returns the order flag.
* Both collectors read slots 0..4 in ascending order (`v7 += 4; while v7 < 20`), skip id 0, call `ScriptVm_CallSyncScript(bank, kind, id)` with `arg0 = (obj+432 & 0x10000) != 0`, `arg1 = obj`; the returned event records
  (first dword = event type) are linked by type category: section 6 `FrameEnterEventList_Link` 0x43E120 (`g_FrameEnterEventCategory_G2` 0x5A1F88; accepted categories 0,1,4,5, at most 20 events; type 249/1 rejected), section 7 `TransitionEventList_Link` 0x43DF90
  (`g_TransitionEventCategory_G2` 0x5A1B88; categories 0,1,2,3,4,6,7, at most 5; category 6 (type 122) is a single slot, type 249 rejected).
  Accepted types - frame enter: cat0 {40,99,100,126..133,230,233,235..241,243..248,250,251}, cat1 {30,252,253}, cat4 {242}, cat5 {50}; transition: cat0 {11,13,21,121,127,128,197,201}, cat1 {15,17,123,125,200,210}, cat2 {14}, cat3 {20,50,52,196,198,199,202,220,255}, cat4 {16,53}, cat6 {122}, cat7 {51,124,126,203,204,205}.
* The ids index the character `.FOB` index tables: FOB = `u32 nFuncs; {char name[32]; u32 pc}[nFuncs]; u32 nTypes; {u32 count; i32 pc[count]}[nTypes]; code`. Proof on the data: for the 16 characters the max section-6 id equals `count[0]-1` (KANAE 8980 of 8981,
  SANDBUG 2000 of 5004, SYSTEMEFFECT 1000 of 1001) and the max section-7 id equals `count[1]-1` (110 of 111, DATE 130 of 131, SYSTEMEFFECT 300 of 301). Two out-of-range ids exist (TAKEDA 10004, TOKUGAWA 9001: the script call fails, the list collector returns 0); KAGE has empty tables and only zero ids.
* Data (effective): section 6 3537 records (slots 0..4 nonzero in 2815 / 782 / 159 / 106 / 932 records; max id 10004), section 7 1005 records (slots 0..4 nonzero in 992 / 129 / 19 / 0 / 4 records, slot 3 is zero in all of them although it is read; max id 300). All 20 bytes T.

## 4. Section 8: effect-spawn records (`Gof2EffectSpawnRecord`, 96 bytes)

Frame `+0x190` is the 1-based index (0 = none, record 0 dummy); 45 files, 729 records. `Obj_SetActionAndRunScript` spawns it after entering the frame when `record.enable != 0`; `Obj_SpawnFrameEffectRecord` 0x4327F0 re-poses
every object in the attacker's linked-target list (`obj+143*4`, the held/thrown victims): position = `facing * (anchor.x + targetOffsetX), anchor.y + targetOffsetY`, facing from `facingCmd`, then the state from `stateMode`, then `linkMode`, then `Obj_ApplyEffectRecordThrowDamage`.
`Obj_SpawnDefaultEffectRecord` 0x432790 builds a synthetic record `{1,0,0,1,2,8,..}` (mode 2, hit class 8, facing cmd 1), which fixes the field order.

| Off | Field | Tag | Evidence |
|---|---|---|---|
| 0x00 | `enable` | T | gate `*(sec8+96*idx) != 0`; 1 in all records |
| 0x04 / 0x08 | `targetOffsetX/Y` | T | stored in the link context (+0x18..0x28) and used for the target position; Y negative in most records (hold point above the feet) |
| 0x0C | `facingCmd` | T | switch: 0 keep (-1), 1 `dword_5A2390[facing]`, 2 attacker facing, 3 force 0, 4 force 1 (then `sub_4353C0`) |
| 0x10 | `stateMode` | T | 0: `ScriptVm(&g_SysData_YarareBankIdx, kind 0, id 0)` table, entry `8*stateArg0` = (pattern, frame); 1: pattern = stateArg0, frame = stateArg1; 2: hit-reaction: class stateArg0 -> pattern (HitClass_ToStandReactionState; 7/31 if the target status object+28 == 1 for classes 0..5,8,14,16,17), `HitJudge_SetVictimFacing(stateArg1)`, `HitJudge_SetKnockDirection(stateArg2)`; modes 1 and 2 detach the link context afterwards (`Obj_ResetOnDetach`). Data (effective): 0 (677), 2 (51), 1 (1) |
| 0x14 / 0x18 / 0x1C | `stateArg0/1/2` | T | as above (frame -1 = start; stateArg2 only in mode 2, values 0/1/4) |
| 0x20 | `unused_20[3]` | U | zero, no reader |
| 0x2C | `linkMode` | T | switch: 1 copies the link context fields (obj+472..484) from the child's context, 2 sets flag obj+472 = 1, 3..6 set obj+476 = 0..3 with obj+472 = 0 |
| 0x30 | `unused_30[5]` | U | zero, no reader |
| 0x44 | `throwDamage` | T | `Obj_ApplyEffectRecordThrowDamage` 0x4326C0: non-zero runs ScriptVm(`g_ControllerManage[112]`, kind 0, id 7, arg0 = +0x4C), writes the value into the result array and calls `Obj_ExecCmdListBetween` on the target (damage on throw end); 10 records: 50, 50, 100, 400, 400, 500, 500, 500, 900, 1250 |
| 0x48 | `eventScriptId` | T | `ScriptVm(bank, kind 5, id)`; the returned -1-terminated list is only skipped over (no effect); 1 in 3 records |
| 0x4C | `throwScriptArg` | T | argument of the kind-0/id-7 call; zero in all records |
| 0x50 | `unused_50[4]` | U | zero, no reader |

Byte count: T 48, U 48.

## 5. File lookup (the answer to "where does GOF2 read a character from")

### 5.1 The VFile layer

`g_FileIo_pfnOpen` 0x5ECEC8 / `g_FileIo_pfnClose` are assigned once, in the static init `sub_444310`: `g_FileIo_pfnOpen = FileIo_OpenCFile` (renamed **`FileIo_OpenLooseThenPacSlots`** 0x415CA0, the GOF2 equivalent of RBO's `VFile_OpenLooseThenPacSlots`):

1. `new CFile; CFile_OpenForRead(path)` = `CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL)`. The path is used as written, **relative to the process current directory** (the exe never calls SetCurrentDirectory).
2. On failure `PacSlots_OpenByFileNameHighestFirst` (0x419770, was sub_419770): `for (i = g_PacSlotCount-1; i >= 0; --i) PacSlot_OpenEntryByName(path, &g_PacSlots[i])`. `g_PacSlotCount` = 6 (`PacSlots_Create` 0x419620, 6 x 20-byte slots at `g_PacSlots` 0x72EEE4).
   Slots are filled by `PacSlots_OpenAllDataArchives` 0x415C50 from the six paths `.\Data\data00.dat` .. `.\Data\data05.dat` (260-byte stride from 0x5A2590; `PacSlot_OpenArchiveReadToc` 0x4193E0 validates magic 1, count ^ 0xE3DF59AC and decodes the 68-byte entries) - a missing/invalid archive aborts startup.
   **Highest slot wins: data05 > data04 > .. > data00.**
3. `PacSlot_FindEntryByName` 0x419570 (RBO `PacSlot_FindEntryByName`): `_stricmp(PathFindFileNameA(path), entry.name)` over the TOC - **the match is on the file name only, case-insensitive, the directory part of the requested path is ignored**.
   The hit becomes a `CPackAccess` window (`PackAccess_OpenEntryWindow` 0x419250: opens the archive, seeks to entry+60, size entry+64).

`FileIo_OpenLooseThenPacSlots_Out` 0x428ED0 is the same two steps for the `.FOB` script loader (`ScriptBank_LoadFile`, which upper-cases the path first). The BGM loader `sub_458410` repeats the idiom inline.

### 5.2 Character files

`CharaFile_BuildDt2PatChpPaths` 0x4932C0 (was sub_4932C0) builds the three paths of one character block (called by `CharaDataResMgr_StepOpenFile` 0x494200 for the threaded loader and by `CharaRecord_LoadAllBlocksSync` 0x4933E0 for the synchronous one;
block index 0 = main, 1 = `_effect`; the character record is `CharaRecord_Setup` 0x492F50 with flags, character id and costume):

| Case | base path (sprintf) |
|---|---|
| flags bit 4 set (battle load, flags = 5): `CharaFile_BuildBasePathById` 0x4A3100 | `.\Data\Chara\<n>\<n>` with `<n>` = `aKanae[16*id]` = kanae, date, imagawa, amiy, maeda, sanada, tokugawa, cyosokabe, takeda, uesugi, negai, katsuko (ids 0..11); id 50/51 `.\Data\Chara\bonus_stage\<n>\<n>` (tekkyu, sandbug); id 100 `.\Data\Chara\SystemEffect\SystemEffect`; id 101 `.\Data\Chara\Kage\Kage`; id 200..209 `.\Data\Chara\judge_chara\shinpan%02d` |
| flags bit 4 clear (character select, flags = 0) | `.\Data\Chara\%s\%s_chr_select` |

Then, with `Buffer` = base and `sfx` = `""` for block 0 or `"_effect"` for block 1, and `NN` = the costume number of the record (`%02d`):

| file | format | example (Kanae, battle) |
|---|---|---|
| pattern area | `"%s%s.dt2"` | `.\Data\Chara\kanae\kanae.dt2`, `.\Data\Chara\kanae\kanae_effect.dt2` |
| PAT parts bank | `"%s%s%02d.pat"` | `.\Data\Chara\kanae\kanae00.pat` / `kanae01.pat`, `kanae_effect00.pat` |
| CHP bank | `"%s%s%02d.chp"` | `.\Data\Chara\kanae\kanae00.chp` / `kanae01.chp` (no `_effect*.chp` exists; the effect block has no CG bank) |
| character script | `CharaFile_LoadFobScriptBank` 0x493100: base + `".Fob"` | `.\Data\Chara\kanae\kanae.Fob` |

(costume number -1 would select the legacy single `"%s%s.dat"` path through `CharacterLoad_SetupDatPath`, `Gof2Han2_OpenPreferDt2ReadHeader32` then prefers a sibling `.dt2`; no shipped record uses it.)
Chr-select blocks use the same formats with base `...\<n>_chr_select`: `.\Data\Chara\kanae\kanae_chr_select.dt2`, `kanae_chr_select00.pat` (no `.chp` is shipped; the archive holds only `*_CHR_SELECT.DT2` and `*_CHR_SELECT00.PAT`).
Other character-named scripts (not part of the pattern load): `.\Data\script\ac_script\<n>\ac_<n>_script.Fob`, `ac_<n>_%04d.Fob`, `.\Data\script\story\story_<n>_script.Fob`, `story_kanae_%04d.Fob`.

Archive names that satisfy these lookups (matched by file name): `KANAE.DT2`, `KANAE00.PAT`, `KANAE01.CHP`, `KANAE.FOB`, `KANAE_EFFECT.DT2`, `KANAE_CHR_SELECT.DT2` ... in data02 (data05 additionally holds newer `.DT2`/`.FOB` for all 13 characters and wins).

### 5.3 Which costume (00 or 01)

The number comes from `g_SideColor[side]`: `CharaDataResMgr_SetupBattleRecords` 0x4938B0 calls `CharaDataResMgr_SetupRecordVf5(5, side, g_SideCharaId[side], side, g_SideColor[side], ...)` for side 0 and 1, then slots 2 (id 100 SystemEffect) and 3 (id 101 Kage) with costume 0.
`g_SideColor` is written by `CharaSelect_CommitPicksToGlobals` 0x4C51E0 from the CSS panel colour: confirming with the alternate button (input bit 0x400) selects colour 1, plain confirm colour 0; if both sides pick the same character with the same colour, the panel that confirms second is flipped `(c+3)%2`.
Story/arcade/CPU: colour 0, and a CPU opponent that mirrors the player's character gets `(playerColour+1)%2`. Replays restore the stored colour (`Replay_ApplyHeaderToBattle`). Judge (`shinpan`), bonus and system blocks always use costume 0.

### 5.4 Does the game accept loose files? Yes - recipe for a test copy

Every open of the above goes through `g_FileIo_pfnOpen` = loose-first, so a loose file under `.\Data\Chara\...` overrides any archive copy, **even data05**. data02.dat does not have to be absent and no config/flag exists.

1. Create the copy (the archives are 2.4 GB; hard links keep the copy tiny):
   `mkdir C:\games\gof_test\Data` ; copy `GOF2.exe` and every file of `C:\games\gof` that is not a folder named `Data\dataNN`/`log`/`Capture`; then
   `for %i in (0 1 2 3 4 5) do mklink /H C:\games\gof_test\Data\data0%i.dat C:\games\gof\Data\data0%i.dat` and copy `Data\System00.dat`, `Data\_NetServerList.txt` (optional). `Data\SaveData\*` is recreated by the game.
   The folders `Data\data00`..`data05` (extracted listings) are never read by the game and can stay out of the copy. Keep the local `wsock32.dll`/`ws2_32.dll` only if you want the original network hooks; they are irrelevant offline.
2. Put the test file at **`C:\games\gof_test\Data\Chara\kanae\kanae.dt2`** (create the folders; case does not matter). Any of `kanae00.pat`, `kanae01.pat`, `kanae00.chp`, `kanae.fob`, `kanae_effect.dt2` can be overridden the same way (`Data\Chara\<n>\<file>`, character ids per 5.2); files you do not place still come from the archives.
3. Start with the **current directory = the copy** (`cd /d C:\games\gof_test && GOF2.exe`, or a shortcut with "Start in" = the copy). The paths are `.\Data\...`; launching from another directory loads the original archives silently.
4. Pick the character in VS/Training. Costume 01 needs the alternate confirm button on the character select (5.3). For `_chr_select` data override `kanae_chr_select.dt2` instead.
5. Constraints for an edited DT2: it must stay a valid HAN2RBO file: the pattern-area header lead dwords must be (0,8,0,5,0,0,0,0) or the load aborts (`State0_4_NextFile`); the `{offset,size}` at header +0x20 is the only thing read in dt2+pat+chp mode (the parts/CG/name refs at +0x28..0x3F are not used because PAT and CHP come from their own files), and the size is clamped to the file length; section sizes at +0x30..0x53 must add up (`Gof2PatternArea_ComputeSectionPointers`).
   To confirm the loose file is used, run Process Monitor with `Path ends with kanae.dt2`: a successful CreateFile of the loose path precedes any archive read.
6. Offline only: online play checks nothing in the code we traced, but both peers must run identical data.

### 5.5 CHP bank loader (`<n>NN.chp`)

The `.chp` is the same BMP Cutter3 bank as RBO's CG area (`RboCgFileHead`): `"BMP Cutter3\0\0\0\0\0\1\0\0\0"`, 8 palettes, `block_count` at +0x201C, `effect_offset[3000]` at +0x2044, `blocks_offset` at +0x4F24 (KANAE00.CHP: 2890 blocks, 98 effects, storage types 1 (97) and 4 (1)).
In dt2+pat+chp mode `State0_0_LoadDT2File` stores the PAT and CHP file sizes (`FileIo_GetFileSizeByPath`, +0x58 / +0x60) with offset 0. The CHP states: `CharacterLoad_BeginChpBlock` 0x447F20 (offset/size -> chara block +8272), `CharacterLoad_ReadChpHeaderAndBlockTable` 0x447FC0
(reads the first 0x4F30 = 20272 bytes of the file, then `AllocateAndReadFileData` of `24*block_count` bytes at `blocks_offset`; they become the block array at chara block +32280), state 0xB/0xC read the whole file, `CharacterLoad_IndexChpEffects` 0x448150 builds `effect[3000] = {dword at record+0x40 (first block), dword at +0x44 (count)}`
(0,0 when the offset is -1) and frees the buffer; the later states (0x10..0x17) decode the effect pixels into 256x256 textures exactly like RBO. xorFlag handling is the same `Gof2_XorDecryptBuffer` and never set.

### 5.6 PAT v4 part records are field-for-field `RboPatPart`

`CharacterLoad` state 6 (`a1[2] == 6`) reads the first **72028** bytes of the `.pat` (`24 + 2000*4 + 2000*32 + 4`; RBO 36028 = 1000 poses), turns `poseOffset[i]` into part indexes `(off - 72028)/92` (0 -> -1) at chara block +68 and the image offset (+72024) into the pose-data pointer at chara block +8068 (the draw code reads `*(block+52+8016)`); the 24-byte header (`{4, 0x01234567, 0,...}`, 64/64 PATs) and the names are never read.
A pose is 40 x 92 = 3680 bytes (checked on all 64 PATs: offsets % 92 == 0 and 3680 apart). The draw code (`Pose_SelectPartsForDraw` 0x49E9A0 -> `DrawSort_BubbleByPriority` 0x43FAA0 -> `DrawSort_Run` 0x440520 with `PosePart_ComputeGeometry` 0x43FE50, `PosePart_ModulateColor` 0x440000,
`PosePart_AddColor` 0x440170, `PosePart_GetFlippedSrcRect` 0x4402A0, `Math_LerpAngle10000ToDeg` 0x43FD00) reads x +0, y +4, destW +8, destH +0xC, flip +0x10, scale +0x14/+0x18, rotation +0x1C, modulate +0x20..0x23, add colour +0x24..0x26, texture +0x28, src +0x2C/+0x30/+0x34/+0x38,
layer +0x3C, origin +0x40/+0x44: the RBO layout and semantics. Differences, all minor: (a) the four src fields and the layer are read as full dwords (RBO int16 + unused hi word; the hi words are zero in all 1 233 240 parts), `src_w == 0` or `src_h == 0` hides the part;
(b) **byte +0x11 (flip bit 0x100) != 0 forces the additive blend state** `dword_5A1AAC` (same as frame blend mode 2), seen in 1441 parts; bit 0x10000 (44 parts) is read nowhere; (c) +0x48..+0x5B are zero in all parts and unread. Types: `Gof2PatPart`, `Gof2PatFileHead`, `Gof2PatHeader`.

## 6. Renamed functions / data in GOF2.exe.i64 (this task)

File lookup: `FileIo_OpenLooseThenPacSlots` 0x415CA0 (was FileIo_OpenCFile), `FileIo_OpenLooseThenPacSlots_Out` 0x428ED0, `PacSlots_OpenByFileNameHighestFirst` 0x419770, `PacSlot_OpenEntryByName` 0x4195B0, `PacSlot_FindEntryByName` 0x419570, `PackAccess_OpenEntryWindow` 0x419250,
`PacSlot_OpenArchiveReadToc` 0x4193E0, `PacSlots_OpenAllDataArchives` 0x415C50, `PacSlots_Create` 0x419620, `PacSlots_Destroy` 0x4196D0, `g_PacSlots` 0x72EEE4, `g_PacSlotCount` 0x72EEE0, `FileIo_GetFileSizeByPath` 0x446630, `CharaFile_BuildDt2PatChpPaths` 0x4932C0, `CharaFile_BuildBasePathById` 0x4A3100,
`CharaFile_LoadFobScriptBank` 0x493100, `CharaRecord_LoadAllBlocksSync` 0x4933E0, `CharaFile_LoadDt2PatChpSync` 0x446FC0, `CharaFile_LoadDatSync` 0x446E30, `CharacterLoad_SetupDt2PatChpPaths` 0x4474D0, `CharacterLoad_SetupDatPath` 0x4473F0, `CharaRecord_Setup` 0x492F50,
`CharaDataResMgr_SetupRecordVf5` 0x493870, `CharaDataResMgr_SetupBattleRecords` 0x4938B0, `CharacterLoad_BeginChpBlock` 0x447F20, `CharacterLoad_ReadChpHeaderAndBlockTable` 0x447FC0, `CharacterLoad_IndexChpEffects` 0x448150, `g_SystemSeScriptBank` 0x5A3A34, `g_ScreenYureScriptBank` 0x739794.
Hit resolution: `HitJudge_StartVictimReaction` 0x435FA0, `HitClass_ToStandReactionState` 0x435E10, `HitClass_ReactionPriority` 0x435D60, `Victim_EnterReactionState` 0x435EA0, `Victim_SetRecoverBlockFlags` 0x43DD00, `HitJudge_ApplyKnockDirAndFacing` 0x435B10,
`HitJudge_ApplyKnockDirAndFacingNoFlipCheck` 0x435C10, `HitJudge_SetKnockDirection` 0x435A80, `HitJudge_SetVictimFacing` 0x4359E0, `HitJudge_ApplyHitStop` 0x496CE0, `Obj_RaiseHitStopFrames` 0x496BE0, `HitJudge_RunHitSoundCommand` 0x49D1A0, `HitJudge_StartHitEffect` 0x4317D0,
`HitJudge_ResolveSecondaryBoxClashes` 0x49ACB0, `HitJudge_OnAttackGuarded` 0x4967C0, `HitJudge_FindBoxOverlap` 0x496A20, `HitJudge_CalcGuardGaugeLoss` 0x4977B0, `HitJudge_GaugeSlotFromAttackKind` 0x494950, `HitJudge_GaugeSlotFromSideMode` 0x4949E0, `Effect_SpawnHitSpark` 0x49CC60,
`Obj_ExecCmdListBetween` 0x43AAF0, `g_SysData_HitStopAttackerTable` 0x739708, `g_SysData_HitStopVictimTable` 0x73970C, `g_SysData_HitStopCounterBonusTable` 0x739718.
Sections 6..8 and parts: `Obj_RunFrameScriptLists` 0x4361E0, `Obj_CollectTransitionRuleEvents` 0x43E030, `Obj_CollectFrameEnterEvents` 0x43E1B0, `FrameEnterEventList_Link` 0x43E120, `TransitionEventList_Link` 0x43DF90, `g_FrameEnterEventCategory_G2` 0x5A1F88, `g_TransitionEventCategory_G2` 0x5A1B88,
`Obj_ApplyEffectRecordThrowDamage` 0x4326C0, `Obj_SpawnDefaultEffectRecord` 0x432790, `Pose_SelectPartsForDraw` 0x49E9A0, `PosePart_ComputeGeometry` 0x43FE50, `PosePart_ModulateColor` 0x440000, `PosePart_AddColor` 0x440170, `PosePart_GetFlippedSrcRect` 0x4402A0, `Math_LerpAngle10000ToDeg` 0x43FD00.
Prototypes with the new types: `Obj_SpawnFrameEffectRecord(void*, Gof2EffectSpawnRecord*)`, `Obj_ApplyEffectRecordThrowDamage`, `Obj_CollectFrameEnterEvents` / `Obj_CollectTransitionRuleEvents` / `CHanteiAnime_GetFrameScriptLists` (Gof2ScriptListEntry**),
`HitJudge_ApplyKnockDirAndFacing`, `HitJudge_StartVictimReaction`, `HitJudge_RunHitSoundCommand`, `HitJudge_ApplyHitStop`, `CAppHanteiKougeki_BuildFromFrame` (Gof2AttackBoxSetPrefix* = first 16 bytes of the kougeki object, `->record` is the Gof2AtRecord pointer).
The attack-flag enums (`Gof2AtHitFlags`, `Gof2AtReactionFlags`, `Gof2AtGuardMask`, `Gof2AtTargetStance`, `Gof2PatPartFlip`) were made bitmask enums with `idc.set_enum_bf`. The DB was not saved by this task.

## 7. Open points (honest list)

* `drawOrderMode`, `hitEffectKind`, `sparkKind` and the `hitSound*` triple are I: the readers and their dispatch are traced, but what the target objects/effects look like on screen was not run.
* `clashRecoilState` decodes to patterns 599..650 which do not fit the 256-entry pattern table; the packing is proven, the target id space is not.
* `editorOnly_18` (590 nonzero records) having no reader is surprising; the proof is the complete `Obj+0x614/0x620` reader scan above (no other path to the record exists: only two `imul 0xEC` sites, one stored pointer). A reader through a dynamically computed pointer cannot be excluded by a static scan.
* `sub_495B60` (reads +0x10/+0x80; called per object pair by `sub_4964D0`) and `sub_43D9F0`/`sub_43D600`/`sub_43D790` (knockback scaling from victim obj+1872/+1876) are traced only as far as the record fields; they are not renamed.
* Section 4/5 meanings are guesses from data only (they are engine-unread).
