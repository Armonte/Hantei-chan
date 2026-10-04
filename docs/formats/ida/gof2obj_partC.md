# GOF2 Obj (pool slot 0x1264) part C: bytes 0xC40..0x1263 (decimal 3136..4707)

Deliverables: `gof2obj_partC_fields.py` (FIELDS, 48 rows, contiguous 3136..4707), `gof2obj_partC_types.h` (11 structs + 7 enums,
parsed into the IDB with idc.parse_decls, sizes machine-checked incl. standalone `Gof2ObjTailC` = 1572 bytes), generator `gof2obj_partC_gen.py`
(asserts offsets/sizes), raw per-offset evidence dump `gof2obj_partC_dump.txt`.

## Method
All [reg+disp] operands with disp 0xC40..0x1263 over the whole binary were enumerated (2171 operands, 232 distinct disps) and grouped by
function; also scanned for absolute operands into g_ObjPool+0xC40.. (none), data dwords holding offsets (none relevant), and indexed
(SIB) accesses. Obj-ness of a function was decided by whether it also touches Obj-only offsets (0x11FC, 0x1254, 0x10E0 ...).

## Findings
* **0xC40..0xFB7 is not free space.** The object at Obj+0x6F0 (CAppHanteiYarare, cleared by `sub_43E3D0` = `Yarare_ResetHitState`, esi=obj+0x6F0)
  is 0x8F0 bytes long (Obj+0x6F0 .. +0xFE0). `sub_495B60` (0x495E62) reads `[defender + (attackerSlot*256 + attackerAction)*4 + 0x7B8]`:
  a 2x256 dword table (0x800 bytes, `memset(a2+200,0,0x800)` in the reset) that ends exactly at 0xFB8. My range holds the second half
  (row 1 entries 34..255). Every other operand with disp 0xC80..0xED4 belongs to CSceneGame / CSceneNetMenu (0x4CC...-0x4D4...),
  CWSServerClient (0x48B..-0x48E..), sub_448270, sub_4213A0, StyleWheel_AnimAngle: none has an Obj base.
* 0xFB8..0xFE0 is the tail of the same Yarare hit memory: an *armor gauge* (never enabled: no writer of the enable word), the guard gauge
  (`guardGauge`, Obj_InitDefaults = DefStatus+16, `sub_440E60` clamps to [0,max]), a hit-grace stage counter.
* 0xFE0/0x1018/0x1048: CAppHanteiEtc (56 B) / Sousai (48 B) / Tobi (44 B) box sets. Layout from vf6/vf7 box builders. Quirk: Etc stores
  the slot-number array at +0xC while the box pointers start at +8 (overlap, slot numbers are write-only). Sousai/Tobi keep slots at +0x14.
  Contact state per category (flag + mask) is consumed by `Obj_ContactCategoryMatches` (sub_42F5B0): bit 0x10 Sousai, 0x20 Etc, 0x40 Tobi.
* 0x1074..0x1098: freeze/hit-stop/shake timers handled by `ObjTree_TickTimersAndTimeScale` (sub_49E120).
* 0x10A4..0x10C4: RBO-style armor state (RboP3ArmorState equivalent) extended with two hit-count armors (state ops 1/2/3 of `Obj_ExecStateOpList`).
* 0x10CC..0x10E8: hit bonus counter, shared-record pointers set by Obj_BindCharaRecord(AndStartAction), incoming-attack AI hints.
* 0x10EC..0x11B4: three 64-byte tint/flash effect slots (kind, tick/apply fn pointers, RGB tint and add colours, period), only slot 0 is started.
* 0x11B8..0x11CC: frame-event flags and the queued state-change request applied by `Obj_ApplyPendingTransition` (sub_4353C0).
* 0x11E0..0x1220: position block (parent link + inherit flags, local XYZ, world XYZ, prev local XY) = RBO localX/Y/Z, posX/Y/Z, prevLocalX/Y;
  ObjEffect list header at 0x1210.
* 0x121C..0x1263: flag words, time-scale accumulator (slow motion), 3-layer zoom block, four refcounted shared nodes
  (Fighter_AllocSharedSubNodes writes obj+0x121C+0x38..0x44).

## Unused / dead bytes (proof)
No operand with that displacement exists anywhere (all functions scanned): armorGauge+0x14 (0xFCC), 0x10C8 (only zeroed by InitDefaults),
tint slot +0x38/+0x3C (x3), Etc +0x18 / +0x20 / +0x24, ObjEffectList +0x00. Write-only (traced writers, no reader): tintFxRef[3], slot-number arrays,
Etc +0x28/+0x1C, Tobi +0x28. Read-only (only zero writers, so always 0): armorGauge.enabled, hudPopupParam, contactFlagsMisc,
ruleOverrideFlags, hitEffectStyleFlags.

## Renamed functions (behaviour-based)
Obj_ExecStateOpList 0x438A70, Obj_ApplyPendingTransition 0x4353C0, Obj_CommitPendingActionFrame 0x430490, Obj_QueuePendingTransition 0x4303B0,
Obj_ClearPendingTransitionAndTargets 0x4303E0, ObjArmorGauge_Tick 0x42ED90, ObjTree_TickFrozen 0x49E0B0, Obj_StartShake 0x496C60,
Obj_GetShakeFrames 0x496CB0, ObjTintFx_* (0x43EC80/43EDE0/43ECD0/43EDA0/43EE70), ObjEffectList_Append/Tick/Clear/Aggregate (0x442180/442430/4422E0/442340),
Obj_OrContactKindMaskOverChain 0x43C1E0, Obj_MarkEtc/Sousai/TobiContact (0x43C1A0/43C3A0/43C3E0), Obj_FreezeInPlaceAndClearBoxes 0x437C70,
Obj_PostFrameClearHitStopAndArmor 0x430C80, ObjArmor_RollChance 0x496F10, ObjGuardGauge_Apply 0x440E60, Obj_ResetTransientStateOnActionStart 0x4306D0,
Obj_ResetSharedModifiers 0x43DD40, Obj_LatchWorldPosAsLocal 0x434F50, ObjSharedNodes_ShareRefs 0x42EA60, Battle_ApplyRoundEventFreeze 0x4A3A80,
ObjZoomBlock_UseLayersAC/AB 0x42EB00/0x42EB60. (0x43E3D0, 0x49E120, 0x42F5B0, 0x42F570, 0x43C530 had already been renamed by other agents.)

## Confidence
Counts per FIELDS row (struct members counted separately in the header): see the tags in each comment. Meanings tagged `[inferred meaning]`
(reactionHoldFrames, hitGraceStage) are the weakest; all offsets/sizes/writers/readers are traced.
