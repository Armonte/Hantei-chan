// GOF2 Obj (pool slot, 0x1264 bytes) merged from parts A/B/C by tools/ida/merge_gof2obj.py. Parse after the part type headers.
struct Gof2Obj {
 struct Gof2ObjATransitionEvents transitionEvents; // +0x0 [traced] Obj_CollectTransitionRuleEvents 0x43E030 0x43E030 receives obj+0 as the event block; TransitionEventList_Link 0x43DF90 0x43DF90; Obj_ResetForCmdBroadcast zeroes the heads
 struct Gof2ObjAFrameEnterEvents frameEnterEvents; // +0x64 [traced] Obj_RunFrameScriptLists 0x4361E0 0x4361E0 passes obj+100 to Obj_CollectFrameEnterEvents 0x43E1B0; FrameEnterEventList_Link 0x43E120 0x43E120 (20 nodes, 4 heads at obj+264..295)
 void * motionHistRing; // +0x128 [traced] ScriptOp_ObjMotionHistCmds 0x4379C0 stores the allocated ObjMotionHist ring in a1[74]; ObjMotionHist_InitFromOwner 0x43E9E0 0x43E9E0; ObjTree_ScriptTickAndReap 0x49E830 advances ring index (+12); Obj_TickTimersRecursive sets ring+8
 int overlayRectBlendIndex; // +0x12c [traced] Obj_ExecOverlayRectOps 0x438D60 case 4 stores it; DrawHelper_Draw 0x49EF60 0x49EF60 `dword_5A7D90[*(a2+300)]` blend/state table index
 struct Gof2ObjAOverlayRectNode * overlayRectList[2]; // +0x130 [traced] Obj_ExecOverlayRectOps 0x438D60 case 3 selects list (obj+304+4*idx), case 1 pushes nodes, case 2 frees; Obj_ReleaseSubNodes 0x4315C0 0x4315C0 frees both; drawn by DrawHelper_Draw 0x49EF60 (RBO overlayQuadList[2] analog)
 int scriptWork[10]; // +0x138 [traced] Obj_CollectFrameEnterEvents 0x43E1B0 sets g_ScriptVm_ArgObj = obj+312; sub_42F3A0 0x42F3A0 indexes `*(base+4*idx)`; Battle_ResetPoolAndSpawnFighters 0x4CC770 copies exactly 10 dwords (a1[78..87]) from the previous round's object
 struct Gof2ObjAParentLink parentLink; // +0x160 [traced] Obj_ExecStateCmdList 0x438660 0x438660 (setup ops 0,2,3,8..17,28), Obj_RunParentLinkRulesOnChildren 0x435130 0x435130, Obj_RunParentLinkAction 0x435070 0x435070
 Gof2ObjASetupFlags setupFlags; // +0x1b0 [traced] Obj_ExecStateCmdList 0x438660 case 21 (set/and/or/andnot); readers Obj_ScriptTick 0x434AD0 (4), Obj_ApplyFrameMoveFlags/Brake (2), Obj_SetActionAndRunScript (0x10..0x200, bit31), Obj_RaiseHitStopFrames (0x40000), sub_433D00 (0x1000000/0x2000000), sub_4CDDB0 (1), sub_4972A0 (0x80000), HitJudge_ResolveSecondaryBoxClashes (0x100000), Obj_SpawnFromSpawnRecord (0x20000), sub_436B60 (0x10000)
 Gof2ObjATurnAroundMode turnAroundMode; // +0x1b4 [traced] sub_433D00 0x433D00: mode 0 asks sub_4970B0 (opponent side) and requests the opposite facing, mode 1 turns by held direction bits; Battle_ResetPoolAndSpawnFighters sets 1 for fighters; Obj_InitDefaults 0
 struct Gof2ObjAScreenBoundState screenBound; // +0x1b8 [traced] Obj_UpdateScreenBoundContacts 0x49D780 0x49D780, Obj_ResolveWallContactLatches 0x49E570 0x49E570, Obj_ExecStateCmdList 0x438660 case 5; reset by Obj_InitDefaults/Obj_ResetOnDetach/sub_437160
 struct Gof2ObjACameraFocus cameraFocus; // +0x1d8 [traced] Camera_ComputeTargetFromFighters 0x4A22D0 0x4A22D0 reads +472/+476/+480/+484; Obj_AttachSlot24CNode 0x431A40 0x431A40 copies them into the link node; Obj_SpawnFrameEffectRecord 0x4327F0 copies back
 struct Gof2ObjABoundRect boundRect; // +0x1e8 [traced] Obj_ConstructPoolSlot 0x435850 `SetRect(this+488,0,0,0,0)`; read as left/top/right/bottom by Obj_UpdateScreenBoundContacts / Obj_ResolveWallContactLatches
 int destroyRequested; // +0x1f8 [traced] Obj_ScriptTick 0x434AD0 `if (*(this+504)) return 0` -> ObjTree_ScriptTickAndReap frees the object; set to 1 by Obj_RunParentLinkAction 0x435070, Obj_UpdateScreenBoundContacts, Obj_TickTimersRecursive (lifetime end), Obj_ResetForCmdBroadcast
 int * perTickScript; // +0x1fc [traced] Obj_SetPerTickScriptFromRecord 0x43D0F0 stores record+4 (or 0); Obj_ScriptTick 0x434AD0 `ScriptVm_LoadArgsForObj(this, *(this+508))` every tick (RBO perTickScript analog)
 int effectCooldownA; // +0x200 [inferred] Obj_TickTimersRecursive 0x49E120 decrements while > 0; set by HitJudge_ApplyHit 0x4983A0 / sub_497230 from DefStatus; gate in sub_49BB70 0x49BB70 (`n[128] == 0`) before spawning a contact effect
 int effectCooldownB; // +0x204 [inferred] same as effectCooldownA (`i[129]--`, sub_49BB70 `n[129] == 0`)
 int hitDepthOverridePattern; // +0x208 [traced] HitJudge_ApplyHit 0x4983A0 stores the requested pattern (a1[130] = a1[175]) with a temporary depth; Obj_RestoreDefaultDepthIfActionChanged 0x432D90 resets depth to 190 when the current pattern differs; Obj_InitDefaults -1
 void * afterimageTrail; // +0x20c [traced] Battle_ResetPoolAndSpawnFighters 0x4CC770 `*(v19+524) = AfterimageTrail_Init()`; AfterimageTrail_RecordTick 0x440DC0, Draw_ObjTreeLayerWithAfterimages, Obj_TickTimersRecursive (i[131])
 int drawDepthOffset; // +0x210 [traced] Obj_ApplyFrameDrawPriority 0x432E30 0x432E30 writes it; Obj_ComputeTotalDepth 0x430570 sums `*(obj+528) + **(obj+536)` over the depthParent chain (RBO drawDepthOffset analog)
 int drawLayer; // +0x214 [traced] Obj_ApplyFrameDrawPriority 0x432E30 cases 35..39 store it; Draw_ObjTreeLayerWithAfterimages 0x49F850 draws objects whose layer equals the pass (RBO drawLayer analog)
 int * drawDepthBasePtr; // +0x218 [traced] Obj_ApplyFrameDrawPriority 0x432E30 / sub_432D90 point it at g_MiscCtx_72EC34[12] or the per-side word 0x65B6C8+4*side; dereferenced in Obj_ComputeTotalDepth
 struct Gof2Obj * depthParent; // +0x21c [traced] Obj_SumAncestorDepthOffsets 0x430540 walks it; Obj_SpawnFromSpawnRecord 0x437270 sets it to the parent for relative-depth spawns (0x40000); cleared by Obj_RunParentLinkAction mask 0x40 (RBO depthParent analog)
 struct Gof2Obj * ownerListNext; // +0x220 [traced] ObjPool_LinkIntoOwnerList 0x49FCF0 0x49FCF0 `*(a2+544) = head`; free list also chains through +544 (ObjPool_AllocIntoOwnerList); owner-list walks `i = *(i+544)`
 struct Gof2Obj * ownerListPrev; // +0x224 [traced] ObjPool_LinkIntoOwnerList 0x49FCF0 `*(a2+548) = prev`; ObjPool_FreeObjAndUnlink 0x49FEA0 0x49FEA0 unlinks via it
 struct Gof2Obj * prevSibling; // +0x228 [traced] ObjPool_AllocChild 0x49FDD0 0x49FDD0 `*(new+552) = 0; *(old_first+552) = new`
 struct Gof2Obj * nextSibling; // +0x22c [traced] ObjPool_AllocChild 0x49FDD0 `new+556 = old first child`; child walks `i = i[139]`
 struct Gof2Obj * parent; // +0x230 [traced] ObjPool_AllocChild 0x49FDD0 `v2[140] = a1`; parent climbs `a1 = a1[140]` throughout (Obj_RaiseHitStopFrames, sub_43C1A0 ...)
 struct Gof2Obj * firstChild; // +0x234 [traced] ObjPool_AllocChild 0x49FDD0 `*(a1+564) = new`; ObjPool_FreeSiblingChainRecursive 0x49FE60 frees via it
 struct Gof2Obj * spawner; // +0x238 [traced] Obj_SpawnFromSpawnRecord 0x437270 `*(v4+568) = a1` (creator); ScriptVm_LoadArgsForObj 0x43D110 case 2 reads spawner pos/facing/pattern; sub_42F3A0 script-var base `a1[142]+312` (RBO spawner analog)
 struct Gof2Obj * firstLinkedTarget; // +0x23c [traced] Obj_ResetOnDetach 0x4390D0 `v2 = a1[143]` walks held targets; Obj_SpawnFrameEffectRecord 0x4327F0 `for (i = obj[143]; i; i = i[145])`; Obj_IsSpecialCancelAllowed tests it
 struct Gof2Obj * linkHolder; // +0x240 [traced] Obj_ResetOnDetach 0x4390D0 zeroes `*(v2+576)`; ScriptVm_LoadArgsForObj 0x43D110 case 2 reads `*(spawner+576)`; Obj_TickTimersRecursive `i[144]` splits held/non-held passes
 struct Gof2Obj * nextLinkedTarget; // +0x244 [traced] Obj_SpawnFrameEffectRecord 0x4327F0 `i = i[145]`; Obj_ResetOnDetach `v2 = *(v2+580)`
 void * linkContext; // +0x248 [traced] Obj_ScriptTick 0x434AD0 lerps targets with ctx[2]/ctx[3]; Obj_ResetOnDetach 0x4390D0 writes the 248-byte context node (a1[146]); Obj_ReleaseSubNodes 0x4315C0 frees it to g_ObjSlot248Pool
 void * linkNode; // +0x24c [traced] Obj_AttachSlot24CNode 0x431A40 0x431A40; Obj_ResetOnDetach 0x4390D0 `*(*(v2+588)+12..24)`; Obj_ReleaseSubNodes 0x4315C0 frees it to g_ObjSlot24CPool
 struct Gof2ObjABufferedJump bufferedJump; // +0x250 [traced] ObjEvent_InputRuleBufferJump 0x4365D0 0x4365D0, Obj_TestRelativePosition 0x436470 0x436470, ObjEvent_ConditionalRuleSelectJump 0x436280 0x436280; only the kind dword has a reader
 int superFreezeSerial; // +0x260 [traced] SuperFreeze_Begin 0x4A2F20 0x4A2F20 / BattleTint_ClearForObj 0x42F0B0 0x42F0B0 store the freeze serial; Obj_IsSuperFrozen 0x4A2FA0 0x4A2FA0 compares it with the per-side serial (1 = always exempt)
 int prevPatternNo; // +0x264 [traced] Obj_SetActionAndRunScript 0x431B90 `*(v4+153) = *(v4+155)` (previous pattern); read by the same function (`(prev - 28) <= 0xB` hit-stun return); Obj_InitDefaults -1
 struct Gof2HanteiAnime anime; // +0x268 [traced] Obj_ConstructPoolSlot 0x435850 0x435850 stores the CHanteiAnime vtable at +616; Obj_BindCharaRecord 0x4316A0 0x4316A0 fills +632..+656; existing type (patternNo +620, frameNo +624, frameTicks +628, frame +644)
 int landedLatch; // +0x2ac [traced] Obj_ApplyLandingFrameJump 0x430A40 sets 1 on landing; sub_430C80 clears per tick; Obj_RunParentLinkRulesOnChildren 0x435130 reads `*(parent+684) == 1` (RBO landedLatch analog)
 int facing; // +0x2b0 [traced] sub_430390 returns `*(this+688)` (0 right, 1 left); sub_430490 applies pending facing; read as `dword_5A2388[facing]` sign tables everywhere
 int fixedFacingValue; // +0x2b4 [traced] Obj_SpawnFromSpawnRecord 0x437270 stores 0/1 (flags 0x100/0x200) and redirects facingPtr here (RBO fixedFacingValue analog)
 int * facingPtr; // +0x2b8 [traced] Obj_InitDefaults 0x431000 `*(a1+696) = a1+688`; DrawHelper_Draw 0x49EF60 `**(a2+696)`; redirected by Obj_SpawnFromSpawnRecord
 int pendingPattern; // +0x2bc [traced] sub_430490 / Obj_RequestPatternFrame: requested next pattern (-1 none); applied by sub_4353C0
 int pendingFrame; // +0x2c0 [traced] sub_430490: requested frame in the new pattern
 int pendingFacing; // +0x2c4 [traced] sub_430490 moves it to facing (-1 none); sub_433D00 0x433D00 requests turn-arounds through it
 struct Gof2ObjAMover mainMover; // +0x2c8 [traced] Obj_ResetMoveTracks 0x430E90 0x430E90 / Mover_StartVelocity; stepped in Obj_ApplyLandingFrameJump 0x430A40 `(a1+724)(&x,&y,w,h,a1+712)`; frame move flags in Obj_ApplyFrameMoveFlags
 int pendingFlipNegatesVelocity; // +0x330 [traced] sub_436970 0x436970 sets 1 when the jump facing nibble is non-zero; sub_4353C0 calls the mover flip callback and clears it (RBO pendingFlipNegatesVelocity analog)
 int knockbackTicksLeft; // +0x334 [traced] Obj_ApplyLandingFrameJump 0x430A40 `if (*(a1+820)) { step track B; --*(a1+820) }`
 struct Gof2ObjAMover knockbackMover; // +0x338 [traced] Obj_ApplyLandingFrameJump 0x430A40 steps it every tick while knockbackTicksLeft != 0 (call at +836 with this = obj+824)
 int brakeMoverActive; // +0x3a0 [traced] Obj_ApplyFrameBrake 0x435280 sets 1; Obj_ApplyLandingFrameJump 0x430A40 `if (*(a1+928) && step(...) == 0) *(a1+928) = 0`
 struct Gof2ObjAMover brakeMover; // +0x3a4 [traced] Obj_ApplyFrameBrake 0x435280 0x435280 builds a kind-2 limited mover here (step sub_4412A0, brake percent / -1000)
 struct Gof2ObjAColorFlash colorFlash; // +0x40c [traced] Obj_ExecColorFlashOps 0x43A2C0 0x43A2C0, Obj_SetActionAndRunScript 0x431B90, Obj_TickTimersRecursive 0x49E120 (elapsed/remaining), Obj_ExecCmdListBetween 0x43AAF0, HitJudge_ApplyHit
 struct Gof2ObjAAlphaFx alphaFx; // +0x42c [traced] Obj_ApplyFrameBlendAlphaZoom 0x435620 0x435620 stores frame alpha in +1072/+1076; Obj_StartDetachFade 0x434F90 builds a ramp; ticked by Obj_TickEffectBlocks 0x430BE0 0x430BE0
 struct Gof2ObjAZoomFx zoomFx; // +0x448 [traced] Obj_ApplyFrameBlendAlphaZoom 0x435620 stores frame.zoom at +1112; DrawHelper_Draw 0x49EF60 `(*(a2+1100))(a2+1096,&x,&y)`
 struct Gof2ObjARgbFx modulateColorFx; // +0x480 [traced] Obj_InitDefaults 0x431000 (update sub_443020, rgb 255,255,255); copied to spawned children by Obj_SpawnFromSpawnRecord `qmemcpy(v4+1152, obj+1152, 0x50)` and by Draw_ObjTreeLayerWithAfterimages
 struct Gof2ObjARgbFx additiveColorFx; // +0x4d0 [traced] Obj_InitDefaults 0x431000 (rgb 0,0,0); `qmemcpy(v4+1232, ..., 0x50)` in Obj_SpawnFromSpawnRecord
 struct Gof2ObjAFlickerGate flickerGate; // +0x520 [unused] Obj_InitDefaults 0x431000 zeroes it; no reader (RBO flickerGate analog)
 struct Gof2ObjARotFx rotXFx; // +0x530 [traced] Obj_TickEffectBlocks 0x430BE0 calls `(*(a1+1332))(a1+1328)`; DrawHelper_Draw 0x49EF60 reads `*(a2+1328)/256`
 struct Gof2ObjARotFx rotYFx; // +0x550 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1364))(a1+1360)`; DrawHelper_Draw `*(a2+1360)/256`; sub_49C7D0 writes 70*256
 struct Gof2ObjARotFx rotZFx; // +0x570 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1396))(a1+1392)`; Effect_SpawnHitSpark / sub_49C910 write angle and base
 struct Gof2ObjAZoomFx zoomAddFx; // +0x590 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1424))(a1+1424)`; DrawHelper_Draw 0x49EF60 `(*(a2+1428))(a2+1424,&x,&y)`; added to zoomFx (RBO zoomAddFx analog)
 Gof2ObjADrawHelperFollowMode drawHelperFollowMode; // +0x5c8 [traced] Obj_ExecStateCmdList 0x438660 case 7; DrawHelper_CopyPosFromOwner 0x49EB40 0x? switch 1/2/3 copies position into g_DrawHelperObjA/B; Battle_ResetPoolAndSpawnFighters sets 1
 int sideIndex; // +0x5cc [traced] Obj_ResetOnDetach 0x4390D0 `g_BattlePlayerRec + 140*obj[371]`; Obj_SetOwnerIdentity 0x431670 a3; -1 for neutral effects; indexes g_SideCombatTimers etc.
 int ownerListIndex; // +0x5d0 [traced] Obj_SetOwnerIdentity 0x431670 a2; passed to ObjPool_AllocIntoOwnerList in Obj_SpawnFromSpawnRecord; with ownerSubSlot indexes the per-slot hit memory at +1736/+4160 (HitJudge_*)
 int ownerSubSlot; // +0x5d4 [traced] Obj_SetOwnerIdentity 0x431670 a4; `1 << obj[373]` in sub_43C530; hit-memory index in HitJudge_ApplyHit / sub_49B360 / sub_49B520
 int scriptTag; // +0x5d8 [traced] Obj_ExecFrameEnterMiscOps case 16 stores it; CmdBroadcast_ApplyIfMatches 0x437DF0 compares it; ScriptVm_LoadArgsForObj 0x43D110 passes it; copied to children by Obj_SpawnFromSpawnRecord (RBO scriptTag analog)
 Gof2ObjAClassFlags classFlags; // +0x5dc [traced] Obj_SetOwnerIdentity 0x431670 a5 (`& 0xF | 0x20` for children); (flags & 0xF0) == 0x10 = fighter entity throughout; Obj_ExecFrameEnterMiscOps case 12 edits; sub_437160 stores 35
 int blendMode; // +0x5e0 [traced] Obj_ApplyFrameBlendAlphaZoom 0x435620 copies frame.blendMode (+9) here; Obj_InitDefaults 1; DrawHelper_Draw 0x49EF60 reads it
 int drawStyle; // +0x5e4 [traced] Obj_ApplyFrameBlendAlphaZoom 0x435620 copies frame.drawStyle (+0x1A); Obj_InitDefaults 2; DrawHelper_Draw
 struct Gof2ObjAInvulnWindows invulnWindows; // +0x5e8 [traced] sub_433A60 0x433A60, ObjTree_RaiseStrikeInvulnWindow 0x43EB30 0x43EB30 / ClashInvulnWindow 0x43EB90, Obj_ResetHanteiBoxObjects 0x4356A0 0x4356A0, Obj_TickTimersRecursive 0x49E120, sub_43BA00
 int hookFlags[7]; // +0x5f8 [traced] Obj_ExecStateCmdList 0x438660 case 29 (`a2[idx+382] = v`); hook k enables ScriptVm kind 3 id 7+k via Obj_CallHookScript 0x43D980 0x43D980: [0] sub_49A090, [1] HitJudge_ResolveSecondaryBoxClashes, [2] Obj_SetActionAndRunScript, [3] sub_4957C0, [4] ObjTree_ScriptTickAndReap, [5] sub_4972A0, [6] HitJudge_ApplyKnockDirAndFacingNoFlipCheck
 void * kougekiVtable; // +0x614 [traced] Obj_ConstructPoolSlot 0x435850 stores the CAppHanteiKougeki vtable here (Gof2AttackBoxSetPrefix.vtable); object continues past 0x61F (owned by part B)
 unsigned int kougekiPrimaryCount; // +0x618 [traced] Gof2AttackBoxSetPrefix.primaryCount (+0x04 of the embedded CAppHanteiKougeki)
 unsigned int kougekiSecondaryCount; // +0x61c [traced] Gof2AttackBoxSetPrefix.secondaryCount (+0x08)
 struct Gof2AtRecord * attackRecord; // +0x620 [traced] CAppHanteiKougeki_BuildFromFrame 0x449250 (+0x0C = view->attackRecords + 236*frame.attackRecordIdx); read as *(obj+1568) by every HitJudge_* routine, sub_43BA00, sub_43C530
 struct Gof2BoxRect * attackBoxPtr[8]; // +0x624 [traced] BuildFromFrame writes view->boxes[idx] for each non -1 frame.attackBoxIdx[0..7]; read through the vtable box getters (vf2 0x43E370, sub_43BB30, sub_49ABE0)
 int attackBoxSlot[8]; // +0x644 [traced] BuildFromFrame writes the frame slot number (0..7) of box i next to attackBoxPtr[i]; read by the vtable getters
 unsigned int hitLimitFlags; // +0x664 [traced] BuildFromFrame on ATKEV_BEGIN copies frame.hitLimitFlags (+0x48); cleared by CAppHanteiKougeki__vf4 0x43E2B0 (never read directly; zero in all frames)
 unsigned int hitLimitCount; // +0x668 [traced] BuildFromFrame copies frame.hitLimitCount; sub_436280 (transition-rule type 1) compares it with +1644 ([410] vs [411])
 unsigned int hitsLandedForLimit; // +0x66c [inferred] only reader is sub_436280 case 1 (`obj[411] < obj[410]` -> rule fails); no writer exists in the exe (all [reg+0x58] stores in the Kougeki vtable methods and no Obj-base store at 1644), so it stays at its zero default
 int attackEndLatch; // +0x670 [traced writer only] CAppHanteiKougeki__vf5 0x449230 and vf6 0x43E2A0 clear it (class +0x5C); no reader found
 unsigned char unused_674[8]; // +0x674 [unused] class +0x60/+0x64: no Kougeki method touches them and no Obj-base [reg+1652/1656] operand exists outside CharaSelect scene structs (scene hits are different objects)
 int attackActive; // +0x67c [traced writer only] BuildFromFrame sets 1 when frame.attackEventFlags has ATKEV_BEGIN; CAppHanteiKougeki__vf3 0x449220 clears it; no reader found
 int boxJitterX; // +0x680 [traced] CAppHanteiKougeki__vf7 0x43E320 = Rng_Range(g_RngStream_Game, ControllerManage+1324) after a successful build; CAppHanteiKougeki__vf2 0x43E370 widens the returned attack rect horizontally by it
 int boxJitterY; // +0x684 [traced] vf7 (Rng_Range with ControllerManage+1328); vf2 widens the rect vertically by it
 int moveHitCount; // +0x688 [traced] -1 = disabled; ++ on every connected hit (HitJudge_ApplyHit 0x4983A0, HitJudge_ResolveHitsOnGuardingVictim) and Obj_ExecCmdListBetween 0x43AAF0; cleared by Obj_ResetHanteiBoxObjects 0x4356A0; >1 scales gauge gain by ControllerManage+1232/1000 (sub_494B60, sub_494C70, sub_494E20)
 int counterHitSeen; // +0x68c [traced] ObjHitFlags_RecordHit 0x43C210 sets 1 when variant==1; Obj_GetNormalCancelState 0x4302E0, Obj_IsSpecialCancelAllowed 0x430120, Obj_GetNormalCancelStateScriptGated 0x4301C0 test ==1 (cancel allowed after a counter hit)
 Gof2ObjHitFlagBits hitFlags; // +0x690 [traced] ObjHitFlags_RecordHit 0x43C210 (variant 0): |=1 on self, |=2 on every ancestor (obj+560 chain); rule matching in Obj_HitFlagRuleMatches 0x42F5B0 (rule bit 1); Obj_AnyHitFlagSet 0x42F570
 Gof2ObjHitFlagBits counterHitFlags; // +0x694 [traced] ObjHitFlags_RecordHit (variant != 0); rule bit 0x10000 in 0x42F5B0
 Gof2ObjHitFlagBits guardStunHitFlags; // +0x698 [traced] ObjHitFlags_RecordGuardStunHit 0x43C2A0 (kind 0, caller HitJudge_ResolveHitsOnGuardingVictim: hit on a victim already in a guard state 21/22/23/54/55/57 that did not crush the guard); rule bit 2
 Gof2ObjHitFlagBits guardCrushHitFlags; // +0x69c [traced] same function with variant != 0 (HitJudge_ResolveHitsOnGuardingVictim passes the sub_440E60 guard-gauge result); rule bit 0x20000
 Gof2ObjHitFlagBits clashFlags; // +0x6a0 [traced] ObjHitFlags_RecordClash 0x43C320 from Obj_ApplyContactEventList 0x43C530 (event kind 2) and sub_49B2E0 (box clash); rule bit 4
 Gof2ObjHitFlagBits guardedFlags; // +0x6a4 [traced] ObjHitFlags_RecordGuarded 0x43C360 from HitJudge_OnAttackGuarded 0x4967C0, sub_49BB70 and event kind 3; rule bit 8
 unsigned int counterHitStanceMask; // +0x6a8 [traced] ObjHitFlags_RecordHit ORs the victim stanceClass (Gof2StanceClass, obj+660) here for variant != 0, ancestors get it <<8; read by 0x42F5B0 as the stance filter of counterHitFlags
 unsigned int hitStanceMask; // +0x6ac [traced] same for variant 0 (partner of hitFlags)
 unsigned int guardStunHitStanceMask; // +0x6b0 [traced] ObjHitFlags_RecordGuardStunHit, variant 0 (partner of guardStunHitFlags)
 unsigned int guardCrushHitStanceMask; // +0x6b4 [traced] ObjHitFlags_RecordGuardStunHit, variant != 0 (partner of guardCrushHitFlags)
 unsigned int clashStanceMask; // +0x6b8 [traced] ObjHitFlags_RecordClash (partner of clashFlags)
 unsigned int guardedStanceMask; // +0x6bc [traced] ObjHitFlags_RecordGuarded (partner of guardedFlags)
 int clashState; // +0x6c0 [traced] ObjHitFlags_MarkAttackClash 0x43C420: 1 on self, 2 on every ancestor (event kind 8 in Obj_ApplyContactEventList); read by sub_433100 and sub_433D00
 int framesSinceHit; // +0x6c4 [traced] -1 at reset (CAppHanteiKougeki__vf4); 0 on every connected hit/guard/clash (HitJudge_ApplyHit, HitJudge_OnAttackGuarded, sub_49B2E0, sub_49BB70); ++ per tick, saturating at -1/0x7FFFFFFF (Obj_TickTimersRecursive 0x49E120); sub_430280 compares it with ControllerManage+1152
 int alreadyHitVictim[2]; // +0x6c8 [traced] one-hit-per-victim latch, index victim.side(+1488)+victim.team(+1492): set 1 by HitJudge_ApplyHit, HitJudge_OnAttackGuarded, sub_49B360, sub_43C530; tested by HitJudge_CollectPairs, ObjScan_IsBlockableAttackFrame 0x432420, sub_49B520; cleared by vf4 (index range assumes team index 0; with team 1 the read would hit +1744)
 struct Gof2ObjPushBoxSet pushBoxSet; // +0x6d0 [traced] CAppHanteiKasanari__vf5 0x449620 (build from frame.kasanariBoxIdx), vf6 0x449680 / vf2 0x4496D0 (rect getters); used by Obj_ResolveBodyPushOverlap 0x49DB20 (a1+1744)
 int pushIgnore; // +0x6e0 [traced] set by script command 27 sub 0 in Obj_ExecStateCmdList; Obj_ResolveBodyPushOverlap skips other objects whose value is non-zero; zeroed by Obj_InitDefaults
 Gof2ObjPushMode pushMode; // +0x6e4 [traced] script command 27 sub 1; switch in Obj_ResolveBodyPushOverlap 0x49DB20
 int pushOpposingSideOnly; // +0x6e8 [traced] script command 27 sub 2; value 1 = only push objects of another side (Obj_ResolveBodyPushOverlap checks +1488 differs)
 int pushImmovable; // +0x6ec [traced] script command 30 (Obj_ExecStateCmdList), forced to 1 temporarily by sub_43C530 event option; when 1 the object is never displaced by Obj_ResolveBodyPushOverlap (the other one absorbs the whole overlap)
 struct Gof2ObjHurtBoxSet hurtBoxSet; // +0x6f0 [traced] CAppHanteiYarare__vf6 0x4494F0 (build), vf2 0x449460 (rect), vf3 0x43E3C0, vf4 0x4494D0; users HitJudge_CollectPairs, HitJudge_ResolveHitsOnGuardingVictim, sub_49BB70, HitJudge_DefenderGuardsAttack
 Gof2ObjReactionKind reactionKind; // +0x734 [traced] 0 = hit reaction (Victim_StartHitReactionFromScript 0x43D9F0), 1 = guard reaction (Victim_StartGuardReactionFromScript 0x43DBE0); Obj_ScriptTick 0x434AD0 picks the recovery path from it (0: sub_4317B0/sub_434A30, 1: state 55)
 int hitstunTicks; // +0x738 [traced] script-returned stun length +1 (0 = none); counts down to 1 in Obj_TickTimersRecursive 0x49E120; Obj_ScriptTick treats 1 as expired; Victim_ExtendHitstun 0x49A2F0 adds ControllerManage+460
 int hitstunExtendBase; // +0x73c [traced] 0 at start; Victim_ExtendHitstun stores the pre-extension value; counts down to 1 beside hitstunTicks; Obj_ScriptTick tests ==1 (v14 = 2 path)
 int recoverWhenMoverDone; // +0x740 [traced] script result unk_5E76F4 (Victim_StartHitReactionFromScript); Obj_ScriptTick: when set and the knock mover (obj+712) reports finished -> clear and go to neutral (sub_434A30)
 int recoverOnLanding; // +0x744 [traced] script result dword_5E76F8; Obj_ScriptTick: when set and the stance (obj+444) shows landing for the knock direction -> applies pending recover flags, sub_42F340, hit class 1896 = -1, neutral
 Gof2ObjKnockXMode knockSpeedXMode; // +0x748 [traced reads only] passed as a4 of Victim_StartHitReactionFromScript (1 -> x speed 1200 permil, 2 -> 800); every writer only clears it (HitJudge_SetKnockDirection, HitJudge_ResolveGuardPassAndCrush, HitJudge_CommitPendingReactions), so only a script/zero can set it
 Gof2ObjKnockYMode knockSpeedYMode; // +0x74c [traced] HitJudge_ApplyKnockDirAndFacing 0x435B10 sets 1/2 from the attacker status flags (+8 & 0xC = 4/8); Victim_StartHitReactionFromScript picks ControllerManage+204/+208 y scale
 Gof2ObjKnockFlags knockFlags; // +0x750 [traced] HitJudge_ApplyKnockDirAndFacing |=1 when the attack record has G2AT_RF_HALVE_KNOCK_SCALE (0x8000000); sub_43D9F0 halves the x speed (a7 & 1)
 unsigned int attackerGuardMask; // +0x754 [traced] copy of the attack record guardMask (+0x7C) written by HitJudge_ApplyKnockDirAndFacing[NoFlipCheck]; mask&3 selects the knock speed factor in sub_43D9F0 (ControllerManage+212/+216/+220); cleared after use
 int totalHitsTaken; // +0x758 [traced] ++ (capped 0xF423F) in HitJudge_ApplyHit; also Obj_ExecCmdListBetween 0x43AAF0; x DefStatus+668 builds the gauge-gain bonus; cleared by Yarare_ResetHitState
 int hitsTakenByAttackKind[2]; // +0x75c [traced] HitJudge_ApplyHit ++ [attackKind (record+0x80)] (cap 0xF423F); sub_495890 (HitJudge_ScaleDamageByComboHits) indexes the combo scaling table with it; cleared by Yarare_ResetHitState
 Gof2AtHitClass lastHitClass; // +0x764 [traced] -1 none; copy of pendingHitClass in HitJudge_CommitPendingReactions 0x49A090; Obj_ScriptTick (case 2) tests != -1 to start the combat timer; Obj_SpawnFrameEffectRecord/HitJudge_ResolveGuardPassAndCrush write 11 (Gof2AtHitClass)
 Gof2AtHitClass activeHitClass; // +0x768 [traced] -1 none; copy of pendingHitClass (0x49A090); HitJudge_StartVictimReaction tests its HitClass_ReactionPriority >= 8 for the in-progress reaction; reset to -1 on landing recovery (Obj_ScriptTick)
 Gof2AtHitClass pendingHitClass; // +0x76c [traced] hit class chosen by HitJudge_StartVictimReaction -> Victim_EnterReactionState; -1 none, -2 = guard-crush special (HitJudge_ResolveHitsOnGuardingVictim), -3 temp in Obj_SpawnFrameEffectRecord; consumed (-> -1) by HitJudge_CommitPendingReactions 0x49A090 / HitJudge_ResolveGuardPassAndCrush
 int knockDirSign; // +0x770 [traced] -1/0/+1 horizontal knock direction set by HitJudge_SetKnockDirection 0x435A80 (record knockDirMode 0..4) and sub_4993F0/HitJudge_ApplyHit; scales the knock mover x (sub_43D600/43D6E0/43D790/43D8A0); used by Obj_ApplyLandingFrameJump and Obj_ScriptTick
 int victimFacingSign; // +0x774 [traced] -1/+1 facing the victim takes (HitJudge_SetVictimFacing 0x4359E0, ...NoFlipCheck); sub_43D5B0 turns it into obj+708; cleared by HitJudge_ApplyHit on a first counter hit
 int knockFacingRelation; // +0x778 [traced] +1 if knockDirSign equals the victim facing sign else -1 (Victim_StartHitReactionFromScript/GuardReaction); Obj_ApplyFrameMoveFlags multiplies the frame x velocity by it*facing; Obj_ResetVictimReactionState sets 1
 int guardCrushLatch; // +0x77c [traced writer only] Victim_MarkGuardCrush 0x496EA0 sets 1 (called when sub_440E60 reports the guard gauge crushed in HitJudge_ResolveHitsOnGuardingVictim); no reader found
 int guardCrushPending; // +0x780 [traced] Victim_MarkGuardCrush sets 1; HitJudge_ResolveGuardPassAndCrush 0x4964D0 turns it into the crush reaction (state 56 on ground, 37 airborne) and clears it; HitJudge_CommitPendingReactions / HitJudge_ResolveHitsOnGuardingVictim test it
 Gof2ObjRecoverFlags recoverFlags; // +0x784 [traced] Victim_SetRecoverBlockFlags 0x43DD00 (3, |=0x10/0x20 when the attack blocks recovery A/B), Victim_EnterReactionState 0x435EA0, sub_499EE0, Obj_SpawnFrameEffectRecord; Obj_CanTechRecover 0x433990 and Yarare_CanTechRecover 0x43E4B0 test (flags&0x22)==2
 Gof2ObjRecoverFlags pendingRecoverFlags; // +0x788 [traced] deferred value of recoverFlags: Obj_ScriptTick copies it to 1924 on landing and clears it; Victim_SetRecoverBlockFlags/Victim_EnterReactionState zero it
 struct Gof2Obj * pushbackAttackerRoot; // +0x78c [traced] root attacker object of the reaction, set by HitJudge_ResolveTick 0x4996A0 (by hit type) and HitJudge_ResolveHitsOnGuardingVictim; Obj_ApplyLandingFrameJump 0x430A40 pushes it (+4592 x) when this object is stopped by the screen edge; 0 = none
 int launchAttackerSide; // +0x790 [traced] attacker side index recorded by Victim_EnterReactionState for launch/sweep states 36..39 (-1 = none, sub_499EE0); Obj_ExecCmdListBetween indexes g_PlayerCharaObj[] with it
 int launchAttackerValid; // +0x794 [traced] 1 when launchAttackerSide is meaningful (states 36..39), 0 otherwise; consumed (-> 0) by sub_495620; tested in Obj_ExecCmdListBetween
 int comboStarted; // +0x798 [traced] set 1 by HitJudge_ApplyHit on the first received hit (and for attacker states 60/62/../78 of the player); HitJudge_ApplyHit/Obj_ExecCmdListBetween read it (`ctl==1 || comboStarted==0`) as the first-hit damage flag; cleared by Yarare_ResetHitState
 int counterHitCount; // +0x79c [traced] ++ on the victim root for each hit with a7==1 (counter/back hit) in HitJudge_ApplyHit; ==1 -> first one resets victimFacingSign; cleared by Yarare_ResetHitState
 int launchedByState36; // +0x7a0 [traced] HitJudge_CommitPendingReactions sets 1 when the reaction pattern is 36 (launch); HitJudge_StartVictimReaction turns a second airborne launch (36) into 37 while it is 1
 int launchDamageScaleApplied; // +0x7a4 [traced] sub_495890 (HitJudge_ScaleDamageByComboHits): first damage in states 36/37 is scaled by ControllerManage+336/+340 and the flag set to 1; cleared by Obj_ResetVictimReactionState
 int airGuardStunChain; // +0x7a8 [traced] HitJudge_ResolveGuardPassAndCrush: ++ each time the guard reaction state is 57 (airborne/high classes), reset to 0 for state 54; sub_43DBE0 halves the speed at ==2 (state 57); HitJudge_ResolveHitsOnGuardingVictim tests ==0/<2
 int airHitLatch; // +0x7ac [traced] sub_4339F0 (Obj_TestAirRecoverableHit) sets 1 when the victim is airborne, recover-capable and not ignored; Obj_ExecCmdListBetween / HitJudge_ApplyHit use ==1 to show the HUD popup
 int gaugeBonusGranted; // +0x7b0 [inferred] sub_438A70 script command 39: opposing player gets the ControllerManage+1156..1168 bonuses once (guard on ==0, then ++); cleared by Yarare_ResetHitState
 int juggleHitCount; // +0x7b4 [traced] HitJudge_ApplyHit: ++ while the victim is in launch states 36/38/39 (37 < state <= 39), else 0; sub_495620 subtracts count*ControllerManage+780 (capped by +784) from the launch damage
 int hitHistoryByAttackerPattern[512]; // +0x7b8 [traced] [2 sides][256 patterns]: Yarare_RegisterHit 0x43E500 / sub_43E590 index 4*(attackerSide*256+attackerPattern)+class200, store 1 and report "already hit" for the repeat-move penalty; memset 0x800 in HitJudge_ResolveHitsOnGuardingVictim and Yarare_ResetHitState 0x43E3D0. Extends to Obj+4023 (beyond this range)
 Gof2ObjCArmorGauge armorGauge; // +0xfb8 [traced] see struct; tail of CAppHanteiYarare hit memory (+0x8C8 .. +0x8E3 from Obj+0x6F0, cleared by sub_43E3D0)
 int hudPopupParam; // +0xfd4 [traced reads] HudPopupBoard_Tick 0x47AE90 reads it; the only write is the zero in sub_43E3D0, so always 0
 int guardGauge; // +0xfd8 [traced] Obj_InitDefaults = DefStatus+16 (max); sub_440E60 adds hit/guard damage and clamps to [0,max]; sub_4CDC70 regenerates per action (g_ControllerManage[67][action]); sub_495B60 passes &guardGauge
 int hitGraceStage; // +0xfdc [traced] set 1 by HitJudge_ApplyHit when a player-class object is in action 40; HitJudge_ComputeDamage / sub_495620 / sub_4952F0: while it is 1 or 2 it is incremented and the extra damage argument is forced to 0 [inferred meaning]
 Gof2ObjCEtcBoxSet etcBoxes; // +0xfe0 [traced] embedded CAppHanteiEtc (Obj_ConstructPoolSlot 0xFE0)
 Gof2ObjCSousaiBoxSet sousaiBoxes; // +0x1018 [traced] embedded CAppHanteiSousai (+0x1018)
 Gof2ObjCTobiBoxSet tobiBoxes; // +0x1048 [traced] embedded CAppHanteiTobi (+0x1048)
 int eventFreezeFrames; // +0x1074 [traced] sub_49E120: >0 -> decrement and run only the frozen tick (sub_49E0B0); raised to DefStatus+536 for the round-event winner by sub_4A3A80; tested by sub_4353C0, Battle_TickIdleStanceCounters
 int hitStopFrames; // +0x1078 [traced] Obj_RaiseHitStopFrames / HitJudge_ApplyHitStop set it; sub_49E120 counts it down and skips logic; sub_437C70 = 0xFFFF; CComWorkBase/CComWork read >=30 as freeze
 int hitStopLatched; // +0x107c [traced] Obj_InitDefaults = 1, sub_437C70/Obj_RaiseHitStopFrames set 1, sub_430C80 clears; Obj_RaiseHitStopFrames refuses a lower request while it is 1
 int reactionHoldFrames; // +0x1080 [traced] sub_434F90 sets 8 / script value when a knock-back starts; sub_49E120 counts it down (at 1 and flag 0x10000 sets obj+0x1F8), logic that advances the time accumulator runs only at 0 [inferred meaning]
 int shakeFrames; // +0x1084 [traced] sub_496C60 sets; sub_496CB0 getter; sub_49E120 counts down
 int shakeAmplitude; // +0x1088 [traced] sub_496C60 arg; sub_49E120 shakeOffsetX = dword_5A2388[facing]*amplitude*(phase%2)
 int shakePhase; // +0x108c [traced] sub_496C60 zeroes; sub_49E120 ++
 int shakeOffsetX; // +0x1090 [traced] sub_49E120 output; read through shakeOffsetRef by sub_49ECB0 / DrawHelper_Draw
 int * shakeOffsetRef; // +0x1094 [traced] Obj_InitDefaults = &shakeOffsetX; DrawHelper_Draw / sub_49ECB0 dereference it
 int frameTickCounter; // +0x1098 [traced] sub_49E120 ++ (saturating -1); sub_430490 zeroes it when a pending pattern-frame change commits
 int actionAgeTicks; // +0x109c [traced] sub_49E120 ++ (saturating); sub_430490 zeroes on a pending action change
 int actionScriptAge; // +0x10a0 [traced] Obj_ScriptTick ++ (saturating); sub_430490 zeroes on action change; sub_495620 scales damage by it (DefStatus+764/768/772)
 Gof2ObjCArmorState armor; // +0x10a4 [traced] RBO-style armor state, see struct
 unsigned char unused_10C8[4]; // +0x10c8 [unused] only zero-written by Obj_InitDefaults; no reader (no [reg+10C8h] operand elsewhere)
 int hitBonusCount; // +0x10cc [traced] HitJudge_CollectPairs ++ (saturating) when the state script (3,5) answers 2; sub_4944B0 multiplies damage by 1+count*DefStatus(276/280); Obj_GetNormalCancelState family gates on it >0
 void * moveUseRecord; // +0x10d0 [traced] Obj_BindCharaRecordAndStartAction arg a9; per-player record: +0 counter, +4..+0x53 20 move counters (sub_42F120 index of action-60), +0x54 flag, +0x58 last action; sub_4306D0/sub_430630 clear it
 void * scriptTables; // +0x10d4 [traced] Obj_BindCharaRecordAndStartAction = charaContext+64588; +8 = array of 24-byte transition rules (sub_4365D0 / sub_430910)
 void * keyCmdRef; // +0x10d8 [traced] refcounted key-command record (refcount at +72): Obj_ShareKeyCmdRef copies, Obj_ReleaseSubNodes releases, HitJudge_ApplyKnockDirAndFacing reads +8&0xC
 int charaSlotIndex; // +0x10dc [traced] Obj_BindCharaRecord arg a1 (record = ctx+32292*idx+4); sub_4B2110/CharaSelect clear; ScriptVm arg
 void * charaContext; // +0x10e0 [traced] Obj_BindCharaRecord arg a3; +64600 = script bank used by every ScriptVm_CallSyncScript((obj+0x10E0)+64600,..)
 int incomingAttackFrames; // +0x10e4 [traced] ObjScan_StoreAttackGuardInfo: frames until the scanned opponent attack frame; Obj_ScriptTick counts it down; AI hint
 int incomingAttackGuardClass; // +0x10e8 [traced] ObjScan_StoreAttackGuardInfo: 1..3 from attack record +0x7C&3 (0 = none)
 Gof2ObjCTintFxSlot tintFx[3]; // +0x10ec [traced] 3 slots stride 64: sub_49E120 ticks all three; HitJudge_StartHitEffect/sub_43EC80/sub_43EDE0 only start slot 0; slots 1/2 are initialised and reset (sub_43DD40) but never started
 void * tintFxRef[3]; // +0x11ac [traced] Obj_InitDefaults stores &tintFx[0..2]; write-only (no [reg+11ACh..11B4h] reader)
 Gof2ObjCFrameEventFlags frameEventFlags; // +0x11b8 [traced] Obj_SetActionAndRunScript / Obj_ScriptTick set 0x1000000F; sub_49E120/sub_49E0B0 clear bits 4|8 after Obj_ApplyLandingFrameJump; sub_49E830 consumes bit 2; sub_4CDEE0 consumes 0x10000000; sub_48FA20 clears 4
 Gof2ObjCPendingTransition pendingTransition; // +0x11bc [traced] queued state-change request, applied by sub_4353C0 (renamed Obj_ApplyPendingTransition)
 unsigned int contactFlagsMisc; // +0x11d0 [traced reads] sub_43C530 tests bit 4; only ever zeroed (sub_4306D0, sub_430630, sub_430770, CharaSelect_*Pose) -> effectively 0
 int cancelLockFrames; // +0x11d4 [traced] sub_43C460 sets DefStatus+1144 after a hit; sub_430C80 counts down; Obj_GetNormalCancelState* return "not cancellable" while !=0; sub_4353C0 clears
 int hitDuringAction16; // +0x11d8 [traced] sub_43C530 sets 1 when the victim is in action 16; sub_433D00 cancel conditions read it; cleared by sub_4306D0
 int hitDuringAction19; // +0x11dc [traced] same for action 19
 Gof2ObjCPositionBlock position; // +0x11e0 [traced] see struct (RBO localX/Y/Z + posX/Y/Z + prevLocalX/Y)
 Gof2ObjCEffectList effects; // +0x1210 [traced] ObjEffect list: sub_442180 append, sub_442430 tick, sub_4422E0 clear, ObjEffect_RemoveById, sub_442340 aggregate
 Gof2ObjCRuleOverrideFlags ruleOverrideFlags; // +0x121c [traced reads] sub_42F7A0 (state-rule evaluator) bit 8 forces the rule to fail; only zeroed elsewhere (ctor, Obj_InitDefaults, sub_43DD40)
 Gof2ObjCHitEffectStyleFlags hitEffectStyleFlags; // +0x1220 [traced reads] HitJudge_ResolveTick bits 1/2/4 pick hit-effect style 1/2/3 when the attack record has none; bit 8 selects DefStatus+1104/1108/1112 guard bonus (sub_497120, sub_497160, HitJudge_ApplyHit); only zeroed elsewhere
 int fixedHalfRate; // +0x1224 [traced] Obj_SetActionAndRunScript sets; sub_49E120: non-zero -> time accumulator drops 128 per tick instead of timeScale
 int timeAccum; // +0x1228 [traced] Obj_SetActionAndRunScript = 128; Obj_ScriptTick / sub_4353C0 += 256; sub_49E120 -= rate each tick, <=0 skips the script step (slow-motion / half speed)
 int timeScale; // +0x122c [traced] default 128 (ctor); sub_438660 sets 256/copies parent or child; CComWorkBase/CComWorkNormal compare >= 0.875*timeScaleBase
 int timeScaleBase; // +0x1230 [traced] default 128; sub_438660; sub_43DD40 recomputes timeScale from it
 Gof2ObjCZoomBlock zoom; // +0x1234 [traced] see struct
 Gof2ObjCSharedNodes sharedNodes; // +0x1254 [traced] Fighter_AllocSharedSubNodes 0x42E9C0 fills +0x38..+0x44 of obj+0x121C; Obj_ReleaseSubNodes/Fighter_ReleaseSharedSubNodes free them
};
