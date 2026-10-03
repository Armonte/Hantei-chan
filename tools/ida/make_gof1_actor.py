#!/usr/bin/env python3
"""Generates docs/formats/ida/gof1_actor_types.h (IDA-parsable decls) for the GOF1 (gof.exe) runtime actor.

Evidence tags in the comments: T = reader/writer traced in gof.exe (function names are the IDB names), I = traced, meaning inferred from
the code shape, U = never accessed by any actor-typed function of gof.exe (ctree scan of all 953 functions + displacement scan); the
byte is zeroed by memset(actor, 0, 0x290) at spawn and carries no data. Every byte of every struct must be covered by a field: any gap or
overlap is a hard error, so the number of unmapped bytes is 0 by construction.   usage:  python3 tools/ida/make_gof1_actor.py > docs/formats/ida/gof1_actor_types.h
"""
import sys

ENUMS = r'''
enum Gof1ObjKind : unsigned char { G1OBJ_FIGHTER=0, G1OBJ_GHOST_TRAIL=31, G1OBJ_CHILD_8F=143, G1OBJ_CHILD=255 };
enum Gof1ControlType : unsigned char { G1CTRL_HUMAN=0, G1CTRL_CPU=1 };
enum Gof1RunState : unsigned char { G1RUN_IDLE=0, G1RUN_PENDING=1, G1RUN_FORCED=2 };
enum Gof1ObjFlagsA : unsigned short { G1OF_A_END_WITH_PARENT_STATE=1, G1OF_A_FORCE_FACE_RIGHT=2, G1OF_A_INHERIT_PARENT_MOTION=4, G1OF_A_PART_OF_OWNER_ATTACK=8, G1OF_A_END_ON_PARENT_ACTION_CHANGE=0x20, G1OF_A_NO_GROUND_LANDING=0x40 };
enum Gof1ObjFlagsB : unsigned short { G1OF_B_DRAW_SHADOW=1, G1OF_B_STEP_FRAME_WITH_PARENT=2, G1OF_B_END_WHEN_PARENT_AIR_OR_GRABBED=4, G1OF_B_HITSTOP_TO_OWNER=8, G1OF_B_IGNORE_SCREEN_ZOOM=0x80 };
enum Gof1DecorFlags : unsigned short { G1DECOR_DEPTH_BUMP=1 };
enum Gof1HomingMode : unsigned char { G1HOME_NONE=0, G1HOME_EASE_TO_TARGET=1, G1HOME_CONSTANT_SPEED=2 };
enum Gof1AttackResultFlags : unsigned char { G1HITRES_HIT=1, G1HITRES_GUARDED=2, G1HITRES_TESTED_BIT4=4, G1HITRES_EVADED_STRIKE=8, G1HITRES_VICTIM_GROUND=0x10, G1HITRES_VICTIM_AIR=0x20 };
enum Gof1FlashMode : unsigned char { G1FLASH_BLUE_BLINK=0, G1FLASH_WHITE_BLINK=1, G1FLASH_YELLOW_BLINK=2, G1FLASH_GREEN_BLINK=3, G1FLASH_GREY_BLINK=4, G1FLASH_WHITE_FADE=5, G1FLASH_BLUE_FADE=6, G1FLASH_RED_FADE=7, G1FLASH_CYAN_FADE=8 };
enum Gof1InputDirection : unsigned char { G1DIR_NEUTRAL=0, G1DIR_DOWN_LEFT=1, G1DIR_DOWN=2, G1DIR_DOWN_RIGHT=3, G1DIR_LEFT=4, G1DIR_RIGHT=6, G1DIR_UP_LEFT=7, G1DIR_UP=8, G1DIR_UP_RIGHT=9 };
enum Gof1InputButtonFlags : unsigned int { G1BTN_A=1, G1BTN_B=2, G1BTN_C=4, G1BTN_D=8, G1BTN_E=0x10, G1BTN_F=0x20, G1BTN_A_PRESSED=0x1000, G1BTN_B_PRESSED=0x2000, G1BTN_C_PRESSED=0x4000, G1BTN_D_PRESSED=0x8000, G1BTN_E_PRESSED=0x10000, G1BTN_F_PRESSED=0x20000 };
enum Gof1CommandMoveClass : unsigned char { G1MOVE_NORMAL=0, G1MOVE_SPECIAL=1, G1MOVE_SUPER=2 };
enum Gof1CommandFlags : unsigned char { G1CMD_STANCE_GROUND=1, G1CMD_STANCE_AIR=2, G1CMD_STANCE_CROUCH=4, G1CMD_NO_CANCEL_ENTRY=8, G1CMD_CLEAR_HISTORY_ON_ACTION=0x10, G1CMD_SCRIPT_ONLY=0x20, G1CMD_GUARD_CANCEL=0x40 };
enum Gof1CommandFlags2 : unsigned char { G1CMD2_USE_RING_B=1, G1CMD2_STRICT_SEQUENCE=2, G1CMD2_EVADE_MOVE=0x80 };
enum Gof1CtFlags : unsigned char { G1CT_RESTRICT_REPEAT_MOVES=1, G1CT_EVADE_ENABLED=2, G1CT_JUST_GUARD_ENABLED=8, G1CT_UNREAD_BIT4=0x10, G1CT_UNREAD_BIT5=0x20 };
'''

# (offset, size, ctype, name, comment)
ACTOR = [
 (0x000, 1, 'unsigned char', 'slotIndex', 'T: fighter slot / owner player index 0..3 (SetupFighterSlot sub_403A90 stores the slot id; per-player tables dword_18645A0[130*i]; EfType9 sound bank arg0+50*(i+8); children copy it in InitChildObject)'),
 (0x001, 1, 'unsigned char', 'characterId', 'T: roster character id (sub_403A90: dword_17919D4[42*sel]); copied through InitFighterSlotForRound; only reader ObjRunActionScript compares it with 34'),
 (0x002, 1, 'enum Gof1ControlType', 'controlType', 'T: 0 human, 1 CPU (sub_403A90 a5 == 1; Battle_UpdateAllCharactersInputAndAI, Actor_RunFrameIfRecord IF 11, Damage_ScaleByComboAndLife, sub_43A120)'),
 (0x003, 1, 'unsigned char', 'unreferenced_03', 'U: no access in any function'),
 (0x004, 1, 'enum Gof1ObjKind', 'objKind', 'T: 0 fighter; 31 ghost trail (SpawnEffectFromActorFrame); 0xFF/0x8F spawned child (InitChildObject, SpawnChildEffectFromParent); >= 0xF0 is non-fighter (sub_4242C0, sub_424560, ObjScreenEdgeOverlap); bit 7 = child (many tests)'),
 (0x005, 1, 'unsigned char', 'paletteIndex', 'T: colour palette chosen at character select (sub_403A90 byte_16224B9 = a4); preserved by InitFighterSlotForRound'),
 (0x006, 1, 'unsigned char', 'pattern', 'T: current pattern (action) id; Actor_ResolveFrameDataPointers indexes charData.patternOffset with it (doc actor+6)'),
 (0x007, 1, 'unsigned char', 'frame', 'T: current frame number inside the pattern (Actor_ResolveFrameDataPointers, ObjRunActionScript)'),
 (0x008, 2, 'unsigned __int16', 'spriteId', 'T: sprite id of the current frame, written by ObjEnterCurrentAction via Pattern_GetFrameSpriteId, read by DrawFighterSprite / PushFighterAfterimage'),
 (0x00A, 1, 'enum Gof1RunState', 'ifRunState', 'T: IF-table run gate: ObjEnterCurrentAction sets 1 (2 stays), sub_4242C0 sets 2 on action change, Actor_TickWordTimers clears it each tick; sub_426BD0/EffectObjects_UpdateAll test it'),
 (0x00B, 1, 'enum Gof1RunState', 'efRunState', 'T: EF-table run gate, same protocol as ifRunState; Actor_RunFrameEFs clears it after running the frame effects'),
 (0x00C, 1, 'unsigned char', 'airReactionActive', 'T: 1 while a launch/air hit-reaction stage (hitStage >= 240) is running: set by Actor_StartKnockback and by ObjRunActionScript on landing, tested/cleared in Actor_ApplyFrameMotionFlags'),
 (0x00D, 1, 'unsigned char', 'unreferenced_0D', 'U: no access in any function'),
 (0x00E, 2, 'unsigned __int16', 'frameTicks', 'T: ticks spent on the current frame, compared with AF.duration (ObjRunActionScript); the grab code copies the grabber value (DrawFighterSprite)'),
 (0x010, 2, 'unsigned __int16', 'lifeTimer', 'T: remaining life of a ghost trail object (EffectObjects_UpdateAll decrements it, sets endConditionMet at 0; sub_413FD0 fades by it)'),
 (0x012, 1, 'unsigned char', 'animationEnded', 'T: set by ObjRunActionScript when the animation reached its last frame (aniFlag 3/4 branch); read by IF 2 END_CONDITION p2 (Actor_RunFrameIfRecord)'),
 (0x013, 1, 'unsigned char', 'unreferenced_13', 'U: no access in any function'),
 (0x014, 1, 'unsigned char', 'loopCounter', 'T: AF.loopCount is loaded here (ObjEnterCurrentAction); IF 9/10 set/test it (Actor_RunFrameIfRecord); aniFlag 2/5 consume it (ObjRunActionScript)'),
 (0x015, 1, 'unsigned char', 'unreferenced_15', 'U: no access in any function'),
 (0x016, 2, 'enum Gof1ObjFlagsA', 'spawnFlagsA', 'T: child flags from EF arg2 bits (InitChildObject). Proven bit meanings, see Gof1ObjFlagsA: EffectObjects_UpdateAll (1, 4, 8, 0x20), ObjIntegrateMotion (0x40), ObjResolvePushboxCollision (8), ChildObject_QueueDraw / Actor_QueueTintedSprite (2); Render_QueueFighterSprite copies it to the draw record, where nothing reads it'),
 (0x018, 2, 'enum Gof1ObjFlagsB', 'spawnFlagsB', 'T: child flags from EF arg3 bits (InitChildObject). Proven bit meanings, see Gof1ObjFlagsB: ChildObject_QueueDraw (1), EffectObjects_UpdateAll (2, 4), Actor_SetHitstopFrames / ObjFighterStateAndHitReaction (8), sub_42D4E0 draw record +0x64 (0x80)'),
 (0x01A, 1, 'enum Gof1HomingMode', 'homingMode', 'T: EF arg4 low byte (InitChildObject); sub_413940 homing object update'),
 (0x01B, 1, 'unsigned char', 'homingBaseAction', 'T: EF subType = first of the homing action family (sub_413940 requests base+1..3 with ObjRequestAction)'),
 (0x01C, 1, 'unsigned char', 'homingReserved_1C', 'U (unproven, neutral name): only written, 0, by InitChildObject next to homingMode/homingBaseAction/homingOffset*; no instruction anywhere reads byte +0x1C of an actor (ctree scan + displacement scan, slot-based +0x20 too). Always 0, so no value to infer a meaning from'),
 (0x01D, 1, 'unsigned char', 'unreferenced_1D', 'U: no access in any function'),
 (0x01E, 2, 'unsigned __int16', 'homingTick', 'T: counts up while homingTimeout != 0 (sub_413940)'),
 (0x020, 2, 'unsigned __int16', 'homingTimeout', 'T: EfType30_SetActorParams sub 0 arg2; sub_413940 ends the homing when homingTick reaches it'),
 (0x022, 2, '__int16', 'homingOffsetX', 'T: EF arg0 / EfType30 arg0; target offset X, 128 = centred (sub_413940 v3 = +-(f022 - 128) << 8)'),
 (0x024, 2, '__int16', 'homingOffsetY', 'T: EF arg1 / EfType30 arg1; target offset Y, 224 = baseline (sub_413940 (f024 - 224) << 8)'),
 (0x026, 2, 'unsigned __int16', 'homingCooldown', 'T: sub_413940 sets 40 after a hit and counts it down'),
 (0x028, 20, '__int16', 'homingParam[10]', 'T: word table written by EfType30_SetActorParams sub 1 (word[40 + 2*arg0] = arg1) and cleared by InitChildObject; sub_413940 reads [0] as the speed'),
 (0x03C, 2, 'unsigned __int16', 'aiActionTimer', 'T: CPU AI: frames left in the running AI script command (FighterCpuAiStep decrements it when not in hit-stop, then calls FighterCpuAi_ApplyScriptCommand)'),
 (0x03E, 2, 'unsigned __int16', 'aiReactCooldown', 'T: CPU AI: frames before the next decision / guard reaction (FighterCpuAiStep loads the difficulty delay v28 or v28 >> 2 and counts it down)'),
 (0x040, 2, '__int16', 'aiScriptIndex', 'T: CPU AI: index of the running AI script, -1 = none (FighterCpuAiStep, FighterCpuAi_ApplyScriptCommand)'),
 (0x042, 2, '__int16', 'aiScriptStep', 'T: CPU AI: step inside the AI script (FighterCpuAiStep increments it, sub_403C10 / ApplyScriptCommand)'),
 (0x044, 1, 'unsigned char', 'cpuCommandReady', 'T: CPU command request latch: IF 11/25 consume it when controlType == CPU (Actor_RunFrameIfRecord); cleared by ObjRunActionScript'),
 (0x045, 3, 'unsigned char', 'unreferenced_45[3]', 'U: no access in any function'),
 (0x048, 4, 'int', 'superMeter', 'T: super meter, 10000 per level, capped 90000 (AddSuperMeter, EfType6 op 4, Actor_SpawnEffectById spends 10000*cost; IF 11 compares f048/10000 with the move cost); preserved across rounds'),
 (0x04C, 1, 'unsigned char', 'superLevelCrossMark', 'T: AddSuperMeter sets it to 10, together with Se_RequestPlay(0x15), when superMeter / 30000 changes (the meter gained a 30000 level boundary). Written ONLY there: nothing decrements, clears or reads it (displacement scan: no byte access to +0x4C or slot +0x50 outside AddSuperMeter), so it is a dead store, not a countdown timer'),
 (0x04D, 3, 'unsigned char', 'unreferenced_4D[3]', 'U: no access in any function'),
 (0x050, 4, 'int', 'reserveGauge', 'T: third gauge 0..10000: EfType6 op 4 adds arg2 and clamps, sub_4252A0 adds the pending combo gauge, sub_424980 clears it at round start; preserved across rounds by InitFighterSlotForRound'),
 (0x054, 4, 'unsigned char', 'unreferenced_54[4]', 'U: no access in any function'),
 (0x058, 4, 'int', 'reservePending', 'T: reserve gauge gain waiting to be banked: sub_4252A0 adds it to reserveGauge (capped 10000) when g_combo_guard_flags[teamIndex] is set and clears it; no writer found in the traced set'),
 (0x05C, 4, 'int', 'life', 'T: hit points, 11400 at round start (InitFighterSlotForRound); Damage_*, ObjFighterStateAndHitReaction, EfType5/6 modify it; <= 0 is KO'),
 (0x060, 4, 'int', 'maxLife', 'T: written with 11400 next to life by InitFighterSlotForRound / FighterSlot_ResetKeepingDataPointers. 11400 (0x2C88) is the life cap: EfType6_ActorOp, Hud_DrawBattleInfo and Battle_RoundStateMachine clamp/compare against the literal 0x2C88, never against this field, so the field is a never-read copy of the cap'),
 (0x064, 2, 'unsigned char', 'unreferenced_64[2]', 'U: no access in any function'),
 (0x066, 1, 'unsigned char', 'superMoveState', 'T: 2 while a super move is active (sub_4242C0 sets it when superMoveFlag; sub_424980 clears it; ObjFighterStateAndHitReaction halves guard damage by the attacker owner state)'),
 (0x067, 1, 'unsigned char', 'superMoveFlag', 'T: armed by sub_426BD0 when a command move is accepted, consumed by sub_4242C0'),
 (0x068, 1, 'unsigned char', 'clearHistoryOnAction', 'T: set from command flag 0x10 (sub_426BD0); sub_4242C0 then clears both input rings with sub_4266D0'),
 (0x069, 3, 'unsigned char', 'unreferenced_69[3]', 'U: no access in any function'),
 (0x06C, 4, 'int', 'guardGauge', 'T: guard gauge, guard break above 10000 (ObjFighterStateAndHitReaction adds AT.guardBreakValue; Actor_DecayGuardGauge decays it in steps of 7..15; DrawFighterSprite flashes above 8000)'),
 (0x070, 4, 'unsigned char', 'unreferenced_70[4]', 'U: no access in any function'),
 (0x074, 4, 'int', 'guardRegenDelay', 'T: frames before the guard gauge starts to decay (45/60/90 set by ObjFighterStateAndHitReaction, 120 by sub_426BD0; Actor_DecayGuardGauge)'),
 (0x078, 4, 'int', 'posX', 'T: world X in 1/128 pixel units (sub_413940, ObjIntegrateMotion, Actor_RunFrameIfRecord, InitChildObject, camera code)'),
 (0x07C, 4, 'int', 'posY', 'T: world Y in 1/128 pixel units, 0 = ground, negative = up (ObjIntegrateMotion lands at > 0)'),
 (0x080, 4, 'unsigned char', 'unreferenced_80[4]', 'U: no access in any function'),
 (0x084, 4, 'int', 'posXAfterIntegrate', 'T: posX as it is when ObjIntegrateMotion returns (written on both exits, after the grab carry). Never read: the readers of the neighbouring posXAtTickStart (+0x88: ObjIntegrateMotion, ObjResolvePushboxCollision, EffectObjects_UpdateAll) are the only per-tick position copy that is used'),
 (0x088, 4, 'int', 'posXAtTickStart', 'T: posX before the tick (ObjIntegrateMotion, sub_413940); EffectObjects_UpdateAll and ObjResolvePushboxCollision read it back'),
 (0x08C, 4, 'int', 'posYAtTickStart', 'T: posY before the tick (ObjIntegrateMotion, sub_413940, EffectObjects_UpdateAll)'),
 (0x090, 4, 'int', 'velX', 'T: velocity X (Actor_ApplyFrameMotionFlags adds AS.velX, ObjIntegrateMotion integrates it)'),
 (0x094, 4, 'int', 'velY', 'T: velocity Y'),
 (0x098, 2, '__int16', 'accelX', 'T: acceleration X (AS.accelX, ObjIntegrateMotion)'),
 (0x09A, 2, '__int16', 'accelY', 'T: acceleration Y (gravity, AS.accelY)'),
 (0x09C, 2, '__int16', 'maxVelX', 'T: X speed clamp (AS.maxVelX, ObjIntegrateMotion)'),
 (0x09E, 2, 'unsigned char', 'unreferenced_9E[2]', 'U: no access in any function'),
 (0x0A0, 4, 'int', 'knockVelX', 'T: knockback velocity X (Actor_StartKnockback, ObjIntegrateMotion adds knockAccelX and zeroes it on sign change)'),
 (0x0A4, 4, 'int', 'knockVelY', 'T: knockback velocity Y'),
 (0x0A8, 2, '__int16', 'knockAccelX', 'T: knockback friction X (g_HitVectorTable, ObjIntegrateMotion)'),
 (0x0AA, 2, '__int16', 'knockAccelY', 'T: knockback gravity Y'),
 (0x0AC, 4, 'int', 'inertiaX', 'T: carried inertia added to posX each tick, decays by inertiaDecay (Actor_ApplyFrameMotionFlags, ObjIntegrateMotion; divided by 3 when entering the air)'),
 (0x0B0, 4, 'int', 'carriedVelX', 'T: velX saved when AS.flags1 bit 0 is set, turned into inertiaX on the next pattern (Actor_ApplyFrameMotionFlags)'),
 (0x0B4, 2, '__int16', 'inertiaDecay', 'T: carriedVelX / 16 (Actor_ApplyFrameMotionFlags); ObjIntegrateMotion subtracts it from inertiaX'),
 (0x0B6, 2, 'unsigned __int16', 'carryPattern', 'T: pattern in which carriedVelX was saved (Actor_ApplyFrameMotionFlags, ObjIntegrateMotion)'),
 (0x0B8, 2, '__int16', 'grabDrawOffsetX0', 'T: start draw offset X of a grabbed actor (EfType4/5 write it, DrawFighterSprite lerps to grabDrawOffsetX1 by the grabber progress, scaled 0.75)'),
 (0x0BA, 2, '__int16', 'grabDrawOffsetY0', 'T: start draw offset Y of a grabbed actor'),
 (0x0BC, 2, '__int16', 'zoomOriginX0', 'T: zoom/rotation origin X at the start of the grab animation, 320 by default (Render_QueueFighterSprite lerps it with zoomOriginX1)'),
 (0x0BE, 2, '__int16', 'zoomOriginY0', 'T: origin Y, 240 by default'),
 (0x0C0, 2, '__int16', 'grabDrawOffsetX1', 'T: end draw offset X of a grabbed actor (EfType4/5, DrawFighterSprite)'),
 (0x0C2, 2, '__int16', 'grabDrawOffsetY1', 'T: end draw offset Y'),
 (0x0C4, 2, '__int16', 'zoomOriginX1', 'T: zoom/rotation origin X at the end, 320 by default'),
 (0x0C6, 2, '__int16', 'zoomOriginY1', 'T: origin Y at the end, 240 by default'),
 (0x0C8, 2, '__int16', 'rotAngleA', 'T: rotation angle, 1/10000 turn: ObjRunActionScript adds 950 per tick while thrown; passed to sub_42FEF0 by Render_QueueFighterSprite; EfType5 sets it from EF arg4'),
 (0x0CA, 2, '__int16', 'rotAngleB', 'T: second rotation angle passed to sub_42FEF0 (set together with rotAngleA)'),
 (0x0CC, 1, 'unsigned char', 'clearInertiaRequest', 'T: set by Actor_StartKnockback; Actor_ApplyFrameMotionFlags then clears inertiaX/inertiaDecay'),
 (0x0CD, 1, 'enum Gof1AttackResultFlags', 'attackResult', 'T: result of this actor attack on its victim (doc actor+205): ObjFighterStateAndHitReaction sets 1/2 and 0x10/0x20 by the victim stance, ObjCheckAttackVsFighters sets 8; reset by ObjEnterCurrentAction; tested by IF 3/6, Actor_RequestLocomotionFromInput'),
 (0x0CE, 1, 'enum Gof1AttackResultFlags', 'ownerAttackResult', 'T: copy of a child attack result written into its owner (ObjFighterStateAndHitReaction f22C->f0CE = f0CD)'),
 (0x0CF, 1, 'unsigned char', 'drawDepth', 'T: draw order key 5..255 (Actor_ApplyDrawPriority; SpawnEffectFromActorFrame; Render_QueueFighterSprite)'),
 (0x0D0, 1, 'unsigned char', 'drawModeVariant', 'T: row offset into g_DrawModeRemapTable while grabbed (EfType4 sets frame/1000; DrawFighterSprite, Box_ApplyDrawModeTransform)'),
 (0x0D1, 1, 'unsigned char', 'activeMoveMeterCost', 'T: meter levels of the running command move (IF 11 copies command+36; Actor_SpawnEffectById spends it, sub_426BD0)'),
 (0x0D2, 1, 'unsigned char', 'superCancelFlag', 'T: sub_426BD0 sets it when a super is started out of a non-actionable frame (v20); ObjRunActionScript stops voice class 10 with sub_42C5B0 and clears it on the next action change'),
 (0x0D3, 1, 'unsigned char', 'hitstopFrames', 'T: remaining hit-stop frames (Actor_SetHitstopFrames; ObjRunActionScript / ObjIntegrateMotion skip while non-zero)'),
 (0x0D4, 1, 'unsigned char', 'hitstopTrailing', 'T: set to 1 by ObjFighterStateAndHitReaction; ObjRunActionScript counts it down with hitstopFrames and clears landed when it runs out'),
 (0x0D5, 1, 'unsigned char', 'contactMark', 'T: set to 1 by ObjCheckAttackVsFighters / ObjCheckAttackVsObjects / ObjCheckReflectBoxVsAttacks when this actor attack box touched something; read by FighterCpuAiStep to chain AI script steps'),
 (0x0D6, 1, 'unsigned char', 'grabState', 'T: 0 free, 1 held by a fighter, 2 held by a child object (Actor_TryGrabFighter); gates ObjRunActionScript, ObjIntegrateMotion, hit detection'),
 (0x0D7, 1, 'unsigned char', 'inputLock', 'T: EfType6 op 9 sets it (doc SET_FLAG_215); blocks input dispatch (Actor_DispatchInputByStance, sub_43A120, sub_426BD0)'),
 (0x0D8, 1, 'unsigned char', 'freezeTimerA', 'T: EfType6 op 2 arg0; Actor_TickFreezeTimers counts it down; also set to 2/8/10 by Actor_ApplyFrameMotionFlags'),
 (0x0D9, 1, 'unsigned char', 'freezeTimerB', 'T: EfType6 op 2 arg1; counted down by Actor_TickFreezeTimers; checked by ObjCheckAttackVs*'),
 (0x0DA, 1, 'unsigned char', 'koFlag', 'T: set at KO by ObjFighterStateAndHitReaction (EfType6 op 252 writes it, doc SET_BYTE_218); tested by sub_424780, ObjResolvePushboxCollision, Camera_UpdateFixedScreen'),
 (0x0DB, 1, 'unsigned char', 'cameraExcludeFlag', 'T: Camera_UpdateFixedScreen skips fighters with it set; no writer in the traced set'),
 (0x0DC, 1, 'unsigned char', 'attackHitsLeft', 'T: hits the current attack may still land (AS.attackHitCount loaded by ObjEnterCurrentAction; ObjCheckAttackVs* decrement; doc actor+220)'),
 (0x0DD, 1, 'unsigned char', 'guardState', 'T: non-zero while guarding (ObjFighterStateAndHitReaction sets 1; DrawFighterSprite flashes when > 4; sub_426BD0 gates guard-cancel moves on > 4)'),
 (0x0DE, 1, 'unsigned char', 'cpuGuardRequest', 'T: written by FighterCpuAiStep (random guard decision by difficulty); ObjFighterStateAndHitReaction treats the hit as guardable when set (a human actor never sets it)'),
 (0x0DF, 3, 'unsigned char', 'unreferenced_DF[3]', 'U: no access in any function'),
 (0x0E2, 1, 'unsigned char', 'recoverMode', 'T: wake-up/recovery selector set by ObjFighterStateAndHitReaction (0, 1, 25), Actor_WallBounceReaction, cleared by EfType4'),
 (0x0E3, 1, 'unsigned char', 'recoverReserved', 'U (unproven, neutral name): only ever written 0, in the canAct reset block of ObjRunActionScript (with recoverMode = 1, throwTechAllowed = 1, throwTechSeen = 0) and in ObjFighterStateAndHitReaction (before throwTechAllowed). Never read, constant 0'),
 (0x0E4, 1, 'unsigned char', 'bounceCount', 'T: wall/ground bounces taken (Actor_WallBounceReaction increments; ObjRunActionScript wraps > 2 to -16)'),
 (0x0E5, 1, 'unsigned char', 'recoveryTimer', 'T: frames since the last hit (ObjRunActionScript increments, ObjFighterStateAndHitReaction resets; Actor_TryTechOrCrouchGuardSwitch window < 16)'),
 (0x0E6, 1, 'unsigned char', 'throwTechSeen', 'T: set on the grabber by sub_425E10 when a throw tech succeeds; ComboRecord_RegisterHit flags the combo'),
 (0x0E7, 1, 'unsigned char', 'stageEdgeSide', 'T: 1/2 = pushed past the right/left stage edge by sub_425A50; sub_424780 / ObjResolvePushboxCollision use it for wall bounce'),
 (0x0E8, 1, 'unsigned char', 'airComboStarterLatch', 'T: set by ObjFighterStateAndHitReaction when AT.flagsB 0x10 hit lands, cleared by ObjRunActionScript / Actor_StartKnockback test'),
 (0x0E9, 1, 'unsigned char', 'throwTechAllowed', 'T: ObjFighterStateAndHitReaction sets !(AT.flagsB & 0x20) ...; sub_425E10 requires it'),
 (0x0EA, 1, 'unsigned char', 'inputBlockFlag', 'T: sub_43A120 refuses input while set; cleared by ObjRunActionScript; no writer in the traced set'),
 (0x0EB, 1, 'unsigned char', 'counterHitSide', 'T: 1 = this actor landed a counter-hit (set on the attacker owner), 2 = this actor was counter-hit (set on the victim) by ObjFighterStateAndHitReaction; Actor_StartKnockback and sub_4242C0 clear it; sub_426BD0 (counter-hit cancels, evade moves) and Actor_RequestLocomotionFromInput read it'),
 (0x0EC, 10, 'unsigned __int16', 'scriptTimer[5]', 'T: five countdown words (Actor_TickWordTimers); EfType6 op 10 (doc SET_WORD_VAR_236) sets [arg0]; [0] = hyper armor window and [2] = counter-hit window are read by ObjFighterStateAndHitReaction'),
 (0x0F6, 1, 'unsigned char', 'hitQueueClear', 'T: ObjCheckAttackVs* set 1 to discard last tick hit queue; ObjFighterStateAndHitReaction zeroes hitQueueCount when set'),
 (0x0F7, 1, 'unsigned char', 'hitQueueCount', 'T: number of valid hitAt/hitRect/hitAttacker entries, max 8 (ObjCheckAttackVsFighters, ObjCheckAttackVsObjects, Actor_TryGrabFighter)'),
 (0x0F8, 1, 'enum Gof1StatusEffect', 'tintMode', 'T: AT.statusEffect written by ObjFighterStateAndHitReaction; DrawFighterSprite tints by it (1 red pulse, 2 random shimmer, 3 yellow/black/grey)'),
 (0x0F9, 1, 'unsigned char', 'shakeTimer', 'T: draw shake after a hit (DrawFighterSprite offsets Y by f0F9>>2 on odd ticks); counted down by Actor_TickFreezeTimers'),
 (0x0FA, 2, '__int16', 'hitStage', 'T: 0 = not in hit-stun; 1..239 = remaining stun frames; 240..255 = launch/air stages dispatched by Actor_ApplyFrameMotionFlags (byte_425734); set from g_HitVectorTable by Actor_StartKnockback'),
 (0x0FC, 2, '__int16', 'juggleState', 'T: non-zero while the victim is in an air juggle (halves damage in Damage_ScaleByComboAndLife; sub_424780 wall bounce rule); cleared by Actor_TryGrabFighter'),
 (0x0FE, 2, 'unsigned __int16', 'hitReactionId', 'T: reaction index passed to Actor_StartKnockback (g_HitVectorTable row); sub_424780 / Actor_WallBounceReaction compare 9, 13, 27'),
 (0x100, 2, '__int16', 'hitStageAtHit', 'T: copy of hitStage taken by Actor_StartKnockback (read by sub_426BD0 as < 240)'),
 (0x102, 1, 'unsigned char', 'hitRecoverFrames', 'T: recovery frames after a hit, computed by ObjFighterStateAndHitReaction from hitStage and hitsTakenInCombo, cleared by EfType4_ControlGrabbedOpponent; no reader in the traced set'),
 (0x103, 1, 'unsigned char', 'unreferenced_103', 'U: no access in any function'),
 (0x104, 1, 'unsigned char', 'invulnFlags', 'T: non-zero = takes no damage (ObjFighterStateAndHitReaction, sub_43A120); EfType6 op 255 sets 0x10 (doc SET_FLAG_260)'),
 (0x105, 1, 'unsigned char', 'scriptFlag105', 'T: EfType6 op 254 sets 1 (doc SET_FLAG_261); cleared by Battle_InitRoundFighters / Round_IntroStateMachine; no reader in the traced set'),
 (0x106, 1, 'unsigned char', 'koMoveType', 'T: (attacker move type & 0x7F) + 1 when the actor is KO by it (ObjFighterStateAndHitReaction); non-zero blocks recovery (Actor_StartKnockback, sub_426BD0)'),
 (0x107, 1, 'unsigned char', 'inputHeldFlags', 'T: Actor_DispatchInputByStance sets bit 7 when the direction is neutral and clears it for crouch/air; ObjRunActionScript clears it; no reader in the traced set'),
 (0x108, 1, 'unsigned char', 'inputReserved_108', 'U (unproven, neutral name): only ever written 0, in the canAct reset block of ObjRunActionScript (between throwTechSeen and guardRecoveryFlag). Never read, constant 0; +0x10C.. is counter[] and is indexed from 0x10C, so no indexed access reaches +0x108'),
 (0x109, 1, 'unsigned char', 'guardRecoveryFlag', 'T: 1 after a guard-cancel/early guard (Actor_StartKnockback special case), read by Actor_CanGuardAttack and Actor_TryTechOrCrouchGuardSwitch'),
 (0x10A, 2, 'unsigned __int16', 'mashCounter', 'T: counts button presses while in hit-stun (Actor_TryTechOrCrouchGuardSwitch); Damage_ScaleByComboAndLife reduces damage by 0.3% each; sub_426BD0 tests it'),
 (0x10C, 10, 'signed char', 'tobiCount[10]', 'T: ten projectile ("TOBI") counters shown by Debug_DrawEntityInfo as "TOBI %03d"; EfType6 op 100/101 add/subtract (arg0 tens digit = index, units = amount), sub_426BD0 refuses a command when the counter is too low'),
 (0x116, 1, 'unsigned char', 'dashCount', 'T: shown by Debug_DrawEntityInfo as "DASH %03d"; EfType6 op 102/103 add/subtract (doc ADD_BYTE_278); cleared by ObjRunActionScript'),
 (0x117, 1, 'unsigned char', 'unreferenced_117', 'U: no access in any function'),
 (0x118, 20, '__int16', 'paramVar[10]', 'T: ten word variables shown by Debug_DrawEntityInfo as "PARAM %05d"; EfType6 op 105 sets/adds [arg0]; IF 25/31 compare/write them (Actor_RunFrameIfRecord); Actor_SpawnEffectById reads [1],[2]; SpawnCommonEffectObject copies a 20-byte defaults block here'),
 (0x12C, 1, 'unsigned char', 'trailMode', 'T: ghost trail colour mode + 1 (EfType6 op 0; DrawFighterSprite spawns SpawnEffectFromActorFrame every 7th tick); on a ghost object (objKind 31) it is the colour mode read by sub_413FD0'),
 (0x12D, 1, 'unsigned char', 'unreferenced_12D', 'U: no access in any function'),
 (0x12E, 2, 'unsigned __int16', 'decorLifeTotal', 'T: ghost object total life (SpawnEffectFromActorFrame a5; sub_413FD0 fades alpha by lifeTimer / this)'),
 (0x130, 2, 'enum Gof1DecorFlags', 'decorFlags', 'T: ghost object flags (SpawnEffectFromActorFrame a6; sub_413FD0 adds 10 to the depth when bit 0)'),
 (0x132, 2, '__int16', 'decorZoomGrowX', 'T: ghost zoom change per remaining tick X (SpawnEffectFromActorFrame a3; sub_413FD0)'),
 (0x134, 2, '__int16', 'decorZoomGrowY', 'T: ghost zoom change per remaining tick Y (a4)'),
 (0x136, 1, 'unsigned char', 'afterimageMode', 'T: afterimage config id + 1 (EfType6 op 3); DrawFighterAfterimage switches on it, 0 = off'),
 (0x137, 1, 'unsigned char', 'afterimageSteps', 'T: number of afterimage steps <= 119 (EfType6 op 3 arg1; DrawFighterAfterimage)'),
 (0x138, 1, 'unsigned char', 'afterimageSpacing', 'T: spacing of the steps (EfType6 op 3 arg2; DrawFighterAfterimage)'),
 (0x139, 1, 'unsigned char', 'afterimageRestart', 'T: EfType6_ActorOp op 3 sets 1 whenever the requested afterimage mode differs from afterimageMode (or it was 0). Written ONLY there: never cleared and never read (DrawFighterAfterimage / PushFighterAfterimage do not touch it), so it is a dead store'),
 (0x13A, 1, 'unsigned char', 'justGuardActive', 'T: set when a just-guard (button bit 7 and CT flag 8) absorbs a hit (ObjFighterStateAndHitReaction); Actor_StartKnockback then pushes the attacker back instead'),
 (0x13B, 1, 'unsigned char', 'unreferenced_13B', 'U: no access in any function'),
 (0x13C, 2, 'unsigned __int16', 'locomotionUsedMask', 'T: bit n = pattern n (1..9 walk/run/dash/jump) used since the last landing (ObjRunActionScript); Actor_RequestLocomotionFromInput tests bits 1,2,4,0x10,0x20,0x80,0x100'),
 (0x13E, 14, 'unsigned char', 'commandUsedMask[14]', 'T: bit i = command move i already used in this chain (ObjRunActionScript sets it, sub_426BD0 refuses repeats, sub_43BE80 counts the bits)'),
 (0x14C, 1, 'unsigned char', 'pendingCommandId', 'T: command move accepted by sub_426BD0 this tick (0xFF = none); ObjRunActionScript moves it into commandUsedMask'),
 (0x14D, 1, 'unsigned char', 'startedByCommandId', 'T: id of the command move (Gof1CommandMove.commandId) that requested the action just entered: ObjRunActionScript copies pendingCommandId here when it switches to pendingPattern (0xFF when the pending action was not requested by a command move). Written ONLY there, never read'),
 (0x14E, 2, '__int16', 'hitsTakenInCombo', 'T: consecutive hits taken (ObjFighterStateAndHitReaction increments; ObjRunActionScript indexes byte_463FE0 with it to time recovery)'),
 (0x150, 2, 'unsigned __int16', 'attackHitsConnected', 'T: hits landed by the current action (ObjCheckAttackVsFighters/Objects increment; IF 17 compares with p1; sub_4242C0 clears)'),
 (0x152, 1, 'unsigned char', 'evadeDir', 'T: 1 forward / 2 back double-tap evade detected by sub_427040; ObjFighterStateAndHitReaction picks the evade-high/low answer from it'),
 (0x153, 1, 'unsigned char', 'evadeTimer', 'T: evade window counter (sub_427040 loads 8, then -8 cooldown; ObjFighterStateAndHitReaction)'),
 (0x154, 2, 'unsigned char', 'unreferenced_154[2]', 'U: no access in any function'),
 (0x156, 1, 'enum Gof1FlashMode', 'flashMode', 'T: colour flash mode (Actor_SetColorFlash; DrawFighterSprite / sub_413FD0 switch)'),
 (0x157, 1, 'unsigned char', 'flashTimer', 'T: remaining flash frames (Actor_TickFreezeTimers decrements)'),
 (0x158, 1, 'unsigned char', 'flashDuration', 'T: total flash frames, denominator of the fade modes'),
 (0x159, 1, 'unsigned char', 'requestParam', 'T: argument a5 of ObjRequestAction, cleared by ObjRunActionScript, set from command byte +38 by sub_426BD0; no reader in the traced set'),
 (0x15A, 1, 'enum Gof1CounterCommand', 'counterWindowState', 'T: counter-hit window set by frame flags2 bits 20..23 (ObjEnterCurrentAction); ObjFighterStateAndHitReaction reads it to apply the counter bonus'),
 (0x15B, 1, 'unsigned char', 'unreferenced_15B', 'U: no access in any function'),
 (0x15C, 2, '__int16', 'launchVelX', 'T: stage-2 launch velocity X loaded when hitStage advances (Actor_StartKnockback fills it from g_HitVectorTable, Actor_ApplyFrameMotionFlags consumes it)'),
 (0x15E, 2, '__int16', 'launchVelY', 'T: stage-2 launch velocity Y'),
 (0x160, 2, '__int16', 'launchAccelX', 'T: stage-2 acceleration X'),
 (0x162, 2, '__int16', 'launchAccelY', 'T: stage-2 acceleration Y'),
 (0x164, 32, 'struct Gof1AtRecord *', 'hitAt[8]', 'T: AT record of each hit received this tick (ObjCheckAttackVsFighters writes *(a+4*count+356); ObjFighterStateAndHitReaction reads)'),
 (0x184, 128, 'struct Gof1RectI32', 'hitRect[8]', 'T: world rectangle of each overlap (ObjCheckAttackVsFighters writes 16 bytes at +388+16*count; used to place the hit spark)'),
 (0x204, 32, 'struct Gof1Actor *', 'hitAttacker[8]', 'T: attacking actor of each hit (ObjCheckAttackVsFighters writes *(a+4*count+516))'),
 (0x224, 4, 'struct Gof1Actor *', 'lastAttacker', 'T: attacker of the last accepted hit (ObjFighterStateAndHitReaction); Actor_StartKnockback applies the push to it on a just-guard; ObjResolvePushboxCollision'),
 (0x228, 4, 'struct Gof1Actor *', 'grabbedBy', 'T: grabber while grabState != 0 (Actor_TryGrabFighter victim->f228 = grabber; ObjIntegrateMotion drags the grabber position along)'),
 (0x22C, 4, 'struct Gof1Actor *', 'ownerFighter', 'T: owning fighter of a child object (InitChildObject via sub_424560, SpawnChildEffectFromParent); homing target in sub_413940; 0 for fighters'),
 (0x230, 4, 'struct Gof1Actor *', 'linkedActor', 'T: parent actor of a child (InitChildObject, SpawnChildEffectFromParent; read by EffectObjects_UpdateAll, Actor_SetHitstopFrames, DrawFighterSprite) / grab victim of a fighter (Actor_TryGrabFighter and sub_425E10 write it)'),
 (0x234, 1, 'enum Gof1InputDirection', 'inputDir', 'T: numpad direction facing-corrected (ReadBattleInputForPlayer; mirrored by Actor_MirrorInputDirection via dword_463FB8; recorded by ReplayRecordOrPlaybackFighterInput)'),
 (0x235, 3, 'unsigned char', 'unreferenced_235[3]', 'U: no access in any function'),
 (0x238, 4, 'enum Gof1InputButtonFlags', 'inputButtons', 'T: buttons held (bits 0..5) and pressed this tick (bits 12..17) (ReadBattleInputForPlayer a3; replay recorder; Actor_RequestLocomotionFromInput, sub_425E10 tests & 0xF)'),
 (0x23C, 4, 'enum Gof1InputButtonFlags', 'inputButtonsReleased', 'T: buttons released this tick (bits 0..5) (ReadBattleInputForPlayer a4); input ring B ORs it in (UpdateFighterInputHistoryRings)'),
 (0x240, 1, 'unsigned char', 'teamIndex', 'T: side/team 0 or 1 (InitFighterSlotForRound a2; children copy it); same-team test in ObjCheckAttackVsFighters, indexes per-team stats (dword_1552F70[15*i])'),
 (0x241, 1, 'unsigned char', 'endConditionMet', 'T: set by IF 2 END_CONDITION and by EffectObjects_UpdateAll (doc actor+577); the object is removed on the next tick'),
 (0x242, 2, 'unsigned __int16', 'pendingPattern', 'T: pattern requested by ObjRequestAction, 0xFFFF = none (ObjRunActionScript consumes it)'),
 (0x244, 2, '__int16', 'pendingPatternPriority', 'T: priority of the pending request, -1 = none; a request wins when its priority is higher (ObjRequestAction)'),
 (0x246, 2, 'unsigned __int16', 'pendingFrame', 'T: frame requested by sub_424510 / IF jumps, 0xFFFF = none'),
 (0x248, 2, '__int16', 'pendingFramePriority', 'T: priority of the pending frame jump, -1 = none'),
 (0x24A, 1, 'unsigned char', 'facingLeft', 'T: 0 faces right, 1 faces left (read by ~70 functions: sub_413940, DrawFighterSprite, Actor_ApplyFrameMotionFlags, InitChildObject)'),
 (0x24B, 1, 'unsigned char', 'nextFacingLeft', 'T: facing to turn to when the action changes (ObjRunActionScript copies it into facingLeft; Actor_WallBounceReaction, EfType6 op 5, sub_413940)'),
 (0x24C, 1, 'unsigned char', 'facingReserved_24C', 'U (unproven, neutral name): only written, 0xFF, by InitFighterSlotForRound / FighterSlot_ResetKeepingDataPointers, the second of which writes nextFacingLeft = 0xFF (an unset sentinel) right before it. Never read'),
 (0x24D, 1, 'unsigned char', 'mirrorHistoryPending', 'T: read by UpdateFighterInputHistoryRings and sub_426910: when the facing changed and it is 1 the recorded directions are mirrored (dword_463FB8) and it is cleared'),
 (0x24E, 1, 'unsigned char', 'inputActionLatch', 'T: set 1 by IF 6/7 input jumps with flag 4 (doc actor+590); ObjRunActionScript steps it 1 -> 2 -> 0; EffectObjects_UpdateAll kills children when the parent shows 2'),
 (0x24F, 1, 'unsigned char', 'actionChangedFlag', 'T: sub_4242C0 sets 1 on every action change, Actor_TickWordTimers clears it; children with spawnFlagsA bit 0x20 die when the parent shows 1'),
 (0x250, 1, 'unsigned char', 'landed', 'T: 1 when posY reached the ground (ObjIntegrateMotion; doc actor+592): drives aniFlag 3/4 landing jumps, IF 2 p1, Actor_ApplyFrameMotionFlags'),
 (0x251, 1, 'unsigned char', 'actionEnded', 'T: ObjRunActionScript sets it when a landing/animation end finished the action; EffectObjects_UpdateAll ends children whose parent shows it'),
 (0x252, 1, 'enum Gof1Stance', 'stanceCopy', 'T: AS.stance of the previous tick (ObjRunActionScript); ObjIntegrateMotion divides inertiaX by 3 on a ground-to-air change'),
 (0x253, 1, 'unsigned char', 'unreferenced_253', 'U: no access in any function'),
 (0x254, 4, 'struct Gof1Box *', 'boxTable', 'T: pattern + boxTableOffset or 0 (Actor_ResolveFrameDataPointers; doc actor+596)'),
 (0x258, 4, 'struct Gof1CharDatHead *', 'charData', 'T: character pattern block (Slot_BindCharacterData from g_CharPatternData; Pattern_Get* read patternOffset[] through it)'),
 (0x25C, 4, 'struct Gof1PatternHeader *', 'patternHeader', 'T: current pattern header (Actor_ResolveFrameDataPointers; Actor_GetPatternFrameRecords)'),
 (0x260, 4, 'struct Gof1FrameRecord *', 'frameRecord', 'T: current 116-byte frame record (Actor_ResolveFrameDataPointers; ObjCheckAttackVsFighters reads atIndex at +80)'),
 (0x264, 4, 'struct Gof1AnimFrame *', 'animFrame', 'T: same address as frameRecord, typed as the AF half (DrawFighterSprite, ObjRunActionScript)'),
 (0x268, 4, 'struct Gof1StateFrame *', 'stateFrame', 'T: frameRecord + 40, the AS half (ObjIntegrateMotion, Actor_ApplyFrameMotionFlags)'),
 (0x26C, 4, 'struct Gof1IfRecord *', 'ifTable', 'T: pattern + ifTableOffset or 0 (ObjRunFrameEventsForPhase)'),
 (0x270, 4, 'struct Gof1EfRecord *', 'efTable', 'T: pattern + efTableOffset or 0 (Actor_RunFrameEFs)'),
 (0x274, 4, '__int16 *', 'boxIndex', 'T: frameRecord + 96, the 10 box indices (Actor_FindOverlappingTarget, ObjCheckAttackVs*)'),
 (0x278, 4, 'void *', 'partsData', 'T: character parts blob (Slot_BindCharacterData from g_CharPartsData; Render_QueueFighterSprite)'),
 (0x27C, 4, 'void *', 'cgData', 'T: character CG blob (Slot_BindCharacterData from g_CharCgData; Render_QueueFighterSprite)'),
 (0x280, 4, 'struct Gof1FighterSlot *', 'fighterSlot', 'T: pointer to the owning fighter slot (InitFighterSlotForRound stores the slot itself); 0 for child objects; gates AddSuperMeter and gives access to the CT data'),
 (0x284, 4, 'unsigned char', 'unreferenced_284[4]', 'U: no access in any function'),
 (0x288, 4, 'int', 'actionTicks', 'T: ticks since the action started (ObjRunActionScript increments, sub_4242C0 clears; DrawFighterSprite flickers on % 3 / % 7)'),
 (0x28C, 1, 'unsigned char', 'ranThisTick', 'T: ObjRunActionScript sets 1 once it passed all gates, 0 at its start; Battle_UpdateAllCharactersInputAndAI, hit detection and DrawFighterSprite test it'),
 (0x28D, 3, 'unsigned char', 'unreferenced_28D[3]', 'U: no access in any function'),
]

CMD = [  # 46 bytes
 (0, 1, 'unsigned char', 'commandId', 'T: own index, 0xFF = unused slot (sub_426FC0, sub_426BD0 test it)'),
 (1, 1, 'unsigned char', 'unreferenced_01', 'U: zero in every record of all 8 characters, never accessed'),
 (2, 32, 'unsigned char', 'sequence[32]', 'T: input token string 0xFF terminated: 0..9 direction, 0x41..0x46 buttons A..F, 0x2B + joiner, 0x54 T hold; parsed backwards from the last token by sub_4266F0 / sub_4267F0 (sub_426910). Bytes 16..31 are cleared by InitFighterSlotForRound'),
 (34, 1, 'unsigned char', 'targetPattern', 'T: pattern requested when the command fires (sub_426BD0 -> ObjRequestAction; compared with the current pattern)'),
 (35, 1, 'enum Gof1CommandMoveClass', 'moveClass', 'T: selects the cancel table in Fighter_TryStartCommandMove: 0 uses stateFrame.normalCancel, 1 uses stateFrame.specialCancel, 2 uses specialCancel plus the super-cancel window (g_PlayerSuperCancelWindow, sets superCancelFlag) and the stateFrame.flags2 bit 0 hit gate; class 0 is also the only class subject to the CT flag-1 repeat restriction (commandUsedMask) and the move-level test < 100. Data: 0 = dashes, command normals, script-only helpers; 1 = specials/throws; 2 = supers (cost 3)'),
 (36, 2, 'unsigned __int16', 'meterCostLevels', 'T: super meter levels needed (compared with superMeter / 10000 in IF 11 and sub_426BD0; low byte -> activeMoveMeterCost)'),
 (38, 1, 'unsigned char', 'requestParam', 'T: stored in actor requestParam (sub_426BD0)'),
 (39, 1, 'unsigned char', 'unreferenced_27', 'U: zero in every record of all 8 characters, never accessed'),
 (40, 2, 'unsigned __int16', 'counterRequirement', 'T: tens digit = counter[] index and units = amount (< 100), or 100+ = global counter bank dword_1864EA0 (sub_426BD0 refuses when not enough)'),
 (42, 2, 'unsigned char', 'unreferenced_2A[2]', 'U: zero in every record of all 8 characters, never accessed'),
 (44, 1, 'enum Gof1CommandFlags', 'flags', 'T: see Gof1CommandFlags (all bits read in Fighter_TryStartCommandMove; bit 0x20 by sub_426FC0)'),
 (45, 1, 'enum Gof1CommandFlags2', 'flags2', 'T: see Gof1CommandFlags2 (sub_426910 ring select / strictness, sub_426BD0 evade moves)'),
]
CTH = [  # 28 bytes
 (0, 1, 'unsigned char', 'unreferenced_00', 'U: zero in all 8 CT files, never accessed'),
 (1, 1, 'unsigned char', 'unreferenced_01', 'U: 9 in all 8 CT files, never accessed'),
 (2, 1, 'unsigned char', 'recoveryStyle', 'T: 1 in all files; ObjRunActionScript (0x424F23) switches the hit recovery handling on it'),
 (3, 1, 'unsigned char', 'unreferenced_03', 'U: zero in all 8 CT files, never accessed'),
 (4, 1, 'unsigned char', 'unreferenced_04', 'U: zero in all 8 CT files; sub_426BD0 reads the word at +4 but only tests bit 8 (= flags bit 0)'),
 (5, 1, 'enum Gof1CtFlags', 'flags', 'T: 0x39 / 0x3B in the data; ObjRunActionScript (bit 0), sub_426BD0 (bit 0), ObjFighterStateAndHitReaction (bits 1, 3)'),
 (6, 2, 'unsigned char', 'unreferenced_06[2]', 'U: zero in all 8 CT files, never accessed'),
 (8, 4, 'float', 'knockbackScale', 'T: 1.0 in all files; Actor_StartKnockback multiplies the vertical knockback (0 means 1.0)'),
 (12, 16, 'float', 'damageScaleByLifeTier[4]', 'T: 1.1 (AKIKO) / 0.9 (others) in all files; Damage_ScaleByComboAndLife picks it by the victim life tier (2850 / 5700 / 8550)'),
]

HITVEC = [  # 18 bytes
 (0, 2, '__int16', 'velX', 'T: knockback velocity X, multiplied by the attack direction sign (Actor_StartKnockback -> actor.knockVelX); vector.txt column 0'),
 (2, 2, '__int16', 'velY', 'T: knockback velocity Y, scaled by the combo decay factor (-> actor.knockVelY); column 1'),
 (4, 2, '__int16', 'accelX', 'T: knockback friction X, signed by direction (-> actor.knockAccelX); column 2'),
 (6, 2, '__int16', 'accelY', 'T: knockback gravity Y (-> actor.knockAccelY); column 3'),
 (8, 2, '__int16', 'launchVelX', 'T: velocity X of the launch stage reached when hitStage >= 240 (-> actor.launchVelX); column 4'),
 (10, 2, '__int16', 'launchVelY', 'T: velocity Y of the launch stage (-> actor.launchVelY); column 5'),
 (12, 2, '__int16', 'launchAccelX', 'T: acceleration X of the launch stage (-> actor.launchAccelX); column 6'),
 (14, 2, '__int16', 'launchAccelY', 'T: acceleration Y of the launch stage (-> actor.launchAccelY); column 7'),
 (16, 2, '__int16', 'hitStage', 'T: stun frames (< 240) or launch stage code 240..255 (-> actor.hitStage); column 8; 255 = air launch, 248 = ground slide'),
]

def emit_struct(name, size, rows, cmt, align_ok=True):
    out = ['struct %s {  // %s' % (name, cmt)]
    pos = 0
    for off, sz, ty, nm, c in sorted(rows, key=lambda r: r[0]):
        if off < pos: sys.exit('%s: overlap at 0x%X (%s) pos 0x%X' % (name, off, nm, pos))
        if off > pos: sys.exit('%s: UNMAPPED bytes 0x%X..0x%X before %s' % (name, pos, off, nm))
        base, _, cnt = nm.partition('[')
        decl = '%s %s%s' % (ty, base, ('[' + cnt) if cnt else '')
        # a pointer type already ends with '*': keep "type *name"
        decl = decl.replace('* ', '*') if ty.endswith('*') else decl
        out.append(' %s; // +0x%03X %s' % (ty + ('' if ty.endswith('*') else ' ') + base + (('[' + cnt) if cnt else ''), off, c))
        pos = off + sz
    if pos != size: sys.exit('%s: ends at 0x%X, expected 0x%X' % (name, pos, size))
    out.append('};')
    return out

out = ['// GOF1 (gof.exe) runtime actor + fighter slot + CT file types, IDA-parsable (idc.parse_decls). GENERATED by tools/ida/make_gof1_actor.py - edit the generator, not this file.',
       '// Evidence: T = access traced in gof.exe (function names are IDB names), I = inferred, U = never accessed by any function (zero-filled at spawn). Zero unmapped bytes: the generator aborts on any gap/overlap.',
       '// Uses the record types of gof1_types.h (Gof1FrameRecord pieces, Gof1AtRecord, Gof1Box, Gof1RectI32, Gof1IfRecord, Gof1EfRecord, Gof1PatternHeader, Gof1CharDatHead, Gof1Stance, Gof1StatusEffect, Gof1CounterCommand).']
out += ENUMS.strip().split('\n')
out += ['struct Gof1Actor;', 'struct Gof1FighterSlot;']
out += emit_struct('Gof1Actor', 0x290, ACTOR, '656 bytes; fighters live in Gof1FighterSlot.actor, spawned objects in Gof1ObjectSlot.actor (memset 0x290 by InitChildObject / SpawnEffectFromActorFrame)')
out += emit_struct('Gof1HitVectorRow', 18, HITVEC, '18 bytes, row r of g_HitVectorTable[32] (0x17F9208), loaded at start-up from the first section of VECTOR.TXT (HitVectors_LoadFromVectorTxt 0x42C1D0, 9 whitespace separated integers per row, rows end at END); row = reaction id passed to Actor_StartKnockback')
out += emit_struct('Gof1CommandMove', 46, CMD, '46 bytes, record i of the CT file / Gof1FighterSlot.command[i]')
out += emit_struct('Gof1CtHeader', 28, CTH, '28 bytes at CT file +4604 / Gof1FighterSlot.ct')
out += ['struct Gof1CtFile {  // 4632 bytes: the .CT file (<CHAR>_C.CT in gof_03.p) Character_LoadCtFile 0x4266A0 splits into slot.commandCount, slot.command[100] and slot.ct',
        ' unsigned int commandCount; // +0x000 T: number of defined commands (Character_LoadCtFile a1[172]); the loop in InitFighterSlotForRound runs to it',
        ' struct Gof1CommandMove command[100]; // +0x004 T: command table copied (0x11F8 bytes) to slot+692',
        ' struct Gof1CtHeader ct; // +0x11FC T: 28 bytes copied to slot+660',
        '};']
out += ['struct Gof1InputRing {  // 512 bytes; 64 entries newest first, shifted by UpdateFighterInputHistoryRings',
        ' unsigned int state[64]; // +0x000 T: (direction << 24) | buttons for each distinct input state (ring A: inputButtons, ring B: inputButtons | inputButtonsReleased); matched backwards by sub_426910',
        ' unsigned int age[64]; // +0x100 T: frames spent in state[i] (capped at 1000000; sub_4266D0 sets 1000000 to invalidate the history)',
        '};']
SLOT = [
 (0, 1, 'unsigned char', 'active', 'T: 1 = slot in use (Slot_BindCharacterData, InitFighterSlotForRound set 1, FighterSlot_ResetKeepingDataPointers 0; all loops test it)'),
 (1, 3, 'unsigned char', 'unreferenced_01[3]', 'U: no access in any function (pool stride 660 = 1 + 3 + 656)'),
]
FIGHTER = SLOT + [
 (4, 656, 'struct Gof1Actor', 'actor', 'T: the actor (actor = slot + 4; memset 0x290 at InitFighterSlotForRound)'),
 (660, 28, 'struct Gof1CtHeader', 'ct', 'T: CT header copied by Character_LoadCtFile (Actor_StartKnockback reads +8, Damage_ScaleByComboAndLife +12, ObjRunActionScript +2/+5)'),
 (688, 4, 'int', 'commandCount', 'T: Character_LoadCtFile a1[172]; InitFighterSlotForRound clears the runtime tail of each of these commands'),
 (692, 4600, 'struct Gof1CommandMove', 'command[100]', 'T: command-move table (sub_426FC0 scans all 100; IF 11 indexes it)'),
 (5292, 512, 'struct Gof1InputRing', 'ringA', 'T: input history A (UpdateFighterInputHistoryRings at +5292/+5548)'),
 (5804, 512, 'struct Gof1InputRing', 'ringB', 'T: input history B with releases (+5804/+6060)'),
]
out += emit_struct('Gof1FighterSlot', 6316, FIGHTER, '6316 bytes; g_PlayerSlots[4] at 0x16224B0 (array ends exactly at g_DemoAttractMode 0x1628760)')
out += emit_struct('Gof1ObjectSlot', 660, SLOT + [(4, 656, 'struct Gof1Actor', 'actor', 'T: the actor (InitChildObject: memset(slot + 4, 0, 0x290))')], '660 bytes; g_EffectPool[1000] at 0x1628770 (660000 bytes, ends at 0x16C9990)')
print('\n'.join(out))
n = sum(r[1] for r in ACTOR if r[3].startswith('unreferenced_'))
sys.stderr.write('Gof1Actor 656 bytes mapped, of which %d bytes are explicit unreferenced_* fields; unmapped = 0\n' % n)
