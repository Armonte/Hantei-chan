struct RboAtRecord;
struct RboCharContext;
struct RboActor;
struct RboActor {
 void * ownerListPrev; // +0x000 [traced] ObjPool_LinkIntoOwnerList 0x43CA60: node[0] = previous node (head record for the first); ObjPool_PopFreeAndClear 0x43C970
 struct RboActor * ownerListNext; // +0x004 [traced] ObjPool_LinkIntoOwnerList; every owner-list walk is `i = *(i+4)` (sub_422340, sub_44A580, sub_449F60, sub_44A1F0)
 struct RboActor * prevSibling; // +0x008 [traced] ObjPool_AllocChild 0x43CC00: new child +8 = 0, old first child +8 = new child
 RboActor * nextSibling; // +0x00C Actor_DrawTree 0x4483E0 walks v2+12
 struct RboActor * parent; // +0x010 [traced] ObjPool_AllocChild: child+16 = parent; Actor_AddAncestorOffsets 0x442230 climbs result[4]; Obj_TreeLatchPositions
 RboActor * firstChild; // +0x014 Actor_DrawTree recurses into v2+20
 struct RboActor * spawner; // +0x018 [traced] sub_41ACF0 spawn: new+24 = creating actor; sub_41C710 cmd 27 copies spawner+2368; sub_41A650 sub_43F200(a1,a1[6])
 unsigned char unused_01C[4]; // +0x01C [unused] no actor-base access in the 372 actor-code functions (0x419000-0x470000, decompiled and var-tracked); every [reg+1Ch] hit is a script record, draw item or other struct
 RboObjectKind objectKind; // +0x020 [traced] 1 set by sub_421F80/sub_4220E0/sub_44CD50 (character-bound objects built via sub_421E40 char index), 2 by sub_43C1C0/43C410/43C4E0/41ACF0 (spawned effects/children)
 int destroyRequested; // +0x024 [traced] sub_43FB10 frame loop: if (+36) -> sub_43F9C0 destroy else tick children; set to 1 by sub_43F360, sub_44A1F0 case 5, sub_440550 (lifetime end), sub_441A00
 RboActorClassFlags classFlags; // +0x028 [traced] low nibble copied to spawned children (sub_41ACF0), 0x10 = player entity (sub_44A1F0/sub_449F60 require (flags&0xF0)==0x10), 0x20 spawned, 0x40 inherited from spawner; HitSkill* test (flags & 0x20100); sub_41C710 cmd 10 edits; preserved across Actor_Init
 int ownerSlotTag; // +0x02C [traced] Actor_Init arg a2 (-1 = none, roster slot+3 for sub_421F80 objects, copied from parent); filter key in sub_41D350/sub_449F60/sub_44A010; indexes per-slot tables in sub_466060/sub_464C70
 int charaId; // +0x030 [traced] sub_41ACF0 = CharData+4 (character id); sprintf "charaid %d" in sub_43C750; compared in sub_41B6B0/sub_41D350; preserved across Actor_Init
 int charaVariant; // +0x034 [inferred] compared with dword_AB16B0 next to charaId/pattern/frame (sub_41B710, sub_41B790); script arg 4/6 of ComActWork/CallItemCreate; copied from spawner; preserved across Actor_Init; sub_41C710 cmd 14
 int scriptVar38; // +0x038 [inferred] only writer sub_41C710 cmd 28; no native reader (script-visible work value)
 int playerId; // +0x03C Actor_Init arg a3; compared in the hit resolver sub_4433E0 (v3[15])
 int hitMaskShift; // +0x040 [traced] Actor_Init arg a4 (slot index); Hit_RegisterHitMask 0x443100 / sub_442FD0: hit_mask[side] |= hitMaskBit << hitMaskShift
 unsigned int hitMaskBit; // +0x044 [traced] Actor_Init sets 1; sub_41C710 cmd 23 sets; used as bit value in Hit_RegisterHitMask / Combat_ResolveGuardContact
 int paramA; // +0x048 [inferred] sub_41C710 cmd 16, sub_421F80 a4 / sub_4220E0 a3, copied from spawner (sub_41ACF0); no native reader
 int paramB; // +0x04C [inferred] sub_41C710 cmd 21, sub_421F80 a5; passed as script arg 7 (sub_43C750) / 9 (sub_43E730)
 unsigned int scriptFlags; // +0x050 [traced] bits set/cleared by sub_44A1F0 cases 11/12 (tag-targeted command); script arg 10 of sub_43E730
 int indicatorMode; // +0x054 [traced] sub_466190 switch(+84): -1 none, 0..4 forced style, else caller default (screen-X mapped marker quad); sub_41C710 cmd 12; spawn sets -1/0; preserved across Actor_Init
 int indicatorParam; // +0x058 [inferred] sub_41C710 cmd 13 only; no native reader
 unsigned char pattern; // +0x05C Actor_CacheCurrentFrame 0x440920
 unsigned char frame; // +0x05D frame number inside the pattern
 unsigned char unused_05E[2]; // +0x05E [unused] no [reg+5Eh]/[reg+5Fh] access in actor code; the dword hits at +5Ch belong to RboAtRecord (Hit_ComputeFinalDamage, sub_421B40...)
 struct RboFrameRecord * nextFrame; // +0x060 Actor_ResolveNextFrameRecord 0x440970
 unsigned __int16 nextSpriteId; // +0x064 Actor_GetFrameSpriteId result
 unsigned char unused_066[2]; // +0x066 [unused] same proof: the [reg+64h] dword hits are RboAtRecord / other structs
 int frameTicks; // +0x068 compared with frame.duration in Actor_TickFrame 0x43F830
 unsigned char loopCounter; // +0x06C loaded from frame.loopCount in Actor_EnterFrame 0x43EF30
 unsigned char unused_06D[3]; // +0x06D [unused] same proof: [reg+6Ch] dword hits are RboAtRecord fields
 RboShadowStyleFlags shadowStyleFlags; // +0x070 [traced] Actor_DrawTree 0x4483E0 (&2 skip alpha pass, &4 use +116), Actor_DrawPartsPose 0x447720 (&1 tint from part); sub_41A650 case 18
 int shadowAlphaOverride; // +0x074 [traced] Actor_DrawTree: alpha for shadow pass when shadowStyleFlags&4; sub_41A650 case 18
 int shadowMode; // +0x078 [traced] Actor_DrawTree: non-zero = second shadow pass; sub_448340 maps 1..4 -> draw item style; sub_41A650 case 7 (0..4)
 int shadowHeightOffset; // +0x07C [traced] Actor_DrawTree copies it into the draw item; sub_41A650 case 7.3 sets from posY or a literal; sub_440550 recomputes remaining*start/total while the tween runs
 int shadowTweenActive; // +0x080 [traced] sub_41A650 case 7.3.2 sets 1; sub_440550 clears at the end
 int shadowTweenStart; // +0x084 [traced] sub_41A650 (copy of +124 at arm time); sub_440550 numerator
 int shadowTweenRemaining; // +0x088 [traced] sub_440550 decrements per tick, tween ends at 0
 int shadowTweenTotal; // +0x08C [traced] sub_41A650 sets; sub_440550 divisor
 RboInheritMask inheritOffsetMask; // +0x090 [traced] Actor_AddAncestorOffsets 0x442230: bit1/2/4 = add ancestor localX/Y/Z, mask ANDed up the chain; sub_41A650 case 1; sub_43C4E0 sets 7
 int localX; // +0x094 [traced] Actor_Init arg a5; Obj_TreeLatchPositions: world = local + ancestors; Actor_StepMotionMovers 0x44BFF0 passes &localX to every mover step
 int localY; // +0x098 [traced] Actor_Init arg a6; sub_442890 landing test (y>=0 && prevLocalY<y); movers write it
 int localZ; // +0x09C [traced] Obj_TreeLatchPositions copies to posZ; eased by depthTween (sub_44B9C0)
 int posX; // +0x0A0 sub_440D50 box placement
 int posY; // +0x0A4 
 int posZ; // +0x0A8 
 struct RboMover mainMover; // +0x0AC [traced] Actor_Init Velocity_Init(255,a1+172); Actor_GetVelocityBlock(actor+172) used by Actor_FrameMoveAddSpeed/ClearAxes, sub_41C230, Actor_ApplyEnterBrake; sub_41CCC0 slot 0 re-kinds it; sub_41C290 makes it a path follower; stepped first in Actor_StepMotionMovers
 int depthTweenActive; // +0x0F8 [traced] Actor_Init 0; sub_44BFF0 runs depthTween.step while non-zero, clears when step returns 0
 struct RboDepthTween depthTween; // +0x0FC [traced] sub_44BA70 (init, called by sub_41BEA0 case 2) / sub_44BA30 / sub_44B9C0; sub_44BFF0 calls step(&drawDepthOffset,&localZ,&depthTween)
 int prevLocalX; // +0x120 [traced] sub_44BFF0 saves localX before moving
 int prevLocalY; // +0x124 [traced] sub_44BFF0 saves localY; sub_442890 compares it with localY to detect landing
 int brakeMoverActive; // +0x128 [traced] Actor_ApplyEnterBrake 0x43F0E0 sets 1; Actor_Init 0; sub_44BFF0 clears when step returns 0
 struct RboMover brakeMover; // +0x12C [traced] Actor_ApplyEnterBrake: copies mainMover velocity (sub_44B990) then arms limited accel -vel/20 (sub_44B7C0 flag 8)
 int scriptMover1Active; // +0x178 [traced] Actor_Init 0; sub_44BFF0 guard; sub_41CCC0 slot 1 sets 1
 struct RboMover scriptMover1; // +0x17C [traced] Actor_Init sub_44B590 (kind 0); sub_41CCC0 slot 1 selects kind 0..3 from a script block; sub_44BFF0 steps it
 int spareMoverActive; // +0x1C8 [traced] Actor_Init zeroes it; no other access in actor code (never armed)
 struct RboP2MotionSlot motionSlot460; // +0x1CC [traced] 5th motion slot (counter at +456); Actor_Init 0x441A90 Mover_InitLimitedAccel; stepped by Actor_StepMotionWithTimedSlot 0x4404F0 (fn at +472). Bytes 0x200..0x217 are its tail
 struct RboP2MotionSlot knockbackMotion; // +0x218 [traced] Velocity_Init(actor+536) in Actor_StartKnockbackMotion 0x444CE0 (script BadCndAnimeStopVData); stepped in sub_440550 via fn at +548 while state mask 0x1E0 set
 struct RboP2AutoMove autoMove; // +0x264 [traced] walk-to-target state: Actor_StepAutoMove 0x44BBD0, Actor_SetAutoMoveTargetX 0x4418B0, Actor_MarkKilledAndCleanup 0x4419A0, script op 7 in sub_41C710
 int cancelMoveCount; // +0x298 [inferred] ++ in sub_43DD80 (starts pattern 38/39), cleared by sub_4414D0/sub_43EEC0, >0 blocks Actor_NormalCancelAllowed via sub_43DAA0
 struct RboP2DrawOffsetKeys drawOffsetKeys; // +0x29C [traced] read by Actor_GetBoxOrigin 0x440B50, Actor_GetDrawOffsetLerp 0x445C20, Actor_DrawCgSprite 0x4470E0 when drawOffsetMode != 0; no direct writer found (script/memcpy)
 unsigned char unused_2AC[4]; // +0x2AC [unused] no [reg+0x2AC] access in any actor function; Actor_Init does not set it
 RboActor * depthParent; // +0x2B0 [traced] Actor_SumParentDepthOffsets 0x442330 walks it; spawner sub_41ACF0 sets it to the owner (+16) for relative-depth mode 0x40000
 int drawDepthOffset; // +0x2B4 sub_440DF0 (draw priority codes 12..61)
 int drawLayer; // +0x2B8 sub_440DF0 (codes 1..11); Actor_DrawTree selects actors by layer
 int fadeOutTicks; // +0x2BC [traced] sub_440550: while non-zero actor ticks every frame; reaching 0 with flag 0x100 of +2176 sets +36 (kill). Set by sub_43F2A0 (fade-out start)
 int hitStopTicks; // +0x2C0 [traced] set by Actor_SetHitStop 0x419950 (hit-event ops case 0/1 in sub_41EED0); while >0 sub_440550 decrements it and skips the anim clock
 int flowRecoverCountdown; // +0x2C4 [traced] Actor_TickFrame 0x43F830: when it equals 1, sub_43F620 jumps to Actor_FindFlowTargetFrame; set by sub_444DF0/sub_444F60 (hit-reaction anim), cleared by sub_4414E0
 int freezeTicks; // +0x2C8 [traced] sub_440550: while >0 only this timer runs (no timers/anim) except the 0x1E0 knockback motion; idle AI loops sub_43EBD0/sub_43EC50 require 0
 unsigned char drawOffsetMode; // +0x2CC [traced] RboP2DrawOffsetMode: !=0 enables drawOffsetKeys (Actor_GetBoxOrigin), ==1 = interpolated draw from actor +1312 (Actor_GetFrameInterpolation 0x445A50, Actor_DrawTree); also tested by sub_442890
 unsigned char unused_2CD[3]; // +0x2CD [unused] only the byte at 0x2CC is ever accessed
 int shakeTicks; // +0x2D0 [traced] Actor_SetHitShake 0x4199A0 sets it; Obj_TreeLatchPositions 0x43CED0 tests it; sub_440550 decrements it
 int shakeTickCounter; // +0x2D4 [traced] Obj_TreeLatchPositions ++ it, parity gives the +/- shake
 int unused_2D8; // +0x2D8 [unused] no [reg+0x2D8] access in any actor function
 int shakeAmplitude; // +0x2DC [traced] set by Actor_SetHitShake (arg a3), multiplied in Obj_TreeLatchPositions
 int shakeOffsetX; // +0x2E0 [traced] Obj_TreeLatchPositions writes +/-(counter%2 * amplitude) (sign by facingLeft); Actor_DrawTree adds *shakeOffsetXPtr to posX
 int * shakeOffsetXPtr; // +0x2E4 [traced] Actor_Init: points at +736; sub_41A650 may point it at the owner shake offset
 int blendMode; // +0x2E8 Actor_EnterFrame copies frame.blendMode here
 struct RboP2RampFx alphaFx; // +0x2EC [traced] AlphaFadeFx_Init 0x4491C0 / AlphaFadeFx_StartRamp 0x449280; ticked by sub_440550 via fn at +748; value (+752) = alpha used by Actor_DrawTree
 struct RboP2ZoomFx zoomFx; // +0x308 [traced] ZoomFx_Init 0x4496A0 (256 = 1.0); Actor_EnterFrame sets it from frame.zoom; Actor_DrawTree reads it via getter at +780
 struct RboP2RgbFx modulateColorFx; // +0x340 [traced] RgbFx_Init 0x4493B0(255,255,255); rgb at +840 multiplied into the draw colour by sub_4464D0
 struct RboP2RgbFx additiveColorFx; // +0x390 [traced] RgbFx_Init(0,0,0); rgb at +920 is the additive tint read by Actor_GetFlashAddColor 0x446350
 struct RboP2FlickerGate flickerGate; // +0x3E0 [traced] FlickerGate_Tick 0x440DC0 gates the draw in Actor_DrawTree; Actor_AdvanceByAniFlag sets (0,3,1,3)
 struct RboP2RotFx rotXFx; // +0x3F0 [traced] RotFx_InitStatic 0x449AB0; sub_41A1D0 script op; ticked by sub_440550 via fn at +1012; angle (+1008) is v36 in Actor_DrawTree (axis order inferred)
 struct RboP2RotFx rotYFx; // +0x410 [traced] same as rotXFx; angle (+1040) is v37 in Actor_DrawTree
 struct RboP2RotFx rotZFx; // +0x430 [traced] same as rotXFx; angle (+1072) is v38 in Actor_DrawTree
 struct RboP2ZoomFx zoomAddFx; // +0x450 [traced] ZoomFx_Init(actor+1104,0); Actor_DrawTree adds its getter result (+1108) to zoomFx
 void * afterimageTrail; // +0x488 [traced] ring buffer of 64 x 44-byte entries allocated by sub_44AB10 in sub_41C290 case 0; sub_44ABA0/sub_44AC30 record into it from sub_440550/sub_43FB10
 int scriptWork[10]; // +0x48C [inferred] script-visible actor local variables: sub_42A1C0/sub_42A1D0 hand actor+1164 to the script VM, sub_41E400 indexes it as dword array; size bounded by colorFlash at +1204
 struct RboP2ColorFlash colorFlash; // +0x4B4 [traced] Actor_StartColorFlash 0x4487A0, Actor_GetFlashAddColor 0x446350, ticked in sub_440550 (bytes +1205/+1207)
 struct RboP2RepeatTracker repeatTracker; // +0x4C8 [traced] move-repeat penalty tracker: RepeatTracker_Reset 0x4415A0, Actor_UpdateRepeatPenalty 0x4415C0, Actor_InitRepeatBonus 0x441720, Actor_ApplyRepeatPenalty 0x441760; extends to +1311 (history ring)
 RboActor * frameMirrorSource; // +0x520 T: Actor_GetFrameInterpolation 0x445A50 reads +104/+1380 of this actor when byte +0x2CC == 1; no writer found (only read)
 int jumpCounter; // +0x524 Actor_TickFrame increments on pattern change
 unsigned __int16 pendingPattern; // +0x528 Actor_ApplyLandJump/hit routines; consumed by Actor_TickFrame, 0xFFFF = none
 unsigned __int16 pendingFrame; // +0x52A 0xFFFF = none
 unsigned char pendingFlip; // +0x52C Actor_TickFrame, 0xFF = none
 unsigned char unused_52D[3]; // +0x52D U: pendingFlip is accessed only as byte at +0x52C (all 80 operands scanned), no access to 0x52D..0x52F
 int pendingFlipNegatesVelocity; // +0x530 Actor_TickFrame
 unsigned char facingLeft; // +0x534 Actor_FrameMoveAddSpeed 0x44B3D0
 unsigned char unused_535[3]; // +0x535 U: facingLeft only accessed as byte (movsx/cmp byte ptr), no access to 0x535..0x537
 unsigned char * facingLeftPtr; // +0x538 Actor_Init: points at +1332
 unsigned char fixedFacingValue; // +0x53C T: sub_41ACF0 (child spawn, flags 0x100/0x200) stores 0/1 and redirects facingLeftPtr here; read only through facingLeftPtr
 unsigned char landedLatch; // +0x53D T: sub_44BFF0 sets 1 on landing, sub_441850 clears; read sub_43DF20, sub_43F460, sub_44ABD0
 unsigned char unused_53E[2]; // +0x53E U: alignment pad after the two bytes at +0x53C/+0x53D, no access
 int aiActionParam; // +0x540 T: sub_43E730 (AI com_act_work call) passes it as script arg 2 and replaces it with result arg 3
 int aiThinkTimer; // +0x544 T: sub_43EC50: <0 off, 0 = run sub_43E730 now, >0 counts down; frame-enter ops 9/15 and sub_44A1F0 case 8 write
 int aiFlags; // +0x548 T: sub_43E730 tests bit 2 (face nearest edge); written by sub_44A750 script command
 int scriptTag; // +0x54C T: frame-enter op 18 sets/adds; copied to spawned children (sub_41ACF0/41BF80); sub_449D50 filters script commands by it
 struct RboP3BufferedJump bufferedJump; // +0x550 T: queued pattern/frame/flip request (kind -1 none) written by sub_41E020/41FA50/41FFA0, consumed by sub_41E500
 int altDrawMode; // +0x560 Actor_CacheCurrentFrame: pattern flag 0x40
 struct RboFrameRecord * curFrame; // +0x564 Actor_CacheCurrentFrame
 unsigned char unused_568[4]; // +0x568 U: no operand with displacement 0x568 in the whole text; node pool starts at +0x570 and sub_41F900/41DA50 index from +0x568 only with +8/+48 constants
 int * singleSlotTimedRule; // +0x56C T: Actor_LinkTransitionRuleEvent category 6 stores the event record; Actor_RunTimedJumpRules 0x420520 evaluates it first
 struct RboP3RuleNode transitionNodePool[5]; // +0x570 T/I: Actor_RunFrameScriptList7 0x41F900 links nodes at actor+0x570+8*n; capacity 5 inferred from pool boundary (+0x598)
 struct RboP3EnterNode frameEnterNodePool[20]; // +0x598 T/I: Actor_RunFrameScriptList6 0x41DA50 links nodes at actor+0x598+12*n; capacity 20 inferred from pool boundary (+0x688)
 struct RboP3TransitionHeads transitionHeads; // +0x688 T: Actor_ClearTransitionRuleLists 0x41F7C0 / Actor_LinkTransitionRuleEvent 0x41F7F0 (6 heads, 8-byte stride)
 struct RboP3FrameEnterHeads frameEnterHeads; // +0x6B8 T: Actor_ClearFrameEnterLists 0x41D990 / Actor_LinkFrameEnterEvent 0x41D9B0 (3 heads, 12-byte stride)
 int * perTickScript; // +0x6DC T: frame-enter op 0x81 (Actor_SetPerTickScript) stores a record {kind,type,index}; ActorTree_TickFrames runs it every tick via Actor_RunPerTickScript
 void * attachedOverlayObj[2]; // +0x6E0 I: sub_43BD40 / sub_44A870 / sub_43BA00 pool objects (banner/caption overlays) with back pointer obj+128 = actor
 struct RboP3KillerRecord killer; // +0x6E8 T: Actor_RecordKillerOnDeath 0x442150, sub_4419A0, sub_43F9F0, sub_43C750
 struct RboP3ArmorState armor; // +0x700 T: super-armor state, Combat_ResolveDamageHit 0x444530, sub_443890, sub_4438C0, sub_441800, sub_440550
 RboP3ArmorDeathFlags armorDeathFlags; // +0x710 T: frame-enter op 17 writes; Hit_ApplyHpDamage tests bit0 (keep armor state on lethal damage)
 struct RboP3AttackHitState attackHit; // +0x714 T: head of the attack state at +0x714 (Actor_UpdateAttackState 0x441060, Hit_RegisterHitMask 0x4430D0, Combat_CountContactHit 0x4449D0); continues at +0x734 and +0x74C
 struct RboP3MultiHitGate multiHitGate; // +0x734 T: Hit_MultiHitGateReady/Latch/Advance 0x4429A0/0x442960/0x4429E0 take actor+1844
 int attackBoxCount; // +0x74C a1[467] Actor_CollectAttackBoxes2 0x440F40
 struct RboAtRecord * curAttack; // +0x750 a1[468]: 120-byte AT record
 void * attackBoxRect[2]; // +0x754 a1[469..470] pointers into the box section
 int attackBoxSlot[2]; // +0x75C a1[471..472] index of the slot in frame.attackBoxIdx
 struct RboP3HurtTimers hurtTimers; // +0x764 T: head of the hurt state at +0x764 (hurtBoxCount at +0x770); sub_440550, Hit_ClassifyGuardOutcome 0x443330
 int hurtBoxCount; // +0x770 a1[476] Actor_CollectHurtBoxes3 0x441120
 void * hurtBoxRect[3]; // +0x774 a1[477..479]
 int hurtBoxSlot[3]; // +0x780 a1[480..482]
 struct RboP4ReactionState reaction; // +0x78C sub_441940/sub_444C10/sub_444C60/sub_444DF0/sub_444F60/Hit_SetVictimReactionAnim/sub_445550 [traced]
 struct RboP4KasanariTouchState kasanariTouch; // +0x7B0 sub_441260/sub_441280/Actor_UpdateBoxes272State/sub_419840/sub_4198B0/Actor_InputMaskMatches [traced]
 int kasanariBoxCount; // +0x7D8 a1[502] Actor_CollectBoxes2_272 0x4411E0
 void * kasanariBoxRect[2]; // +0x7DC a1[503..504]
 int kasanariBoxSlot[2]; // +0x7E4 a1[505..506]
 struct RboCharContext * charContext; // +0x7EC Actor_Init
 struct RboPatternAreaView * patternArea; // +0x7F0 Actor_Init: charContext+28
 void * inputBuffer; // +0x7F4 player input object: factory sub_4425E0 (sub_45E990), release sub_45E9E0, current input bits +0x10 read by sub_43E1B0 [traced]
 void * vitals; // +0x7F8 per-fighter vitals block: hp +24, max hp +28, guard gauge +32/+36, guard flag +96 (Actor_ApplyFrameStatusFlags, Hit_ApplyHpDamage) [traced]
 void * fighterLoadout; // +0x7FC per-fighter loadout/stat block (ids +8/+12/+16, stat vector +84 passed to scripts, +100 cancel limit); factory sub_442520/sub_4423F0 [traced]
 void * combatModifiers; // +0x800 damage/defence modifier block, base triples +8/+20/+32, derived table +88 (sub_4423F0, sub_4571E0, Hit_ComputeFinalDamage) [traced]
 void * scriptVarBlock; // +0x804 script variable table: dword array at +8 indexed by condition ops (sub_418C40, sub_419140, sub_41D200, sub_44E4C0 fills it) [traced]
 void * hudWidget; // +0x808 HUD gauge widget allocated by sub_4668E0, freed sub_466880, cloned sub_4668D0 [traced alloc/free, inferred meaning]
 enum RboP4WallContact wallContactFlags; // +0x80C sub_43FD50 (edge/ceiling/floor bits), sub_442640 (stage bound bits 1/2), reset sub_441850 [traced]
 int isGuardingPose; // +0x810 Actor_ApplyFrameStatusFlags: 1 when pattern in {17,18,19,44,45,46}; read sub_418C40 cond 1, Combat_ResolveGuardContacts [traced]
 int isInHitStunPose; // +0x814 Actor_ApplyFrameStatusFlags: 1 when pattern in {23..27,29,30}; read sub_443870, Hit_RollEvade [traced]
 int stanceClass; // +0x818 Actor_ApplyFrameStatusFlags 0x441330 copies frame.stanceClass
 unsigned int hitClassMask; // +0x81C Actor_ApplyFrameStatusFlags: 895 minus frame.hurtMaskFlags classes
 struct RboP4FollowState follow; // +0x820 sub_41A650 ops 0..3, sub_440550, sub_4197B0, sub_4198E0; Actor_Init copies +0x820..+0x8A3 from the old actor [traced]
 struct RboP4CameraFocus cameraFocus; // +0x82C Camera_FrameActors 0x42FFD0, sub_44A1F0, sub_43FD50 [traced]
 struct RboP4ScreenBound screenBound; // +0x83C sub_43FD50 0x43FD50, sub_41A650 ops 4/5/6/17/19 [traced]
 struct RboP4ParentLink parentLink; // +0x860 sub_43F460/sub_43F360/sub_43F410/sub_43F2A0/sub_442890/sub_442CC0, sub_41A650 ops 8..16 [traced]
 enum RboP4SetupFlags setupFlags; // +0x8A0 sub_41A650 op 20 (set/and/or/clear); read Actor_EnterFrame, sub_43EBD0, Actor_ApplyEnterBrake, Actor_ApplyFrameMovement, sub_419950, sub_41ACF0, Actor_RunFrameScriptList6/7 [traced]
 struct RboP4ColorFxSlot colorFxSlot[2]; // +0x8A4 Actor_Init (sub_442130 x2), sub_440550, sub_440330, sub_448810, sub_4464D0 [traced]
 struct RboP4ColorFxSlot * colorFxSlotPtr[2]; // +0x8F4 Actor_Init points them at the own slots; sub_41A650 op 24 can point them at parent/root slots; read by sub_4464D0 [traced]
 enum RboP4StatusKindMask activeStatusKindMask; // +0x8FC sub_4571E0 rebuilds it (bit = status kind 3..14); tested with 0x1E0 in sub_440550/Hit_SetVictimReactionAnim [traced]
 enum RboP4StatusHookMask statusHookMask; // +0x900 sub_4571E0; read sub_45B5A0/sub_45B5F0/Hit_ComputeFinalDamage [traced]
 int cancelLockoutTimer; // +0x904 sub_41C710 op 25 sets, sub_440550 decrements, sub_418C40 cond 16 blocks while non-zero [traced]
 struct RboP4BufferedMove bufferedMove; // +0x908 sub_43E1B0, sub_4209B0, sub_4414B0 [traced reads; arming write not found]
 int * cancelCounters; // +0x91C points to 6 per-player use counters (24-byte stride table at 0x2615B60): sub_43D8A0 alloc, sub_43D8B0 clear, sub_43D8C0 limit test [traced]
 void * comboPopup; // +0x920 damage/combo number popup object: sub_462930 creates/updates (slot back-pointer at +80), sub_4628E0 frees [traced]
 void * overlayQuadList[2]; // +0x924 heads of two linked lists of overlay quads built by sub_41CDA0, drawn by sub_446FE0/Actor_DrawSprite, freed sub_44B340 [traced]
 void * statusEffectList; // +0x92C head of status effect node list: sub_456B70/456DB0/456BA0/4569F0 manipulate, sub_456950 frees, sub_4571E0 folds [traced]
 int ticksUntilWrap; // +0x930 Actor_TickFrame: += 256 when <= 0
 int tickRateBase; // +0x934 Actor_Init a8; 256 = normal (sub_44E1A0 clamps 1..256); sub_43F1B0/sub_43F1D0/sub_43F200/sub_4571E0 [traced]
 int tickRate; // +0x938 Actor_Init a9; effective rate subtracted from ticksUntilWrap each tick via tickRatePtr (sub_440550) [traced]
 int * tickRatePtr; // +0x93C Actor_Init: &tickRate; sub_41C710 op 22 can point it at the parent tickRate [traced]
 unsigned int skillHookLatch; // +0x940 bit 1 = skill script slot 0x3D already fired (sub_45A320); inherited from parent by sub_41C710 op 27 [traced]
 unsigned int enterFlags; // +0x944 Actor_EnterFrame sets 7
};
