# RboActor part 2: bytes 0x200..0x4FF (decimal 512..1279). Same tuple format as table F of make_rbo_actor.py.
# Confidence tags: [traced] = read the code that uses it, [inferred] = from data/usage pattern, [unused] = no actor-pointer access found.
# Existing named fields inside the range are NOT redefined: drawDepthOffset (692), drawLayer (696), blendMode (744).
# MERGE NOTE: motionSlot460 starts at 460 (0x1CC, part 1 range) and repeatTracker ends at 1311 (0x51F, part 3 range): they are single
# structs, so the owner of the neighbouring range must drop its own fields for those bytes. Types: actor_part2_types.h
FIELDS = [
 (460, 76, 'struct RboP2MotionSlot', 'motionSlot460', '[traced] 5th motion slot (counter at +456); Actor_Init 0x441A90 Mover_InitLimitedAccel; stepped by Actor_StepMotionWithTimedSlot 0x4404F0 (fn at +472). Bytes 0x200..0x217 are its tail'),
 (536, 76, 'struct RboP2MotionSlot', 'knockbackMotion', '[traced] Velocity_Init(actor+536) in Actor_StartKnockbackMotion 0x444CE0 (script BadCndAnimeStopVData); stepped in sub_440550 via fn at +548 while state mask 0x1E0 set'),
 (612, 52, 'struct RboP2AutoMove', 'autoMove', '[traced] walk-to-target state: Actor_StepAutoMove 0x44BBD0, Actor_SetAutoMoveTargetX 0x4418B0, Actor_MarkKilledAndCleanup 0x4419A0, script op 7 in sub_41C710'),
 (664, 4, 'int', 'cancelMoveCount', '[inferred] ++ in sub_43DD80 (starts pattern 38/39), cleared by sub_4414D0/sub_43EEC0, >0 blocks Actor_NormalCancelAllowed via sub_43DAA0'),
 (668, 16, 'struct RboP2DrawOffsetKeys', 'drawOffsetKeys', '[traced] read by Actor_GetBoxOrigin 0x440B50, Actor_GetDrawOffsetLerp 0x445C20, Actor_DrawCgSprite 0x4470E0 when drawOffsetMode != 0; no direct writer found (script/memcpy)'),
 (684, 4, 'unsigned char', 'unused_2AC[4]', '[unused] no [reg+0x2AC] access in any actor function; Actor_Init does not set it'),
 (688, 4, 'RboActor *', 'depthParent', '[traced] Actor_SumParentDepthOffsets 0x442330 walks it; spawner sub_41ACF0 sets it to the owner (+16) for relative-depth mode 0x40000'),
 (700, 4, 'int', 'fadeOutTicks', '[traced] sub_440550: while non-zero actor ticks every frame; reaching 0 with flag 0x100 of +2176 sets +36 (kill). Set by sub_43F2A0 (fade-out start)'),
 (704, 4, 'int', 'hitStopTicks', '[traced] set by Actor_SetHitStop 0x419950 (hit-event ops case 0/1 in sub_41EED0); while >0 sub_440550 decrements it and skips the anim clock'),
 (708, 4, 'int', 'flowRecoverCountdown', '[traced] Actor_TickFrame 0x43F830: when it equals 1, sub_43F620 jumps to Actor_FindFlowTargetFrame; set by sub_444DF0/sub_444F60 (hit-reaction anim), cleared by sub_4414E0'),
 (712, 4, 'int', 'freezeTicks', '[traced] sub_440550: while >0 only this timer runs (no timers/anim) except the 0x1E0 knockback motion; idle AI loops sub_43EBD0/sub_43EC50 require 0'),
 (716, 1, 'unsigned char', 'drawOffsetMode', '[traced] RboP2DrawOffsetMode: !=0 enables drawOffsetKeys (Actor_GetBoxOrigin), ==1 = interpolated draw from actor +1312 (Actor_GetFrameInterpolation 0x445A50, Actor_DrawTree); also tested by sub_442890'),
 (717, 3, 'unsigned char', 'unused_2CD[3]', '[unused] only the byte at 0x2CC is ever accessed'),
 (720, 4, 'int', 'shakeTicks', '[traced] Actor_SetHitShake 0x4199A0 sets it; Obj_TreeLatchPositions 0x43CED0 tests it; sub_440550 decrements it'),
 (724, 4, 'int', 'shakeTickCounter', '[traced] Obj_TreeLatchPositions ++ it, parity gives the +/- shake'),
 (728, 4, 'int', 'unused_2D8', '[unused] no [reg+0x2D8] access in any actor function'),
 (732, 4, 'int', 'shakeAmplitude', '[traced] set by Actor_SetHitShake (arg a3), multiplied in Obj_TreeLatchPositions'),
 (736, 4, 'int', 'shakeOffsetX', '[traced] Obj_TreeLatchPositions writes +/-(counter%2 * amplitude) (sign by facingLeft); Actor_DrawTree adds *shakeOffsetXPtr to posX'),
 (740, 4, 'int *', 'shakeOffsetXPtr', '[traced] Actor_Init: points at +736; sub_41A650 may point it at the owner shake offset'),
 (748, 28, 'struct RboP2RampFx', 'alphaFx', '[traced] AlphaFadeFx_Init 0x4491C0 / AlphaFadeFx_StartRamp 0x449280; ticked by sub_440550 via fn at +748; value (+752) = alpha used by Actor_DrawTree'),
 (776, 56, 'struct RboP2ZoomFx', 'zoomFx', '[traced] ZoomFx_Init 0x4496A0 (256 = 1.0); Actor_EnterFrame sets it from frame.zoom; Actor_DrawTree reads it via getter at +780'),
 (832, 80, 'struct RboP2RgbFx', 'modulateColorFx', '[traced] RgbFx_Init 0x4493B0(255,255,255); rgb at +840 multiplied into the draw colour by sub_4464D0'),
 (912, 80, 'struct RboP2RgbFx', 'additiveColorFx', '[traced] RgbFx_Init(0,0,0); rgb at +920 is the additive tint read by Actor_GetFlashAddColor 0x446350'),
 (992, 16, 'struct RboP2FlickerGate', 'flickerGate', '[traced] FlickerGate_Tick 0x440DC0 gates the draw in Actor_DrawTree; Actor_AdvanceByAniFlag sets (0,3,1,3)'),
 (1008, 32, 'struct RboP2RotFx', 'rotXFx', '[traced] RotFx_InitStatic 0x449AB0; sub_41A1D0 script op; ticked by sub_440550 via fn at +1012; angle (+1008) is v36 in Actor_DrawTree (axis order inferred)'),
 (1040, 32, 'struct RboP2RotFx', 'rotYFx', '[traced] same as rotXFx; angle (+1040) is v37 in Actor_DrawTree'),
 (1072, 32, 'struct RboP2RotFx', 'rotZFx', '[traced] same as rotXFx; angle (+1072) is v38 in Actor_DrawTree'),
 (1104, 56, 'struct RboP2ZoomFx', 'zoomAddFx', '[traced] ZoomFx_Init(actor+1104,0); Actor_DrawTree adds its getter result (+1108) to zoomFx'),
 (1160, 4, 'void *', 'afterimageTrail', '[traced] ring buffer of 64 x 44-byte entries allocated by sub_44AB10 in sub_41C290 case 0; sub_44ABA0/sub_44AC30 record into it from sub_440550/sub_43FB10'),
 (1164, 40, 'int', 'scriptWork[10]', '[inferred] script-visible actor local variables: sub_42A1C0/sub_42A1D0 hand actor+1164 to the script VM, sub_41E400 indexes it as dword array; size bounded by colorFlash at +1204'),
 (1204, 20, 'struct RboP2ColorFlash', 'colorFlash', '[traced] Actor_StartColorFlash 0x4487A0, Actor_GetFlashAddColor 0x446350, ticked in sub_440550 (bytes +1205/+1207)'),
 (1224, 88, 'struct RboP2RepeatTracker', 'repeatTracker', '[traced] move-repeat penalty tracker: RepeatTracker_Reset 0x4415A0, Actor_UpdateRepeatPenalty 0x4415C0, Actor_InitRepeatBonus 0x441720, Actor_ApplyRepeatPenalty 0x441760; extends to +1311 (history ring)'),
]
