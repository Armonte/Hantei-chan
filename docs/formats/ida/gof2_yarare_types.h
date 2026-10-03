// GOF2 Yarare hit-memory view: the Obj tail at Obj+0x6F0 (decimal 1776..4707) rebased to class offset 0 (class offset = Obj offset - 1776).
// IDB-only struct Gof2ObjYarareState (0xB74 bytes) = CAppHanteiYarare hit memory used by the Yarare_* functions. Field names/sizes equal the Gof2Obj fields; evidence comments are copied from them.
// Checked by tools/ida/check_yarare_view.py. Parse after the part A/B/C and gof2_obj_types.h headers.
struct Gof2ObjYarareState {
 struct Gof2ObjHurtBoxSet hurtBoxSet; // +0x0 (Obj+0x6F0) [traced] CAppHanteiYarare__vf6 0x4494F0 (build), vf2 0x449460 (rect), vf3 0x43E3C0, vf4 0x4494D0; users HitJudge_CollectPairs, HitJudge_ResolveHitsOnGuardingVictim, sub_49BB70, HitJudge_DefenderGuardsAttack
 Gof2ObjReactionKind reactionKind; // +0x44 (Obj+0x734) [traced] 0 = hit reaction (Victim_StartHitReactionFromScript 0x43D9F0), 1 = guard reaction (Victim_StartGuardReactionFromScript 0x43DBE0); Obj_ScriptTick 0x434AD0 picks the recovery path from it (0: sub_4317B0/sub_434A30, 1: state 55)
 int hitstunTicks; // +0x48 (Obj+0x738) [traced] script-returned stun length +1 (0 = none); counts down to 1 in Obj_TickTimersRecursive 0x49E120; Obj_ScriptTick treats 1 as expired; Victim_ExtendHitstun 0x49A2F0 adds ControllerManage+460
 int hitstunExtendBase; // +0x4C (Obj+0x73C) [traced] 0 at start; Victim_ExtendHitstun stores the pre-extension value; counts down to 1 beside hitstunTicks; Obj_ScriptTick tests ==1 (v14 = 2 path)
 int recoverWhenMoverDone; // +0x50 (Obj+0x740) [traced] script result unk_5E76F4 (Victim_StartHitReactionFromScript); Obj_ScriptTick: when set and the knock mover (obj+712) reports finished -> clear and go to neutral (sub_434A30)
 int recoverOnLanding; // +0x54 (Obj+0x744) [traced] script result dword_5E76F8; Obj_ScriptTick: when set and the stance (obj+444) shows landing for the knock direction -> applies pending recover flags, sub_42F340, hit class 1896 = -1, neutral
 Gof2ObjKnockXMode knockSpeedXMode; // +0x58 (Obj+0x748) [traced reads only] passed as a4 of Victim_StartHitReactionFromScript (1 -> x speed 1200 permil, 2 -> 800); every writer only clears it (HitJudge_SetKnockDirection, HitJudge_ResolveGuardPassAndCrush, HitJudge_CommitPendingReactions), so only a script/zero can set it
 Gof2ObjKnockYMode knockSpeedYMode; // +0x5C (Obj+0x74C) [traced] HitJudge_ApplyKnockDirAndFacing 0x435B10 sets 1/2 from the attacker status flags (+8 & 0xC = 4/8); Victim_StartHitReactionFromScript picks ControllerManage+204/+208 y scale
 Gof2ObjKnockFlags knockFlags; // +0x60 (Obj+0x750) [traced] HitJudge_ApplyKnockDirAndFacing |=1 when the attack record has G2AT_RF_HALVE_KNOCK_SCALE (0x8000000); sub_43D9F0 halves the x speed (a7 & 1)
 unsigned int attackerGuardMask; // +0x64 (Obj+0x754) [traced] copy of the attack record guardMask (+0x7C) written by HitJudge_ApplyKnockDirAndFacing[NoFlipCheck]; mask&3 selects the knock speed factor in sub_43D9F0 (ControllerManage+212/+216/+220); cleared after use
 int totalHitsTaken; // +0x68 (Obj+0x758) [traced] ++ (capped 0xF423F) in HitJudge_ApplyHit; also Obj_ExecCmdListBetween 0x43AAF0; x DefStatus+668 builds the gauge-gain bonus; cleared by Yarare_ResetHitState
 int hitsTakenByAttackKind[2]; // +0x6C (Obj+0x75C) [traced] HitJudge_ApplyHit ++ [attackKind (record+0x80)] (cap 0xF423F); sub_495890 (HitJudge_ScaleDamageByComboHits) indexes the combo scaling table with it; cleared by Yarare_ResetHitState
 Gof2AtHitClass lastHitClass; // +0x74 (Obj+0x764) [traced] -1 none; copy of pendingHitClass in HitJudge_CommitPendingReactions 0x49A090; Obj_ScriptTick (case 2) tests != -1 to start the combat timer; Obj_SpawnFrameEffectRecord/HitJudge_ResolveGuardPassAndCrush write 11 (Gof2AtHitClass)
 Gof2AtHitClass activeHitClass; // +0x78 (Obj+0x768) [traced] -1 none; copy of pendingHitClass (0x49A090); HitJudge_StartVictimReaction tests its HitClass_ReactionPriority >= 8 for the in-progress reaction; reset to -1 on landing recovery (Obj_ScriptTick)
 Gof2AtHitClass pendingHitClass; // +0x7C (Obj+0x76C) [traced] hit class chosen by HitJudge_StartVictimReaction -> Victim_EnterReactionState; -1 none, -2 = guard-crush special (HitJudge_ResolveHitsOnGuardingVictim), -3 temp in Obj_SpawnFrameEffectRecord; consumed (-> -1) by HitJudge_CommitPendingReactions 0x49A090 / HitJudge_ResolveGuardPassAndCrush
 int knockDirSign; // +0x80 (Obj+0x770) [traced] -1/0/+1 horizontal knock direction set by HitJudge_SetKnockDirection 0x435A80 (record knockDirMode 0..4) and sub_4993F0/HitJudge_ApplyHit; scales the knock mover x (sub_43D600/43D6E0/43D790/43D8A0); used by Obj_ApplyLandingFrameJump and Obj_ScriptTick
 int victimFacingSign; // +0x84 (Obj+0x774) [traced] -1/+1 facing the victim takes (HitJudge_SetVictimFacing 0x4359E0, ...NoFlipCheck); sub_43D5B0 turns it into obj+708; cleared by HitJudge_ApplyHit on a first counter hit
 int knockFacingRelation; // +0x88 (Obj+0x778) [traced] +1 if knockDirSign equals the victim facing sign else -1 (Victim_StartHitReactionFromScript/GuardReaction); Obj_ApplyFrameMoveFlags multiplies the frame x velocity by it*facing; Obj_ResetVictimReactionState sets 1
 int guardCrushLatch; // +0x8C (Obj+0x77C) [traced writer only] Victim_MarkGuardCrush 0x496EA0 sets 1 (called when sub_440E60 reports the guard gauge crushed in HitJudge_ResolveHitsOnGuardingVictim); no reader found
 int guardCrushPending; // +0x90 (Obj+0x780) [traced] Victim_MarkGuardCrush sets 1; HitJudge_ResolveGuardPassAndCrush 0x4964D0 turns it into the crush reaction (state 56 on ground, 37 airborne) and clears it; HitJudge_CommitPendingReactions / HitJudge_ResolveHitsOnGuardingVictim test it
 Gof2ObjRecoverFlags recoverFlags; // +0x94 (Obj+0x784) [traced] Victim_SetRecoverBlockFlags 0x43DD00 (3, |=0x10/0x20 when the attack blocks recovery A/B), Victim_EnterReactionState 0x435EA0, sub_499EE0, Obj_SpawnFrameEffectRecord; Obj_CanTechRecover 0x433990 and Yarare_CanTechRecover 0x43E4B0 test (flags&0x22)==2
 Gof2ObjRecoverFlags pendingRecoverFlags; // +0x98 (Obj+0x788) [traced] deferred value of recoverFlags: Obj_ScriptTick copies it to 1924 on landing and clears it; Victim_SetRecoverBlockFlags/Victim_EnterReactionState zero it
 struct Gof2Obj * pushbackAttackerRoot; // +0x9C (Obj+0x78C) [traced] root attacker object of the reaction, set by HitJudge_ResolveTick 0x4996A0 (by hit type) and HitJudge_ResolveHitsOnGuardingVictim; Obj_ApplyLandingFrameJump 0x430A40 pushes it (+4592 x) when this object is stopped by the screen edge; 0 = none
 int launchAttackerSide; // +0xA0 (Obj+0x790) [traced] attacker side index recorded by Victim_EnterReactionState for launch/sweep states 36..39 (-1 = none, sub_499EE0); Obj_ExecCmdListBetween indexes g_PlayerCharaObj[] with it
 int launchAttackerValid; // +0xA4 (Obj+0x794) [traced] 1 when launchAttackerSide is meaningful (states 36..39), 0 otherwise; consumed (-> 0) by sub_495620; tested in Obj_ExecCmdListBetween
 int comboStarted; // +0xA8 (Obj+0x798) [traced] set 1 by HitJudge_ApplyHit on the first received hit (and for attacker states 60/62/../78 of the player); HitJudge_ApplyHit/Obj_ExecCmdListBetween read it (`ctl==1 || comboStarted==0`) as the first-hit damage flag; cleared by Yarare_ResetHitState
 int counterHitCount; // +0xAC (Obj+0x79C) [traced] ++ on the victim root for each hit with a7==1 (counter/back hit) in HitJudge_ApplyHit; ==1 -> first one resets victimFacingSign; cleared by Yarare_ResetHitState
 int launchedByState36; // +0xB0 (Obj+0x7A0) [traced] HitJudge_CommitPendingReactions sets 1 when the reaction pattern is 36 (launch); HitJudge_StartVictimReaction turns a second airborne launch (36) into 37 while it is 1
 int launchDamageScaleApplied; // +0xB4 (Obj+0x7A4) [traced] sub_495890 (HitJudge_ScaleDamageByComboHits): first damage in states 36/37 is scaled by ControllerManage+336/+340 and the flag set to 1; cleared by Obj_ResetVictimReactionState
 int airGuardStunChain; // +0xB8 (Obj+0x7A8) [traced] HitJudge_ResolveGuardPassAndCrush: ++ each time the guard reaction state is 57 (airborne/high classes), reset to 0 for state 54; sub_43DBE0 halves the speed at ==2 (state 57); HitJudge_ResolveHitsOnGuardingVictim tests ==0/<2
 int airHitLatch; // +0xBC (Obj+0x7AC) [traced] sub_4339F0 (Obj_TestAirRecoverableHit) sets 1 when the victim is airborne, recover-capable and not ignored; Obj_ExecCmdListBetween / HitJudge_ApplyHit use ==1 to show the HUD popup
 int gaugeBonusGranted; // +0xC0 (Obj+0x7B0) [inferred] sub_438A70 script command 39: opposing player gets the ControllerManage+1156..1168 bonuses once (guard on ==0, then ++); cleared by Yarare_ResetHitState
 int juggleHitCount; // +0xC4 (Obj+0x7B4) [traced] HitJudge_ApplyHit: ++ while the victim is in launch states 36/38/39 (37 < state <= 39), else 0; sub_495620 subtracts count*ControllerManage+780 (capped by +784) from the launch damage
 int hitHistoryByAttackerPattern[512]; // +0xC8 (Obj+0x7B8) [traced] [2 sides][256 patterns]: Yarare_RegisterHit 0x43E500 / sub_43E590 index 4*(attackerSide*256+attackerPattern)+class200, store 1 and report "already hit" for the repeat-move penalty; memset 0x800 in HitJudge_ResolveHitsOnGuardingVictim and Yarare_ResetHitState 0x43E3D0. Extends to Obj+4023 (beyond this range)
 Gof2ObjCArmorGauge armorGauge; // +0x8C8 (Obj+0xFB8) [traced] see struct; tail of CAppHanteiYarare hit memory (+0x8C8 .. +0x8E3 from Obj+0x6F0, cleared by sub_43E3D0)
 int hudPopupParam; // +0x8E4 (Obj+0xFD4) [traced reads] HudPopupBoard_Tick 0x47AE90 reads it; the only write is the zero in sub_43E3D0, so always 0
 int guardGauge; // +0x8E8 (Obj+0xFD8) [traced] Obj_InitDefaults = DefStatus+16 (max); sub_440E60 adds hit/guard damage and clamps to [0,max]; sub_4CDC70 regenerates per action (g_ControllerManage[67][action]); sub_495B60 passes &guardGauge
 int hitGraceStage; // +0x8EC (Obj+0xFDC) [traced] set 1 by HitJudge_ApplyHit when a player-class object is in action 40; HitJudge_ComputeDamage / sub_495620 / sub_4952F0: while it is 1 or 2 it is incremented and the extra damage argument is forced to 0 [inferred meaning]
 Gof2ObjCEtcBoxSet etcBoxes; // +0x8F0 (Obj+0xFE0) [traced] embedded CAppHanteiEtc (Obj_ConstructPoolSlot 0xFE0)
 Gof2ObjCSousaiBoxSet sousaiBoxes; // +0x928 (Obj+0x1018) [traced] embedded CAppHanteiSousai (+0x1018)
 Gof2ObjCTobiBoxSet tobiBoxes; // +0x958 (Obj+0x1048) [traced] embedded CAppHanteiTobi (+0x1048)
 int eventFreezeFrames; // +0x984 (Obj+0x1074) [traced] sub_49E120: >0 -> decrement and run only the frozen tick (sub_49E0B0); raised to DefStatus+536 for the round-event winner by sub_4A3A80; tested by sub_4353C0, Battle_TickIdleStanceCounters
 int hitStopFrames; // +0x988 (Obj+0x1078) [traced] Obj_RaiseHitStopFrames / HitJudge_ApplyHitStop set it; sub_49E120 counts it down and skips logic; sub_437C70 = 0xFFFF; CComWorkBase/CComWork read >=30 as freeze
 int hitStopLatched; // +0x98C (Obj+0x107C) [traced] Obj_InitDefaults = 1, sub_437C70/Obj_RaiseHitStopFrames set 1, sub_430C80 clears; Obj_RaiseHitStopFrames refuses a lower request while it is 1
 int reactionHoldFrames; // +0x990 (Obj+0x1080) [traced] sub_434F90 sets 8 / script value when a knock-back starts; sub_49E120 counts it down (at 1 and flag 0x10000 sets obj+0x1F8), logic that advances the time accumulator runs only at 0 [inferred meaning]
 int shakeFrames; // +0x994 (Obj+0x1084) [traced] sub_496C60 sets; sub_496CB0 getter; sub_49E120 counts down
 int shakeAmplitude; // +0x998 (Obj+0x1088) [traced] sub_496C60 arg; sub_49E120 shakeOffsetX = dword_5A2388[facing]*amplitude*(phase%2)
 int shakePhase; // +0x99C (Obj+0x108C) [traced] sub_496C60 zeroes; sub_49E120 ++
 int shakeOffsetX; // +0x9A0 (Obj+0x1090) [traced] sub_49E120 output; read through shakeOffsetRef by sub_49ECB0 / DrawHelper_Draw
 int * shakeOffsetRef; // +0x9A4 (Obj+0x1094) [traced] Obj_InitDefaults = &shakeOffsetX; DrawHelper_Draw / sub_49ECB0 dereference it
 int frameTickCounter; // +0x9A8 (Obj+0x1098) [traced] sub_49E120 ++ (saturating -1); sub_430490 zeroes it when a pending pattern-frame change commits
 int actionAgeTicks; // +0x9AC (Obj+0x109C) [traced] sub_49E120 ++ (saturating); sub_430490 zeroes on a pending action change
 int actionScriptAge; // +0x9B0 (Obj+0x10A0) [traced] Obj_ScriptTick ++ (saturating); sub_430490 zeroes on action change; sub_495620 scales damage by it (DefStatus+764/768/772)
 Gof2ObjCArmorState armor; // +0x9B4 (Obj+0x10A4) [traced] RBO-style armor state, see struct
 unsigned char unused_10C8[4]; // +0x9D8 (Obj+0x10C8) [unused] only zero-written by Obj_InitDefaults; no reader (no [reg+10C8h] operand elsewhere)
 int hitBonusCount; // +0x9DC (Obj+0x10CC) [traced] HitJudge_CollectPairs ++ (saturating) when the state script (3,5) answers 2; sub_4944B0 multiplies damage by 1+count*DefStatus(276/280); Obj_GetNormalCancelState family gates on it >0
 void * moveUseRecord; // +0x9E0 (Obj+0x10D0) [traced] Obj_BindCharaRecordAndStartAction arg a9; per-player record: +0 counter, +4..+0x53 20 move counters (sub_42F120 index of action-60), +0x54 flag, +0x58 last action; sub_4306D0/sub_430630 clear it
 void * scriptTables; // +0x9E4 (Obj+0x10D4) [traced] Obj_BindCharaRecordAndStartAction = charaContext+64588; +8 = array of 24-byte transition rules (sub_4365D0 / sub_430910)
 void * keyCmdRef; // +0x9E8 (Obj+0x10D8) [traced] refcounted key-command record (refcount at +72): Obj_ShareKeyCmdRef copies, Obj_ReleaseSubNodes releases, HitJudge_ApplyKnockDirAndFacing reads +8&0xC
 int charaSlotIndex; // +0x9EC (Obj+0x10DC) [traced] Obj_BindCharaRecord arg a1 (record = ctx+32292*idx+4); sub_4B2110/CharaSelect clear; ScriptVm arg
 void * charaContext; // +0x9F0 (Obj+0x10E0) [traced] Obj_BindCharaRecord arg a3; +64600 = script bank used by every ScriptVm_CallSyncScript((obj+0x10E0)+64600,..)
 int incomingAttackFrames; // +0x9F4 (Obj+0x10E4) [traced] ObjScan_StoreAttackGuardInfo: frames until the scanned opponent attack frame; Obj_ScriptTick counts it down; AI hint
 int incomingAttackGuardClass; // +0x9F8 (Obj+0x10E8) [traced] ObjScan_StoreAttackGuardInfo: 1..3 from attack record +0x7C&3 (0 = none)
 Gof2ObjCTintFxSlot tintFx[3]; // +0x9FC (Obj+0x10EC) [traced] 3 slots stride 64: sub_49E120 ticks all three; HitJudge_StartHitEffect/sub_43EC80/sub_43EDE0 only start slot 0; slots 1/2 are initialised and reset (sub_43DD40) but never started
 void * tintFxRef[3]; // +0xABC (Obj+0x11AC) [traced] Obj_InitDefaults stores &tintFx[0..2]; write-only (no [reg+11ACh..11B4h] reader)
 Gof2ObjCFrameEventFlags frameEventFlags; // +0xAC8 (Obj+0x11B8) [traced] Obj_SetActionAndRunScript / Obj_ScriptTick set 0x1000000F; sub_49E120/sub_49E0B0 clear bits 4|8 after Obj_ApplyLandingFrameJump; sub_49E830 consumes bit 2; sub_4CDEE0 consumes 0x10000000; sub_48FA20 clears 4
 Gof2ObjCPendingTransition pendingTransition; // +0xACC (Obj+0x11BC) [traced] queued state-change request, applied by sub_4353C0 (renamed Obj_ApplyPendingTransition)
 unsigned int contactFlagsMisc; // +0xAE0 (Obj+0x11D0) [traced reads] sub_43C530 tests bit 4; only ever zeroed (sub_4306D0, sub_430630, sub_430770, CharaSelect_*Pose) -> effectively 0
 int cancelLockFrames; // +0xAE4 (Obj+0x11D4) [traced] sub_43C460 sets DefStatus+1144 after a hit; sub_430C80 counts down; Obj_GetNormalCancelState* return "not cancellable" while !=0; sub_4353C0 clears
 int hitDuringAction16; // +0xAE8 (Obj+0x11D8) [traced] sub_43C530 sets 1 when the victim is in action 16; sub_433D00 cancel conditions read it; cleared by sub_4306D0
 int hitDuringAction19; // +0xAEC (Obj+0x11DC) [traced] same for action 19
 Gof2ObjCPositionBlock position; // +0xAF0 (Obj+0x11E0) [traced] see struct (RBO localX/Y/Z + posX/Y/Z + prevLocalX/Y)
 Gof2ObjCEffectList effects; // +0xB20 (Obj+0x1210) [traced] ObjEffect list: sub_442180 append, sub_442430 tick, sub_4422E0 clear, ObjEffect_RemoveById, sub_442340 aggregate
 Gof2ObjCRuleOverrideFlags ruleOverrideFlags; // +0xB2C (Obj+0x121C) [traced reads] sub_42F7A0 (state-rule evaluator) bit 8 forces the rule to fail; only zeroed elsewhere (ctor, Obj_InitDefaults, sub_43DD40)
 Gof2ObjCHitEffectStyleFlags hitEffectStyleFlags; // +0xB30 (Obj+0x1220) [traced reads] HitJudge_ResolveTick bits 1/2/4 pick hit-effect style 1/2/3 when the attack record has none; bit 8 selects DefStatus+1104/1108/1112 guard bonus (sub_497120, sub_497160, HitJudge_ApplyHit); only zeroed elsewhere
 int fixedHalfRate; // +0xB34 (Obj+0x1224) [traced] Obj_SetActionAndRunScript sets; sub_49E120: non-zero -> time accumulator drops 128 per tick instead of timeScale
 int timeAccum; // +0xB38 (Obj+0x1228) [traced] Obj_SetActionAndRunScript = 128; Obj_ScriptTick / sub_4353C0 += 256; sub_49E120 -= rate each tick, <=0 skips the script step (slow-motion / half speed)
 int timeScale; // +0xB3C (Obj+0x122C) [traced] default 128 (ctor); sub_438660 sets 256/copies parent or child; CComWorkBase/CComWorkNormal compare >= 0.875*timeScaleBase
 int timeScaleBase; // +0xB40 (Obj+0x1230) [traced] default 128; sub_438660; sub_43DD40 recomputes timeScale from it
 Gof2ObjCZoomBlock zoom; // +0xB44 (Obj+0x1234) [traced] see struct
 Gof2ObjCSharedNodes sharedNodes; // +0xB64 (Obj+0x1254) [traced] Fighter_AllocSharedSubNodes 0x42E9C0 fills +0x38..+0x44 of obj+0x121C; Obj_ReleaseSubNodes/Fighter_ReleaseSharedSubNodes free them
};
