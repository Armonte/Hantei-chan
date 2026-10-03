# Generator for gof2obj_partC_types.h and gof2obj_partC_fields.py (GOF2 Obj bytes 0xC40..0x1263).
# Run: python3 gof2obj_partC_gen.py   (writes both files next to itself and asserts every size/offset).
import os
D = os.path.dirname(os.path.abspath(__file__))

ENUMS = [
 "enum Gof2ObjCFrameEventFlags : unsigned int { FEV_RESERVED_01=0x1, FEV_ADVANCE_MOTION_HISTORY=0x2, FEV_LANDING_FRAME_JUMP_PENDING=0x4, FEV_CLEARED_WITH_LANDING_08=0x8, FEV_FRAME_ENTERED=0x10000000 };",
 "enum Gof2ObjCPendingEnterFlags : unsigned int { PEF_RESET_STATE_ON_ENTER=0x1, PEF_GUARD_SPECIAL_CANCEL=0x8 };",
 "enum Gof2ObjCPendingCondFlags : unsigned int { PCF_NEEDS_HIT_CONFIRM=0x1, PCF_ADD_SPECIAL_GAUGE=0x2, PCF_BUMP_MOVE_USE_COUNTER=0x4 };",
 "enum Gof2ObjCHitEffectStyleFlags : unsigned int { HESF_STYLE_1=0x1, HESF_STYLE_2=0x2, HESF_STYLE_3=0x4, HESF_GUARD_BONUS_TABLE=0x8 };",
 "enum Gof2ObjCRuleOverrideFlags : unsigned int { ROF_FAIL_STANCE_RULES=0x8 };",
 "enum Gof2ObjCContactSource : unsigned int { CSRC_OWN_BOX=0x1, CSRC_INHERITED_FROM_CHAIN=0x2 };",
 "enum Gof2ObjCTintFxKind : unsigned int { TFK_NONE=0, TFK_HIT_FLASH_CYCLE=1, TFK_HIT_FLASH_FIXED=2 };",
]

# struct name -> (size, [(rel_off, size, ctype, name, evidence)])
STRUCTS = {}
STRUCTS['Gof2ObjCArmorGauge'] = (28, [
 (0x00,4,'int','enabled','[traced reads] tested by sub_433990, HitJudge_CollectPairs, HitJudge_ResolveSecondaryBoxClashes, sub_4383D0, sub_438A70 op39, sub_4A3A80; NO writer anywhere (Obj_InitDefaults/sub_43E3D0 zero it) so this guard-point gauge is never switched on'),
 (0x04,4,'int','gauge','[traced] compared with DefStatus+1052 in HitJudge_CollectPairs (hit passes when gauge > threshold); +DefStatus+540 by sub_4A3A80, +DefStatus+1168 by sub_438A70 op39; decays by DefStatus+984/988 in sub_42ED90'),
 (0x08,4,'int','timerA','[traced] set to DefStatus+236 by Obj_SpawnFrameEffectRecord; += DefStatus+528 (sub_4383D0) / +1156 (sub_438A70 op39); counts down in sub_42ED90'),
 (0x0C,4,'int','timerB','[traced] sub_433990 / sub_4339xx: no flinch while timerB<=0 and gauge<=DefStatus+1056; += DefStatus+1160 (sub_438A70 op39); counts down in sub_42ED90'),
 (0x10,4,'int','breakTimer','[traced] HitJudge_CollectPairs / ResolveSecondaryBoxClashes: hit goes through while >0; += DefStatus+532 (sub_4383D0) / +1164 (sub_438A70 op39); counts down in sub_42ED90'),
 (0x14,4,'unsigned char','unused_14[4]','[unused] no [reg+0FCCh] operand in any function (all 2171 [reg+disp] hits for disp 0xC40..0x1263 scanned) and sub_43E3D0 (reset) skips it'),
 (0x18,4,'int','suppressed','[traced] HitJudge_ResolveSecondaryBoxClashes sets 1 (0x49B230), Obj_SpawnFrameEffectRecord clears; sub_42ED90 skips the countdown while ==1'),
])
STRUCTS['Gof2ObjCArmorState'] = (36, [
 (0x00,4,'int','active','[traced] sub_430C80 derives it (1 when timerFrames!=0 || hitCountA>0 || hitCountB>0); sub_496F10 sets 1 on a successful roll; read by HitJudge_ApplyHitStop, sub_496DD0, sub_47B700. Same role as RBO RboP3ArmorState.armorActive'),
 (0x04,4,'int','clearPending','[traced] set 1 by HitJudge_ApplyHit / sub_49E120 when a timer runs out, tested+cleared in sub_430C80 which then zeroes the whole state'),
 (0x08,4,'int','chancePercent','[traced] state op 0 (sub_438A70) sets it; sub_496F10 rolls Rng_Range(100) < value and lowers it by 2 per call; RBO armor chancePercent'),
 (0x0C,4,'int','strainCount','[traced] ++ in HitJudge_ResolveTick (0x4996A0) on armor success; RBO armor strainCount'),
 (0x10,4,'int','timerFrames','[traced] state op 1 (sub_438A70) sets it; sub_49E120 counts it down, at 0 sets clearPending'),
 (0x14,4,'int','hitCountA','[traced] state op 2: armor that survives N hits; HitJudge_ApplyHit decrements per hit'),
 (0x18,4,'int','hitTimerA','[traced] state op 2: frame limit of the hit-count armor; sub_49E120 counts it down'),
 (0x1C,4,'int','hitCountB','[traced] state op 3: second hit-count armor; HitJudge_ApplyHit decrements per hit'),
 (0x20,4,'int','hitTimerB','[traced] state op 3: frame limit of the second armor; sub_49E120 counts it down'),
])
STRUCTS['Gof2ObjCTintFxSlot'] = (64, [
 (0x00,4,'int','lifetimeTicks','[traced] sub_49E120 (3 slots, stride 64): while !=0 it counts down (>0) and calls tickFn; -1 = until cleared (sub_4306D0 restarts it as 31); sub_43EC80/sub_43EDE0 set duration+120'),
 (0x04,4,'int','colorPhase','[traced] sub_43EDA0: (colorPhase++ % 3) indexes the colour table dword_5A1AC4'),
 (0x08,4,'int','tintR','[traced] reset to 255 (Obj_InitDefaults, sub_430830); written by applyFn sub_43EDA0'),
 (0x0C,4,'int','tintG','[traced] same as tintR'),
 (0x10,4,'int','tintB','[traced] same as tintR'),
 (0x14,4,'int','addR','[traced] reset to 0; sub_43EDE0 loads dword_5A1AF4..AFC'),
 (0x18,4,'int','addG','[traced] same as addR'),
 (0x1C,4,'int','addB','[traced] same as addR'),
 (0x20,4,'Gof2ObjCTintFxKind','kind','[traced] sub_43EC80 sets 1, sub_43EDE0 sets 2, expiry resets to 0'),
 (0x24,4,'void *','tickFn','[traced] default nullsub_2; sub_43ECD0 / sub_43EE70 (hit flash tick + SFX 91/92) installed; called by sub_49E120 as (fn)(slot, obj)'),
 (0x28,4,'void *','applyFn','[traced] default nullsub_1; sub_43EDA0 (colour cycle) installed; called by sub_49E120 once per drawn frame'),
 (0x2C,4,'int','duration','[traced] sub_43EC80/sub_43EDE0 arg; tick fns count it down and clear the slot at 0; sub_4306D0 / sub_4317B0 set 30 for infinite flashes'),
 (0x30,4,'int','period','[traced] 6 (kind 1) / 12 (kind 2): reload value of periodCounter'),
 (0x34,4,'int','periodCounter','[traced] set 1 at start; tick fns count down, at 0 spawn spark sub_49CF20(91/92) and reload'),
 (0x38,8,'unsigned char','unused_38[8]','[unused] no function touches slot+0x38/+0x3C (the only consumers are the slot helpers above, all this-relative; none read dwords 14/15)'),
])
STRUCTS['Gof2ObjCPendingTransition'] = (20, [
 (0x00,4,'void *','rule','[traced] sub_4353C0 (Obj_ApplyPendingTransition) runs sub_42F7A0(rule,1/2,0,obj) on it; sub_4303B0 stores; sub_4303E0/sub_430410/sub_430450/sub_430490 clear'),
 (0x04,4,'int','deferWhileFrozen','[traced] sub_4353C0 first line: if non-zero and (hitStopFrames||eventFreezeFrames) the transition waits'),
 (0x08,4,'int','clearCancelLock','[traced] sub_4353C0: non-zero zeroes cancelLockFrames and calls sub_43EAE0'),
 (0x0C,4,'Gof2ObjCPendingEnterFlags','enterFlags','[traced] sub_4353C0 bit0 -> Obj_ResetMoveTracks + sub_4306D0 full state reset (+effect 60060); bit3 set by sub_433100'),
 (0x10,4,'Gof2ObjCPendingCondFlags','condFlags','[traced] sub_4353C0: bit0 hit-confirm check vs move-use record, bit1 sub_4300B0, bit2 ++ move-use counter; bit2 set by sub_4331C0'),
])
STRUCTS['Gof2ObjCPositionBlock'] = (48, [
 (0x00,4,'struct Gof2ObjCPositionBlock *','parentRef','[traced] Obj_UpdatePositionFromParent: parent object whose world X/Y are added; sub_438660 sets it to obj+0x230 (chain parent)'),
 (0x04,4,'int','inheritX','[traced] Obj_UpdatePositionFromParent: add parent worldX; sub_434F50 clears; Obj_ResetOnDetach sets 1'),
 (0x08,4,'int','inheritY','[traced] same for Y'),
 (0x0C,4,'int','inheritZ','[traced] flag tested for Z (the code assigns localZ unchanged - the add is dead); sub_434F50 clears'),
 (0x10,4,'int','localX','[traced] Obj_BindCharaRecord arg; moved by sub_438270/sub_438350/sub_49DB20 (position tween); world = local (+ parent)'),
 (0x14,4,'int','localY','[traced] same'),
 (0x18,4,'int','localZ','[traced] Obj_BindCharaRecord sets 0, ScriptVm_LoadArgsForObj loads from script'),
 (0x1C,4,'int','worldX','[traced] Obj_UpdatePositionFromParent output; read by hit/knock-direction code (HitJudge_SetKnockDirection, Obj_TestRelativePosition)'),
 (0x20,4,'int','worldY','[traced] same'),
 (0x24,4,'int','worldZ','[traced] same'),
 (0x28,4,'int','prevLocalX','[traced] sub_49E120 latches localX each tick; sub_49DB20 reads it'),
 (0x2C,4,'int','prevLocalY','[traced] sub_49E120 latches localY; Obj_ApplyLandingFrameJump (landing test)'),
])
STRUCTS['Gof2ObjCEffectList'] = (12, [
 (0x00,4,'int','unused_00','[unused] only written 0 by Obj_InitDefaults; sub_442180/sub_442430/sub_4422E0/ObjEffect_RemoveById never touch it'),
 (0x04,4,'int','dirty','[traced] sub_442180/sub_442430/ObjEffect_RemoveById/sub_4422E0 set 1 on any change; sub_442340 (aggregate) clears; also set by sub_49B940.. effect adders'),
 (0x08,4,'void *','head','[traced] head of the singly linked list of 88-byte ObjEffect nodes (next at node+84); ObjEffect_Alloc pool g_ObjEffectPool_*'),
])
STRUCTS['Gof2ObjCZoomBlock'] = (32, [
 (0x00,4,'int','zoomX','[traced] sub_42EB00/sub_42EB60 output = product of the scale layers (256 = 100%); passed to the mover (*(obj+724))(&localX,&localY,zoomX,zoomY,..) by Obj_ApplyLandingFrameJump / ScriptVm_LoadArgsForObj'),
 (0x04,4,'int','zoomY','[traced] same'),
 (0x08,4,'int','scaleAX','[traced] layer A, neutral 256; sub_42EB60 sets 256 for C, multiplies A*B; sub_42EB00 multiplies A*C'),
 (0x0C,4,'int','scaleAY','[traced] same'),
 (0x10,4,'int','scaleBX','[traced] layer B (HitJudge_ApplyHit writes it); sub_42EB00 resets to 256'),
 (0x14,4,'int','scaleBY','[traced] same'),
 (0x18,4,'int','scaleCX','[traced] layer C; sub_42EB60 resets to 256'),
 (0x1C,4,'int','scaleCY','[traced] same'),
])
STRUCTS['Gof2ObjCSharedNodes'] = (16, [
 (0x00,4,'void *','nodeA4','[traced] Fighter_AllocSharedSubNodes 0x42E9C0 (esi+0x38) from SubNodeA4_Alloc; per-fighter status record (+8/+12 ints, +28 flag, +52 sub-object, +96, +112); read as obj[1173] by ~70 functions (sub_433990, sub_433D00, sub_43B560, ...)'),
 (0x04,4,'void *','node8A','[traced] Fighter_AllocSharedSubNodes (esi+0x3C) refcounted node pool g_SubNode8A_*; copied+refcounted by sub_42EA60'),
 (0x08,4,'void *','node18','[traced] Fighter_AllocSharedSubNodes (esi+0x40) pool g_SubNode18_* (refcount at +0x10); +8/+12 are scale ints reset by sub_43DD40, read by sub_4944B0'),
 (0x0C,4,'void *','node8B','[traced] Fighter_AllocSharedSubNodes (esi+0x44) pool g_SubNode8B_*; released by Fighter_ReleaseSharedSubNodes'),
])
STRUCTS['Gof2ObjCEtcBoxSet'] = (56, [
 (0x00,4,'void *','vtable','[traced] &CAppHanteiEtc::vftable (Obj_ConstructPoolSlot); vf0 sub_414300 returns boxCount'),
 (0x04,4,'int','boxCount','[traced] CAppHanteiEtc__vf7 0x449750 (frame etcBoxIdx[3] -> boxes); vf1/vf3 reset'),
 (0x08,4,'void *','boxPtr0','[traced] vf7: &anime.view.boxes[idx] (8-byte Gof2BoxRect); vf2 sub_4499D0 returns the rect of index i'),
 (0x0C,4,'void *','boxPtr1_slotNo0','[traced] vf7 stores boxPtr[n] at +8+4n AND slotNo[n] at +0xC+4n, so the slot-number array overlaps the pointer array (slot numbers are never read)'),
 (0x10,4,'void *','boxPtr2_slotNo1','[traced] same overlap'),
 (0x14,4,'int','lastSlotNo','[traced] slotNo[2] when three boxes were collected (write-only)'),
 (0x18,4,'unsigned char','unused_18[4]','[unused] no vf touches dword 6'),
 (0x1C,4,'int','clearedOnly_1C','[traced] only ever cleared to 0 (vf5 CAppHanteiEtc__vf5, sub_42E600); no reader'),
 (0x20,8,'unsigned char','unused_20[8]','[unused] no vf touches dwords 8/9'),
 (0x28,4,'int','attackEvent4Latch','[traced] vf7 sets 1 when frame attackEventFlags&4, vf3 clears; no reader'),
 (0x2C,4,'unsigned int','contactKindMask','[traced] sub_43C1E0 ORs attack-record class bits shifted by the box slot over the object chain (called from sub_43C530 cases 2/3); sub_436280 (script cond ops 0/2/3) tests it; vf4 CAppHanteiEtc__vf4 clears'),
 (0x30,4,'Gof2ObjCContactSource','contactSource','[traced] sub_43C1A0: |=1 own, |=2 chain children; read by sub_42F5B0 category 0x20; vf4 clears'),
 (0x34,4,'unsigned int','contactMask','[traced] sub_43C1A0 ORs mask (children: mask<<8); sub_42F5B0 ORs it into the contact test; vf4 clears'),
])
STRUCTS['Gof2ObjCSousaiBoxSet'] = (48, [
 (0x00,4,'void *','vtable','[traced] &CAppHanteiSousai::vftable'),
 (0x04,4,'int','boxCount','[traced] CAppHanteiSousai__vf6 0x449820 (frame sousaiBoxIdx[3]); sub_4967C0/sub_49B360/sub_49B520/sub_43C530 gate on vf0 > 0'),
 (0x08,12,'void *','boxPtr[3]','[traced] vf6 stores &boxes[idx] at +8+4n; vf2 sub_4499D0 getter'),
 (0x14,12,'int','slotNo[3]','[traced] vf6 stores the slot number 0/1/2 at +0x14+4n (write-only)'),
 (0x20,4,'Gof2ObjCContactSource','contactSource','[traced] sub_43C3A0 |=1 / |=2; sub_42F5B0 category 0x10; vf4 clears'),
 (0x24,4,'unsigned int','contactMask','[traced] sub_43C3A0; sub_42F5B0; vf4 clears'),
 (0x28,8,'int','clashMark[2]','[traced] HitJudge_OnAttackGuarded 0x4967C0 / sub_49B360 / sub_49B520 index it by (obj+0x5D0 + obj+0x5D4) (side + sub-slot, 0..1 in Battle_ResetPoolAndSpawnFighters) = already clashed with that attacker; vf4 clears'),
])
STRUCTS['Gof2ObjCTobiBoxSet'] = (44, [
 (0x00,4,'void *','vtable','[traced] &CAppHanteiTobi::vftable'),
 (0x04,4,'int','boxCount','[traced] CAppHanteiTobi__vf6 0x449910 (frame tobiBoxIdx[3]); sub_49C4D0 gates on vf0'),
 (0x08,12,'void *','boxPtr[3]','[traced] vf6 stores &boxes[idx] at +8+4n; vf2 sub_4499D0 getter'),
 (0x14,12,'int','slotNo[3]','[traced] vf6 stores slot number 0/1/2 at +0x14+4n (write-only)'),
 (0x20,4,'Gof2ObjCContactSource','contactSource','[traced] sub_43C3E0 |=1 / |=2; sub_42F5B0 category 0x40; CAppHanteiTobi__vf4 clears'),
 (0x24,4,'unsigned int','contactMask','[traced] sub_43C3E0; sub_42F5B0; vf4 clears'),
 (0x28,4,'int','zeroedOnly_28','[traced] only ever zeroed (sub_430630, sub_4306D0 at 0x1070); no reader'),
])
for n,(sz,fl) in STRUCTS.items():
    pos=0
    for off,s,t,nm,ev in fl:
        assert off==pos,(n,nm,hex(off),hex(pos)); pos+=s
    assert pos==sz,(n,pos,sz)

# Obj-level fields (absolute decimal offsets)
F = []
def f(off,size,ct,name,ev): F.append((off,size,ct,name,ev))
f(3136,888,'int','hitByAttackerActionTail[222]','[traced] tail of the 2x256 dword table that starts at Obj+0x7B8 (owner of that start: part B). It is the second half of CAppHanteiYarare-sized hit memory at Obj+0x6F0 (sub_43E3D0 memset(a2+200,0,0x800)). sub_495B60 0x495E62: index = attackerSlot(+0x5CC)*256 + attackerAction(+0x26C), flag set to 1 on first connect so later hits of the same attacker action are scaled (guard-gauge call sub_440E60). This part = row 1 entries 34..255. Scanned: no other [reg+disp] operand with disp 0xC40..0xFB7 uses an Obj base (hits are CSceneGame/CSceneNetMenu/CWSServerClient/sub_448270/sub_4213A0)')
f(4024,28,'Gof2ObjCArmorGauge','armorGauge','[traced] see struct; tail of CAppHanteiYarare hit memory (+0x8C8 .. +0x8E3 from Obj+0x6F0, cleared by sub_43E3D0)')
f(4052,4,'int','hudPopupParam','[traced reads] HudPopupBoard_Tick 0x47AE90 reads it; the only write is the zero in sub_43E3D0, so always 0')
f(4056,4,'int','guardGauge','[traced] Obj_InitDefaults = DefStatus+16 (max); sub_440E60 adds hit/guard damage and clamps to [0,max]; sub_4CDC70 regenerates per action (g_ControllerManage[67][action]); sub_495B60 passes &guardGauge')
f(4060,4,'int','hitGraceStage','[traced] set 1 by HitJudge_ApplyHit when a player-class object is in action 40; HitJudge_ComputeDamage / sub_495620 / sub_4952F0: while it is 1 or 2 it is incremented and the extra damage argument is forced to 0 [inferred meaning]')
f(4064,56,'Gof2ObjCEtcBoxSet','etcBoxes','[traced] embedded CAppHanteiEtc (Obj_ConstructPoolSlot 0xFE0)')
f(4120,48,'Gof2ObjCSousaiBoxSet','sousaiBoxes','[traced] embedded CAppHanteiSousai (+0x1018)')
f(4168,44,'Gof2ObjCTobiBoxSet','tobiBoxes','[traced] embedded CAppHanteiTobi (+0x1048)')
f(4212,4,'int','eventFreezeFrames','[traced] sub_49E120: >0 -> decrement and run only the frozen tick (sub_49E0B0); raised to DefStatus+536 for the round-event winner by sub_4A3A80; tested by sub_4353C0, Battle_TickIdleStanceCounters')
f(4216,4,'int','hitStopFrames','[traced] Obj_RaiseHitStopFrames / HitJudge_ApplyHitStop set it; sub_49E120 counts it down and skips logic; sub_437C70 = 0xFFFF; CComWorkBase/CComWork read >=30 as freeze')
f(4220,4,'int','hitStopLatched','[traced] Obj_InitDefaults = 1, sub_437C70/Obj_RaiseHitStopFrames set 1, sub_430C80 clears; Obj_RaiseHitStopFrames refuses a lower request while it is 1')
f(4224,4,'int','reactionHoldFrames','[traced] sub_434F90 sets 8 / script value when a knock-back starts; sub_49E120 counts it down (at 1 and flag 0x10000 sets obj+0x1F8), logic that advances the time accumulator runs only at 0 [inferred meaning]')
f(4228,4,'int','shakeFrames','[traced] sub_496C60 sets; sub_496CB0 getter; sub_49E120 counts down')
f(4232,4,'int','shakeAmplitude','[traced] sub_496C60 arg; sub_49E120 shakeOffsetX = dword_5A2388[facing]*amplitude*(phase%2)')
f(4236,4,'int','shakePhase','[traced] sub_496C60 zeroes; sub_49E120 ++')
f(4240,4,'int','shakeOffsetX','[traced] sub_49E120 output; read through shakeOffsetRef by sub_49ECB0 / DrawHelper_Draw')
f(4244,4,'int *','shakeOffsetRef','[traced] Obj_InitDefaults = &shakeOffsetX; DrawHelper_Draw / sub_49ECB0 dereference it')
f(4248,4,'int','frameTickCounter','[traced] sub_49E120 ++ (saturating -1); sub_430490 zeroes it when a pending pattern-frame change commits')
f(4252,4,'int','actionAgeTicks','[traced] sub_49E120 ++ (saturating); sub_430490 zeroes on a pending action change')
f(4256,4,'int','actionScriptAge','[traced] Obj_ScriptTick ++ (saturating); sub_430490 zeroes on action change; sub_495620 scales damage by it (DefStatus+764/768/772)')
f(4260,36,'Gof2ObjCArmorState','armor','[traced] RBO-style armor state, see struct')
f(4296,4,'unsigned char','unused_10C8[4]','[unused] only zero-written by Obj_InitDefaults; no reader (no [reg+10C8h] operand elsewhere)')
f(4300,4,'int','hitBonusCount','[traced] HitJudge_CollectPairs ++ (saturating) when the state script (3,5) answers 2; sub_4944B0 multiplies damage by 1+count*DefStatus(276/280); Obj_GetNormalCancelState family gates on it >0')
f(4304,4,'void *','moveUseRecord','[traced] Obj_BindCharaRecordAndStartAction arg a9; per-player record: +0 counter, +4..+0x53 20 move counters (sub_42F120 index of action-60), +0x54 flag, +0x58 last action; sub_4306D0/sub_430630 clear it')
f(4308,4,'void *','scriptTables','[traced] Obj_BindCharaRecordAndStartAction = charaContext+64588; +8 = array of 24-byte transition rules (sub_4365D0 / sub_430910)')
f(4312,4,'void *','keyCmdRef','[traced] refcounted key-command record (refcount at +72): Obj_ShareKeyCmdRef copies, Obj_ReleaseSubNodes releases, HitJudge_ApplyKnockDirAndFacing reads +8&0xC')
f(4316,4,'int','charaSlotIndex','[traced] Obj_BindCharaRecord arg a1 (record = ctx+32292*idx+4); sub_4B2110/CharaSelect clear; ScriptVm arg')
f(4320,4,'void *','charaContext','[traced] Obj_BindCharaRecord arg a3; +64600 = script bank used by every ScriptVm_CallSyncScript((obj+0x10E0)+64600,..)')
f(4324,4,'int','incomingAttackFrames','[traced] ObjScan_StoreAttackGuardInfo: frames until the scanned opponent attack frame; Obj_ScriptTick counts it down; AI hint')
f(4328,4,'int','incomingAttackGuardClass','[traced] ObjScan_StoreAttackGuardInfo: 1..3 from attack record +0x7C&3 (0 = none)')
f(4332,192,'Gof2ObjCTintFxSlot','tintFx[3]','[traced] 3 slots stride 64: sub_49E120 ticks all three; HitJudge_StartHitEffect/sub_43EC80/sub_43EDE0 only start slot 0; slots 1/2 are initialised and reset (sub_43DD40) but never started')
f(4524,12,'void *','tintFxRef[3]','[traced] Obj_InitDefaults stores &tintFx[0..2]; write-only (no [reg+11ACh..11B4h] reader)')
f(4536,4,'Gof2ObjCFrameEventFlags','frameEventFlags','[traced] Obj_SetActionAndRunScript / Obj_ScriptTick set 0x1000000F; sub_49E120/sub_49E0B0 clear bits 4|8 after Obj_ApplyLandingFrameJump; sub_49E830 consumes bit 2; sub_4CDEE0 consumes 0x10000000; sub_48FA20 clears 4')
f(4540,20,'Gof2ObjCPendingTransition','pendingTransition','[traced] queued state-change request, applied by sub_4353C0 (renamed Obj_ApplyPendingTransition)')
f(4560,4,'unsigned int','contactFlagsMisc','[traced reads] sub_43C530 tests bit 4; only ever zeroed (sub_4306D0, sub_430630, sub_430770, CharaSelect_*Pose) -> effectively 0')
f(4564,4,'int','cancelLockFrames','[traced] sub_43C460 sets DefStatus+1144 after a hit; sub_430C80 counts down; Obj_GetNormalCancelState* return "not cancellable" while !=0; sub_4353C0 clears')
f(4568,4,'int','hitDuringAction16','[traced] sub_43C530 sets 1 when the victim is in action 16; sub_433D00 cancel conditions read it; cleared by sub_4306D0')
f(4572,4,'int','hitDuringAction19','[traced] same for action 19')
f(4576,48,'Gof2ObjCPositionBlock','position','[traced] see struct (RBO localX/Y/Z + posX/Y/Z + prevLocalX/Y)')
f(4624,12,'Gof2ObjCEffectList','effects','[traced] ObjEffect list: sub_442180 append, sub_442430 tick, sub_4422E0 clear, ObjEffect_RemoveById, sub_442340 aggregate')
f(4636,4,'Gof2ObjCRuleOverrideFlags','ruleOverrideFlags','[traced reads] sub_42F7A0 (state-rule evaluator) bit 8 forces the rule to fail; only zeroed elsewhere (ctor, Obj_InitDefaults, sub_43DD40)')
f(4640,4,'Gof2ObjCHitEffectStyleFlags','hitEffectStyleFlags','[traced reads] HitJudge_ResolveTick bits 1/2/4 pick hit-effect style 1/2/3 when the attack record has none; bit 8 selects DefStatus+1104/1108/1112 guard bonus (sub_497120, sub_497160, HitJudge_ApplyHit); only zeroed elsewhere')
f(4644,4,'int','fixedHalfRate','[traced] Obj_SetActionAndRunScript sets; sub_49E120: non-zero -> time accumulator drops 128 per tick instead of timeScale')
f(4648,4,'int','timeAccum','[traced] Obj_SetActionAndRunScript = 128; Obj_ScriptTick / sub_4353C0 += 256; sub_49E120 -= rate each tick, <=0 skips the script step (slow-motion / half speed)')
f(4652,4,'int','timeScale','[traced] default 128 (ctor); sub_438660 sets 256/copies parent or child; CComWorkBase/CComWorkNormal compare >= 0.875*timeScaleBase')
f(4656,4,'int','timeScaleBase','[traced] default 128; sub_438660; sub_43DD40 recomputes timeScale from it')
f(4660,32,'Gof2ObjCZoomBlock','zoom','[traced] see struct')
f(4692,16,'Gof2ObjCSharedNodes','sharedNodes','[traced] Fighter_AllocSharedSubNodes 0x42E9C0 fills +0x38..+0x44 of obj+0x121C; Obj_ReleaseSubNodes/Fighter_ReleaseSharedSubNodes free them')

# check contiguity
pos=3136
for off,sz,ct,nm,ev in F:
    assert off==pos,(nm,off,pos); pos+=sz
assert pos==4708,pos

def cdecl(ct,nm):
    # "type name[n]" handling
    return '%s %s;'%(ct,nm)
out=['// GOF2 Obj (pool slot 0x1264) part C: bytes 0xC40..0x1263. New types for IDB (idc.parse_decls). Offsets in struct comments are relative to the struct start.',
     '// Evidence: docs/formats/ida/gof2obj_partC.md. Parse in this order (enums first, flag enums are bitmask).']
out+=ENUMS
for n in STRUCTS: out.append('struct %s;'%n)
for n,(sz,fl) in STRUCTS.items():
    out.append('struct %s {   // %d bytes'%(n,sz))
    for off,s,t,nm,ev in fl:
        out.append(' %s %s; // +0x%03X %s'%(t,nm,off,ev))
    out.append('};')
out.append('struct Gof2ObjTailC {   // bytes 0xC40..0x1263 of Obj (1572 bytes), standalone view for IDA')
for off,sz,ct,nm,ev in F:
    out.append(' %s %s; // +0x%03X %s'%(ct,nm,off-3136,ev))
out.append('};')
open(os.path.join(D,'gof2obj_partC_types.h'),'w').write('\n'.join(out)+'\n')
fl=['# GOF2 Obj bytes 0xC40..0xFFF..0x1263 (decimal 3136..4707): (decimal offset, size, c type, name, comment). Generated by gof2obj_partC_gen.py.',
    '# Nested struct/enum types are defined in gof2obj_partC_types.h (parse BEFORE the Obj struct).','FIELDS = [']
for off,sz,ct,nm,ev in F:
    fl.append(' (%d, %d, %r, %r, %r),'%(off,sz,ct,nm,ev))
fl.append(']')
open(os.path.join(D,'gof2obj_partC_fields.py'),'w').write('\n'.join(fl)+'\n')
print(len(F),'fields', sum(1 for x in F if '[unused]' in x[4]),'unused')
