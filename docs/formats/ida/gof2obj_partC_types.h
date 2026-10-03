// GOF2 Obj (pool slot 0x1264) part C: bytes 0xC40..0x1263. New types for IDB (idc.parse_decls). Offsets in struct comments are relative to the struct start.
// Evidence: docs/formats/ida/gof2obj_partC.md. Parse in this order (enums first, flag enums are bitmask).
enum Gof2ObjCFrameEventFlags : unsigned int { FEV_RESERVED_01=0x1, FEV_ADVANCE_MOTION_HISTORY=0x2, FEV_LANDING_FRAME_JUMP_PENDING=0x4, FEV_CLEARED_WITH_LANDING_08=0x8, FEV_FRAME_ENTERED=0x10000000 };
enum Gof2ObjCPendingEnterFlags : unsigned int { PEF_RESET_STATE_ON_ENTER=0x1, PEF_GUARD_SPECIAL_CANCEL=0x8 };
enum Gof2ObjCPendingCondFlags : unsigned int { PCF_NEEDS_HIT_CONFIRM=0x1, PCF_ADD_SPECIAL_GAUGE=0x2, PCF_BUMP_MOVE_USE_COUNTER=0x4 };
enum Gof2ObjCHitEffectStyleFlags : unsigned int { HESF_STYLE_1=0x1, HESF_STYLE_2=0x2, HESF_STYLE_3=0x4, HESF_GUARD_BONUS_TABLE=0x8 };
enum Gof2ObjCRuleOverrideFlags : unsigned int { ROF_FAIL_STANCE_RULES=0x8 };
enum Gof2ObjCContactSource : unsigned int { CSRC_OWN_BOX=0x1, CSRC_INHERITED_FROM_CHAIN=0x2 };
enum Gof2ObjCTintFxKind : unsigned int { TFK_NONE=0, TFK_HIT_FLASH_CYCLE=1, TFK_HIT_FLASH_FIXED=2 };
struct Gof2ObjCArmorGauge;
struct Gof2ObjCArmorState;
struct Gof2ObjCTintFxSlot;
struct Gof2ObjCPendingTransition;
struct Gof2ObjCPositionBlock;
struct Gof2ObjCEffectList;
struct Gof2ObjCZoomBlock;
struct Gof2ObjCSharedNodes;
struct Gof2ObjCEtcBoxSet;
struct Gof2ObjCSousaiBoxSet;
struct Gof2ObjCTobiBoxSet;
struct Gof2ObjCArmorGauge {   // 28 bytes
 int enabled; // +0x000 [traced reads] tested by sub_433990, HitJudge_CollectPairs, HitJudge_ResolveSecondaryBoxClashes, sub_4383D0, sub_438A70 op39, sub_4A3A80; NO writer anywhere (Obj_InitDefaults/sub_43E3D0 zero it) so this guard-point gauge is never switched on
 int gauge; // +0x004 [traced] compared with DefStatus+1052 in HitJudge_CollectPairs (hit passes when gauge > threshold); +DefStatus+540 by sub_4A3A80, +DefStatus+1168 by sub_438A70 op39; decays by DefStatus+984/988 in sub_42ED90
 int timerA; // +0x008 [traced] set to DefStatus+236 by Obj_SpawnFrameEffectRecord; += DefStatus+528 (sub_4383D0) / +1156 (sub_438A70 op39); counts down in sub_42ED90
 int timerB; // +0x00C [traced] sub_433990 / sub_4339xx: no flinch while timerB<=0 and gauge<=DefStatus+1056; += DefStatus+1160 (sub_438A70 op39); counts down in sub_42ED90
 int breakTimer; // +0x010 [traced] HitJudge_CollectPairs / ResolveSecondaryBoxClashes: hit goes through while >0; += DefStatus+532 (sub_4383D0) / +1164 (sub_438A70 op39); counts down in sub_42ED90
 unsigned char unused_14[4]; // +0x014 [unused] no [reg+0FCCh] operand in any function (all 2171 [reg+disp] hits for disp 0xC40..0x1263 scanned) and sub_43E3D0 (reset) skips it
 int suppressed; // +0x018 [traced] HitJudge_ResolveSecondaryBoxClashes sets 1 (0x49B230), Obj_SpawnFrameEffectRecord clears; sub_42ED90 skips the countdown while ==1
};
struct Gof2ObjCArmorState {   // 36 bytes
 int active; // +0x000 [traced] sub_430C80 derives it (1 when timerFrames!=0 || hitCountA>0 || hitCountB>0); sub_496F10 sets 1 on a successful roll; read by HitJudge_ApplyHitStop, sub_496DD0, sub_47B700. Same role as RBO RboP3ArmorState.armorActive
 int clearPending; // +0x004 [traced] set 1 by HitJudge_ApplyHit / sub_49E120 when a timer runs out, tested+cleared in sub_430C80 which then zeroes the whole state
 int chancePercent; // +0x008 [traced] state op 0 (sub_438A70) sets it; sub_496F10 rolls Rng_Range(100) < value and lowers it by 2 per call; RBO armor chancePercent
 int strainCount; // +0x00C [traced] ++ in HitJudge_ResolveTick (0x4996A0) on armor success; RBO armor strainCount
 int timerFrames; // +0x010 [traced] state op 1 (sub_438A70) sets it; sub_49E120 counts it down, at 0 sets clearPending
 int hitCountA; // +0x014 [traced] state op 2: armor that survives N hits; HitJudge_ApplyHit decrements per hit
 int hitTimerA; // +0x018 [traced] state op 2: frame limit of the hit-count armor; sub_49E120 counts it down
 int hitCountB; // +0x01C [traced] state op 3: second hit-count armor; HitJudge_ApplyHit decrements per hit
 int hitTimerB; // +0x020 [traced] state op 3: frame limit of the second armor; sub_49E120 counts it down
};
struct Gof2ObjCTintFxSlot {   // 64 bytes
 int lifetimeTicks; // +0x000 [traced] sub_49E120 (3 slots, stride 64): while !=0 it counts down (>0) and calls tickFn; -1 = until cleared (sub_4306D0 restarts it as 31); sub_43EC80/sub_43EDE0 set duration+120
 int colorPhase; // +0x004 [traced] sub_43EDA0: (colorPhase++ % 3) indexes the colour table dword_5A1AC4
 int tintR; // +0x008 [traced] reset to 255 (Obj_InitDefaults, sub_430830); written by applyFn sub_43EDA0
 int tintG; // +0x00C [traced] same as tintR
 int tintB; // +0x010 [traced] same as tintR
 int addR; // +0x014 [traced] reset to 0; sub_43EDE0 loads dword_5A1AF4..AFC
 int addG; // +0x018 [traced] same as addR
 int addB; // +0x01C [traced] same as addR
 Gof2ObjCTintFxKind kind; // +0x020 [traced] sub_43EC80 sets 1, sub_43EDE0 sets 2, expiry resets to 0
 void * tickFn; // +0x024 [traced] default nullsub_2; sub_43ECD0 / sub_43EE70 (hit flash tick + SFX 91/92) installed; called by sub_49E120 as (fn)(slot, obj)
 void * applyFn; // +0x028 [traced] default nullsub_1; sub_43EDA0 (colour cycle) installed; called by sub_49E120 once per drawn frame
 int duration; // +0x02C [traced] sub_43EC80/sub_43EDE0 arg; tick fns count it down and clear the slot at 0; sub_4306D0 / sub_4317B0 set 30 for infinite flashes
 int period; // +0x030 [traced] 6 (kind 1) / 12 (kind 2): reload value of periodCounter
 int periodCounter; // +0x034 [traced] set 1 at start; tick fns count down, at 0 spawn spark sub_49CF20(91/92) and reload
 unsigned char unused_38[8]; // +0x038 [unused] no function touches slot+0x38/+0x3C (the only consumers are the slot helpers above, all this-relative; none read dwords 14/15)
};
struct Gof2ObjCPendingTransition {   // 20 bytes
 void * rule; // +0x000 [traced] sub_4353C0 (Obj_ApplyPendingTransition) runs sub_42F7A0(rule,1/2,0,obj) on it; sub_4303B0 stores; sub_4303E0/sub_430410/sub_430450/sub_430490 clear
 int deferWhileFrozen; // +0x004 [traced] sub_4353C0 first line: if non-zero and (hitStopFrames||eventFreezeFrames) the transition waits
 int clearCancelLock; // +0x008 [traced] sub_4353C0: non-zero zeroes cancelLockFrames and calls sub_43EAE0
 Gof2ObjCPendingEnterFlags enterFlags; // +0x00C [traced] sub_4353C0 bit0 -> Obj_ResetMoveTracks + sub_4306D0 full state reset (+effect 60060); bit3 set by sub_433100
 Gof2ObjCPendingCondFlags condFlags; // +0x010 [traced] sub_4353C0: bit0 hit-confirm check vs move-use record, bit1 sub_4300B0, bit2 ++ move-use counter; bit2 set by sub_4331C0
};
struct Gof2ObjCPositionBlock {   // 48 bytes
 struct Gof2ObjCPositionBlock * parentRef; // +0x000 [traced] Obj_UpdatePositionFromParent: parent object whose world X/Y are added; sub_438660 sets it to obj+0x230 (chain parent)
 int inheritX; // +0x004 [traced] Obj_UpdatePositionFromParent: add parent worldX; sub_434F50 clears; Obj_ResetOnDetach sets 1
 int inheritY; // +0x008 [traced] same for Y
 int inheritZ; // +0x00C [traced] flag tested for Z (the code assigns localZ unchanged - the add is dead); sub_434F50 clears
 int localX; // +0x010 [traced] Obj_BindCharaRecord arg; moved by sub_438270/sub_438350/sub_49DB20 (position tween); world = local (+ parent)
 int localY; // +0x014 [traced] same
 int localZ; // +0x018 [traced] Obj_BindCharaRecord sets 0, ScriptVm_LoadArgsForObj loads from script
 int worldX; // +0x01C [traced] Obj_UpdatePositionFromParent output; read by hit/knock-direction code (HitJudge_SetKnockDirection, Obj_TestRelativePosition)
 int worldY; // +0x020 [traced] same
 int worldZ; // +0x024 [traced] same
 int prevLocalX; // +0x028 [traced] sub_49E120 latches localX each tick; sub_49DB20 reads it
 int prevLocalY; // +0x02C [traced] sub_49E120 latches localY; Obj_ApplyLandingFrameJump (landing test)
};
struct Gof2ObjCEffectList {   // 12 bytes
 int unused_00; // +0x000 [unused] only written 0 by Obj_InitDefaults; sub_442180/sub_442430/sub_4422E0/ObjEffect_RemoveById never touch it
 int dirty; // +0x004 [traced] sub_442180/sub_442430/ObjEffect_RemoveById/sub_4422E0 set 1 on any change; sub_442340 (aggregate) clears; also set by sub_49B940.. effect adders
 void * head; // +0x008 [traced] head of the singly linked list of 88-byte ObjEffect nodes (next at node+84); ObjEffect_Alloc pool g_ObjEffectPool_*
};
struct Gof2ObjCZoomBlock {   // 32 bytes
 int zoomX; // +0x000 [traced] sub_42EB00/sub_42EB60 output = product of the scale layers (256 = 100%); passed to the mover (*(obj+724))(&localX,&localY,zoomX,zoomY,..) by Obj_ApplyLandingFrameJump / ScriptVm_LoadArgsForObj
 int zoomY; // +0x004 [traced] same
 int scaleAX; // +0x008 [traced] layer A, neutral 256; sub_42EB60 sets 256 for C, multiplies A*B; sub_42EB00 multiplies A*C
 int scaleAY; // +0x00C [traced] same
 int scaleBX; // +0x010 [traced] layer B (HitJudge_ApplyHit writes it); sub_42EB00 resets to 256
 int scaleBY; // +0x014 [traced] same
 int scaleCX; // +0x018 [traced] layer C; sub_42EB60 resets to 256
 int scaleCY; // +0x01C [traced] same
};
struct Gof2ObjCSharedNodes {   // 16 bytes
 void * nodeA4; // +0x000 [traced] Fighter_AllocSharedSubNodes 0x42E9C0 (esi+0x38) from SubNodeA4_Alloc; per-fighter status record (+8/+12 ints, +28 flag, +52 sub-object, +96, +112); read as obj[1173] by ~70 functions (sub_433990, sub_433D00, sub_43B560, ...)
 void * node8A; // +0x004 [traced] Fighter_AllocSharedSubNodes (esi+0x3C) refcounted node pool g_SubNode8A_*; copied+refcounted by sub_42EA60
 void * node18; // +0x008 [traced] Fighter_AllocSharedSubNodes (esi+0x40) pool g_SubNode18_* (refcount at +0x10); +8/+12 are scale ints reset by sub_43DD40, read by sub_4944B0
 void * node8B; // +0x00C [traced] Fighter_AllocSharedSubNodes (esi+0x44) pool g_SubNode8B_*; released by Fighter_ReleaseSharedSubNodes
};
struct Gof2ObjCEtcBoxSet {   // 56 bytes
 void * vtable; // +0x000 [traced] &CAppHanteiEtc::vftable (Obj_ConstructPoolSlot); vf0 sub_414300 returns boxCount
 int boxCount; // +0x004 [traced] CAppHanteiEtc__vf7 0x449750 (frame etcBoxIdx[3] -> boxes); vf1/vf3 reset
 void * boxPtr0; // +0x008 [traced] vf7: &anime.view.boxes[idx] (8-byte Gof2BoxRect); vf2 sub_4499D0 returns the rect of index i
 void * boxPtr1_slotNo0; // +0x00C [traced] vf7 stores boxPtr[n] at +8+4n AND slotNo[n] at +0xC+4n, so the slot-number array overlaps the pointer array (slot numbers are never read)
 void * boxPtr2_slotNo1; // +0x010 [traced] same overlap
 int lastSlotNo; // +0x014 [traced] slotNo[2] when three boxes were collected (write-only)
 unsigned char unused_18[4]; // +0x018 [unused] no vf touches dword 6
 int clearedOnly_1C; // +0x01C [traced] only ever cleared to 0 (vf5 CAppHanteiEtc__vf5, sub_42E600); no reader
 unsigned char unused_20[8]; // +0x020 [unused] no vf touches dwords 8/9
 int attackEvent4Latch; // +0x028 [traced] vf7 sets 1 when frame attackEventFlags&4, vf3 clears; no reader
 unsigned int contactKindMask; // +0x02C [traced] sub_43C1E0 ORs attack-record class bits shifted by the box slot over the object chain (called from sub_43C530 cases 2/3); sub_436280 (script cond ops 0/2/3) tests it; vf4 CAppHanteiEtc__vf4 clears
 Gof2ObjCContactSource contactSource; // +0x030 [traced] sub_43C1A0: |=1 own, |=2 chain children; read by sub_42F5B0 category 0x20; vf4 clears
 unsigned int contactMask; // +0x034 [traced] sub_43C1A0 ORs mask (children: mask<<8); sub_42F5B0 ORs it into the contact test; vf4 clears
};
struct Gof2ObjCSousaiBoxSet {   // 48 bytes
 void * vtable; // +0x000 [traced] &CAppHanteiSousai::vftable
 int boxCount; // +0x004 [traced] CAppHanteiSousai__vf6 0x449820 (frame sousaiBoxIdx[3]); sub_4967C0/sub_49B360/sub_49B520/sub_43C530 gate on vf0 > 0
 void * boxPtr[3]; // +0x008 [traced] vf6 stores &boxes[idx] at +8+4n; vf2 sub_4499D0 getter
 int slotNo[3]; // +0x014 [traced] vf6 stores the slot number 0/1/2 at +0x14+4n (write-only)
 Gof2ObjCContactSource contactSource; // +0x020 [traced] sub_43C3A0 |=1 / |=2; sub_42F5B0 category 0x10; vf4 clears
 unsigned int contactMask; // +0x024 [traced] sub_43C3A0; sub_42F5B0; vf4 clears
 int clashMark[2]; // +0x028 [traced] HitJudge_OnAttackGuarded 0x4967C0 / sub_49B360 / sub_49B520 index it by (obj+0x5D0 + obj+0x5D4) (side + sub-slot, 0..1 in Battle_ResetPoolAndSpawnFighters) = already clashed with that attacker; vf4 clears
};
struct Gof2ObjCTobiBoxSet {   // 44 bytes
 void * vtable; // +0x000 [traced] &CAppHanteiTobi::vftable
 int boxCount; // +0x004 [traced] CAppHanteiTobi__vf6 0x449910 (frame tobiBoxIdx[3]); sub_49C4D0 gates on vf0
 void * boxPtr[3]; // +0x008 [traced] vf6 stores &boxes[idx] at +8+4n; vf2 sub_4499D0 getter
 int slotNo[3]; // +0x014 [traced] vf6 stores slot number 0/1/2 at +0x14+4n (write-only)
 Gof2ObjCContactSource contactSource; // +0x020 [traced] sub_43C3E0 |=1 / |=2; sub_42F5B0 category 0x40; CAppHanteiTobi__vf4 clears
 unsigned int contactMask; // +0x024 [traced] sub_43C3E0; sub_42F5B0; vf4 clears
 int zeroedOnly_28; // +0x028 [traced] only ever zeroed (sub_430630, sub_4306D0 at 0x1070); no reader
};
struct Gof2ObjTailC {   // bytes 0xC40..0x1263 of Obj (1572 bytes), standalone view for IDA
 int hitByAttackerActionTail[222]; // +0x000 [traced] tail of the 2x256 dword table that starts at Obj+0x7B8 (owner of that start: part B). It is the second half of CAppHanteiYarare-sized hit memory at Obj+0x6F0 (sub_43E3D0 memset(a2+200,0,0x800)). sub_495B60 0x495E62: index = attackerSlot(+0x5CC)*256 + attackerAction(+0x26C), flag set to 1 on first connect so later hits of the same attacker action are scaled (guard-gauge call sub_440E60). This part = row 1 entries 34..255. Scanned: no other [reg+disp] operand with disp 0xC40..0xFB7 uses an Obj base (hits are CSceneGame/CSceneNetMenu/CWSServerClient/sub_448270/sub_4213A0)
 Gof2ObjCArmorGauge armorGauge; // +0x378 [traced] see struct; tail of CAppHanteiYarare hit memory (+0x8C8 .. +0x8E3 from Obj+0x6F0, cleared by sub_43E3D0)
 int hudPopupParam; // +0x394 [traced reads] HudPopupBoard_Tick 0x47AE90 reads it; the only write is the zero in sub_43E3D0, so always 0
 int guardGauge; // +0x398 [traced] Obj_InitDefaults = DefStatus+16 (max); sub_440E60 adds hit/guard damage and clamps to [0,max]; sub_4CDC70 regenerates per action (g_ControllerManage[67][action]); sub_495B60 passes &guardGauge
 int hitGraceStage; // +0x39C [traced] set 1 by HitJudge_ApplyHit when a player-class object is in action 40; HitJudge_ComputeDamage / sub_495620 / sub_4952F0: while it is 1 or 2 it is incremented and the extra damage argument is forced to 0 [inferred meaning]
 Gof2ObjCEtcBoxSet etcBoxes; // +0x3A0 [traced] embedded CAppHanteiEtc (Obj_ConstructPoolSlot 0xFE0)
 Gof2ObjCSousaiBoxSet sousaiBoxes; // +0x3D8 [traced] embedded CAppHanteiSousai (+0x1018)
 Gof2ObjCTobiBoxSet tobiBoxes; // +0x408 [traced] embedded CAppHanteiTobi (+0x1048)
 int eventFreezeFrames; // +0x434 [traced] sub_49E120: >0 -> decrement and run only the frozen tick (sub_49E0B0); raised to DefStatus+536 for the round-event winner by sub_4A3A80; tested by sub_4353C0, Battle_TickIdleStanceCounters
 int hitStopFrames; // +0x438 [traced] Obj_RaiseHitStopFrames / HitJudge_ApplyHitStop set it; sub_49E120 counts it down and skips logic; sub_437C70 = 0xFFFF; CComWorkBase/CComWork read >=30 as freeze
 int hitStopLatched; // +0x43C [traced] Obj_InitDefaults = 1, sub_437C70/Obj_RaiseHitStopFrames set 1, sub_430C80 clears; Obj_RaiseHitStopFrames refuses a lower request while it is 1
 int reactionHoldFrames; // +0x440 [traced] sub_434F90 sets 8 / script value when a knock-back starts; sub_49E120 counts it down (at 1 and flag 0x10000 sets obj+0x1F8), logic that advances the time accumulator runs only at 0 [inferred meaning]
 int shakeFrames; // +0x444 [traced] sub_496C60 sets; sub_496CB0 getter; sub_49E120 counts down
 int shakeAmplitude; // +0x448 [traced] sub_496C60 arg; sub_49E120 shakeOffsetX = dword_5A2388[facing]*amplitude*(phase%2)
 int shakePhase; // +0x44C [traced] sub_496C60 zeroes; sub_49E120 ++
 int shakeOffsetX; // +0x450 [traced] sub_49E120 output; read through shakeOffsetRef by sub_49ECB0 / DrawHelper_Draw
 int * shakeOffsetRef; // +0x454 [traced] Obj_InitDefaults = &shakeOffsetX; DrawHelper_Draw / sub_49ECB0 dereference it
 int frameTickCounter; // +0x458 [traced] sub_49E120 ++ (saturating -1); sub_430490 zeroes it when a pending pattern-frame change commits
 int actionAgeTicks; // +0x45C [traced] sub_49E120 ++ (saturating); sub_430490 zeroes on a pending action change
 int actionScriptAge; // +0x460 [traced] Obj_ScriptTick ++ (saturating); sub_430490 zeroes on action change; sub_495620 scales damage by it (DefStatus+764/768/772)
 Gof2ObjCArmorState armor; // +0x464 [traced] RBO-style armor state, see struct
 unsigned char unused_10C8[4]; // +0x488 [unused] only zero-written by Obj_InitDefaults; no reader (no [reg+10C8h] operand elsewhere)
 int hitBonusCount; // +0x48C [traced] HitJudge_CollectPairs ++ (saturating) when the state script (3,5) answers 2; sub_4944B0 multiplies damage by 1+count*DefStatus(276/280); Obj_GetNormalCancelState family gates on it >0
 void * moveUseRecord; // +0x490 [traced] Obj_BindCharaRecordAndStartAction arg a9; per-player record: +0 counter, +4..+0x53 20 move counters (sub_42F120 index of action-60), +0x54 flag, +0x58 last action; sub_4306D0/sub_430630 clear it
 void * scriptTables; // +0x494 [traced] Obj_BindCharaRecordAndStartAction = charaContext+64588; +8 = array of 24-byte transition rules (sub_4365D0 / sub_430910)
 void * keyCmdRef; // +0x498 [traced] refcounted key-command record (refcount at +72): Obj_ShareKeyCmdRef copies, Obj_ReleaseSubNodes releases, HitJudge_ApplyKnockDirAndFacing reads +8&0xC
 int charaSlotIndex; // +0x49C [traced] Obj_BindCharaRecord arg a1 (record = ctx+32292*idx+4); sub_4B2110/CharaSelect clear; ScriptVm arg
 void * charaContext; // +0x4A0 [traced] Obj_BindCharaRecord arg a3; +64600 = script bank used by every ScriptVm_CallSyncScript((obj+0x10E0)+64600,..)
 int incomingAttackFrames; // +0x4A4 [traced] ObjScan_StoreAttackGuardInfo: frames until the scanned opponent attack frame; Obj_ScriptTick counts it down; AI hint
 int incomingAttackGuardClass; // +0x4A8 [traced] ObjScan_StoreAttackGuardInfo: 1..3 from attack record +0x7C&3 (0 = none)
 Gof2ObjCTintFxSlot tintFx[3]; // +0x4AC [traced] 3 slots stride 64: sub_49E120 ticks all three; HitJudge_StartHitEffect/sub_43EC80/sub_43EDE0 only start slot 0; slots 1/2 are initialised and reset (sub_43DD40) but never started
 void * tintFxRef[3]; // +0x56C [traced] Obj_InitDefaults stores &tintFx[0..2]; write-only (no [reg+11ACh..11B4h] reader)
 Gof2ObjCFrameEventFlags frameEventFlags; // +0x578 [traced] Obj_SetActionAndRunScript / Obj_ScriptTick set 0x1000000F; sub_49E120/sub_49E0B0 clear bits 4|8 after Obj_ApplyLandingFrameJump; sub_49E830 consumes bit 2; sub_4CDEE0 consumes 0x10000000; sub_48FA20 clears 4
 Gof2ObjCPendingTransition pendingTransition; // +0x57C [traced] queued state-change request, applied by sub_4353C0 (renamed Obj_ApplyPendingTransition)
 unsigned int contactFlagsMisc; // +0x590 [traced reads] sub_43C530 tests bit 4; only ever zeroed (sub_4306D0, sub_430630, sub_430770, CharaSelect_*Pose) -> effectively 0
 int cancelLockFrames; // +0x594 [traced] sub_43C460 sets DefStatus+1144 after a hit; sub_430C80 counts down; Obj_GetNormalCancelState* return "not cancellable" while !=0; sub_4353C0 clears
 int hitDuringAction16; // +0x598 [traced] sub_43C530 sets 1 when the victim is in action 16; sub_433D00 cancel conditions read it; cleared by sub_4306D0
 int hitDuringAction19; // +0x59C [traced] same for action 19
 Gof2ObjCPositionBlock position; // +0x5A0 [traced] see struct (RBO localX/Y/Z + posX/Y/Z + prevLocalX/Y)
 Gof2ObjCEffectList effects; // +0x5D0 [traced] ObjEffect list: sub_442180 append, sub_442430 tick, sub_4422E0 clear, ObjEffect_RemoveById, sub_442340 aggregate
 Gof2ObjCRuleOverrideFlags ruleOverrideFlags; // +0x5DC [traced reads] sub_42F7A0 (state-rule evaluator) bit 8 forces the rule to fail; only zeroed elsewhere (ctor, Obj_InitDefaults, sub_43DD40)
 Gof2ObjCHitEffectStyleFlags hitEffectStyleFlags; // +0x5E0 [traced reads] HitJudge_ResolveTick bits 1/2/4 pick hit-effect style 1/2/3 when the attack record has none; bit 8 selects DefStatus+1104/1108/1112 guard bonus (sub_497120, sub_497160, HitJudge_ApplyHit); only zeroed elsewhere
 int fixedHalfRate; // +0x5E4 [traced] Obj_SetActionAndRunScript sets; sub_49E120: non-zero -> time accumulator drops 128 per tick instead of timeScale
 int timeAccum; // +0x5E8 [traced] Obj_SetActionAndRunScript = 128; Obj_ScriptTick / sub_4353C0 += 256; sub_49E120 -= rate each tick, <=0 skips the script step (slow-motion / half speed)
 int timeScale; // +0x5EC [traced] default 128 (ctor); sub_438660 sets 256/copies parent or child; CComWorkBase/CComWorkNormal compare >= 0.875*timeScaleBase
 int timeScaleBase; // +0x5F0 [traced] default 128; sub_438660; sub_43DD40 recomputes timeScale from it
 Gof2ObjCZoomBlock zoom; // +0x5F4 [traced] see struct
 Gof2ObjCSharedNodes sharedNodes; // +0x614 [traced] Fighter_AllocSharedSubNodes 0x42E9C0 fills +0x38..+0x44 of obj+0x121C; Obj_ReleaseSubNodes/Fighter_ReleaseSharedSubNodes free them
};
