// GOF2 Obj part A (bytes 0x000..0x61F) types, IDA-parsable (idc.parse_decls). Parse after gof2_container_types.h / gof2_frame_types.h (uses Gof2HanteiAnime). Evidence: docs/formats/ida/gof2obj_partA.md.
// Names are prefixed Gof2ObjA / G2OBJA_ so they never collide with the other parts. Flag enums are made bitmask with the IDA enum bitfield flag.
enum Gof2ObjASetupFlags : unsigned int { G2OBJA_SETUP_SKIP_COMMAND_INPUT=1, G2OBJA_SETUP_NO_ENTER_MOVE_OR_BRAKE=2, G2OBJA_SETUP_HOLD_FRAME_TICK=4, G2OBJA_SETUP_RESET_KASANARI_ON_ENTER=0x10, G2OBJA_SETUP_RESET_KOUGEKI_ON_ENTER=0x20, G2OBJA_SETUP_RESET_YARARE_ON_ENTER=0x40, G2OBJA_SETUP_RESET_ETC_ON_ENTER=0x100, G2OBJA_SETUP_RESET_TOBI_ON_ENTER=0x200, G2OBJA_SETUP_MIRROR_EVENT_ARG=0x10000, G2OBJA_SETUP_COPY_TINT_TO_SPAWNED=0x20000, G2OBJA_SETUP_NO_HITSTOP=0x40000, G2OBJA_SETUP_SUPPRESS_CHAIN_CANCEL=0x80000, G2OBJA_SETUP_RECORD_CLASH_MEMORY=0x100000, G2OBJA_SETUP_BLOCK_DIRECTION_INPUT=0x1000000, G2OBJA_SETUP_BLOCK_ATTACK_INPUT=0x2000000, G2OBJA_SETUP_RESET_SOUSAI_ON_ENTER=0x80000000 };
enum Gof2ObjAClassFlags : unsigned int { G2OBJA_CLASS_NIBBLE_MASK=0xf, G2OBJA_CLASS_PLAYER_ENTITY=0x10, G2OBJA_CLASS_SPAWNED=0x20, G2OBJA_CLASS_INHERIT_MARK=0x40 };
enum Gof2ObjAParentActionMask : unsigned int { G2OBJA_PARENTACT_DESTROY=1, G2OBJA_PARENTACT_FREEZE_POSITION=0x10, G2OBJA_PARENTACT_SET_PENDING_PATTERN=0x20, G2OBJA_PARENTACT_ADD_ANCESTOR_DEPTH=0x40 };
enum Gof2ObjAFacingFollow : int { G2OBJA_FFOLLOW_NONE=0, G2OBJA_FFOLLOW_COPY_PARENT_FACING=1, G2OBJA_FFOLLOW_TRACK_PARENT_FACING_ONLY=2, G2OBJA_FFOLLOW_OPPOSITE_ON_PARENT_FACING_CHANGE=3 };
enum Gof2ObjAFadeFlags : unsigned int { G2OBJA_FADE_DEFAULT_10_FRAMES=1, G2OBJA_FADE_CUSTOM_DURATION=2, G2OBJA_FADE_KILL_ON_LIFETIME_END=0x10000 };
enum Gof2ObjAPropagateFlags : unsigned int { G2OBJA_PROP_SKIP_INVULN_WINDOW=1, G2OBJA_PROP_INVULN_CONTINUE_TO_PARENT=2, G2OBJA_PROP_SKIP_HIT_MARK_RESET=0x10, G2OBJA_PROP_HIT_MARK_CONTINUE_TO_PARENT=0x20 };
enum Gof2ObjATurnAroundMode : int { G2OBJA_TURN_AUTO_FACE_OPPONENT=0, G2OBJA_TURN_FACE_HELD_DIRECTION=1 };
enum Gof2ObjAEdgeMode : int { G2OBJA_EDGE_SCREEN_CLAMP=0, G2OBJA_EDGE_STAGE_CLAMP=1, G2OBJA_EDGE_STAGE_CLAMP_STICKY=0x7ffffffe, G2OBJA_EDGE_NONE=0x7fffffff };
enum Gof2ObjAWallContactFlags : unsigned int { G2OBJA_WALL_STAGE_BEYOND_LEFT=0x10, G2OBJA_WALL_STAGE_BEYOND_RIGHT=0x20, G2OBJA_WALL_STAGE_AT_LEFT=0x40, G2OBJA_WALL_STAGE_AT_RIGHT=0x80, G2OBJA_WALL_SCREEN_BEYOND_LEFT=0x100, G2OBJA_WALL_SCREEN_BEYOND_RIGHT=0x200, G2OBJA_WALL_SCREEN_AT_LEFT=0x400, G2OBJA_WALL_SCREEN_AT_RIGHT=0x800, G2OBJA_WALL_CAMERA_LEFT=0x100000, G2OBJA_WALL_CAMERA_RIGHT=0x200000 };
enum Gof2ObjABoundLatchFlags : unsigned int { G2OBJA_LATCH_STAGE_EXIT_PENDING=0x1000000, G2OBJA_LATCH_STAGE_ENTER_PENDING=0x2000000, G2OBJA_LATCH_CAMERA_CLAMP=0x4000000, G2OBJA_LATCH_OFFSCREEN_STAGE_CHECK=0x10000000, G2OBJA_LATCH_OFFSCREEN_SCREEN_CHECK=0x20000000 };
enum Gof2ObjAVertBoundFlags : unsigned int { G2OBJA_VERT_CHECK_CEILING=1, G2OBJA_VERT_CHECK_FLOOR=2, G2OBJA_VERT_DESPAWN_ABOVE_CEILING=4, G2OBJA_VERT_DESPAWN_BELOW_FLOOR=8 };
enum Gof2ObjAVertContactFlags : unsigned int { G2OBJA_VCONTACT_ABOVE_CEILING=0x1000, G2OBJA_VCONTACT_AT_CEILING=0x4000, G2OBJA_VCONTACT_AT_FLOOR=0x8000 };
enum Gof2ObjAScreenBoundFlags : unsigned int { G2OBJA_SBOUND_DESPAWN_BEYOND_SCREEN=1, G2OBJA_SBOUND_DESPAWN_BEYOND_STAGE=2, G2OBJA_SBOUND_FACING_DEPENDENT=0x1000 };
enum Gof2ObjACameraFocusMode : int { G2OBJA_CAMFOCUS_EXCLUDED=0, G2OBJA_CAMFOCUS_USE_FOCUS_XY=1, G2OBJA_CAMFOCUS_USE_ACTOR_POS=2, G2OBJA_CAMFOCUS_ACTOR_POS_CLAMPED_TO_STAGE=3 };
enum Gof2ObjABufferedJumpKind : int { G2OBJA_BJUMP_NONE=-1, G2OBJA_BJUMP_DIRECT=0, G2OBJA_BJUMP_RULE_INDEX=1, G2OBJA_BJUMP_BUTTON_DIRECT=2 };
enum Gof2ObjAMoverKind : int { G2OBJA_MOVER_VELOCITY=1, G2OBJA_MOVER_LIMITED_ACCEL=2, G2OBJA_MOVER_PATH_FOLLOW=4, G2OBJA_MOVER_SCRIPT_CALLBACK=5 };
enum Gof2ObjAMoverLimitFlags : unsigned int { G2OBJA_MLIM_X_ACTIVE=1, G2OBJA_MLIM_Y_ACTIVE=2 };
enum Gof2ObjAFlashMode : int { G2OBJA_FLASH_BLINK=0, G2OBJA_FLASH_FADE=1 };
enum Gof2ObjADrawHelperFollowMode : int { G2OBJA_DHFOLLOW_NONE=0, G2OBJA_DHFOLLOW_HELPER_B_FROM_OWNER=1, G2OBJA_DHFOLLOW_HELPER_A_FROM_OWNER=2, G2OBJA_DHFOLLOW_HELPER_B_BLENDED=3 };
enum Gof2ObjAHookSlot : int { G2OBJA_HOOK_PRE_HIT_REACTION=0, G2OBJA_HOOK_SECONDARY_CLASH=1, G2OBJA_HOOK_ON_ACTION_ENTER=2, G2OBJA_HOOK_DAMAGE=3, G2OBJA_HOOK_PER_TICK=4, G2OBJA_HOOK_POST_HIT=5, G2OBJA_HOOK_KNOCKBACK=6 };
typedef int (__cdecl *Gof2ObjAFxUpdateFn)(void *self);
typedef void (__cdecl *Gof2ObjAZoomGetXYFn)(void *self, int *x, int *y);
typedef int (__cdecl *Gof2ObjAMoverStepFn)(int *x, int *y, int tickRateX, int tickRateY, void *self);
typedef void (__cdecl *Gof2ObjAMoverCopyVelFn)(void *self, void *out48);
typedef void (__cdecl *Gof2ObjAMoverLoadVelFn)(void *self, const void *in48);
typedef int (__cdecl *Gof2ObjAMoverGetVelYFn)(void *self);
typedef void *(__cdecl *Gof2ObjAMoverFlipFn)(void *self);
struct Gof2ObjAEventNode {                      // 8 bytes: one linked-list node of the per-object event lists (rule/enter events); node = {next, event record}
 struct Gof2ObjAEventNode * next;  // +0x00 [traced] TransitionEventList_Link 0x43DF90: `*node = 0; if (tail) *tail = node else head = node` (node[0] = next)
 int * eventRecord;  // +0x04 [traced] TransitionEventList_Link 0x43DF90 / FrameEnterEventList_Link 0x43E120: events[2*count+2] = ScriptVm event record
};
struct Gof2ObjAEventList {                      // 8 bytes: head/tail pair of one event category
 struct Gof2ObjAEventNode * head;  // +0x00 [traced] TransitionEventList_Link 0x43DF90: v3 = list; *tail = node else head = node; walked by ObjEventList_RunUntilHandled 0x436560
 struct Gof2ObjAEventNode * tail;  // +0x04 [traced] TransitionEventList_Link 0x43DF90: list[1] = newest node; ObjEventList_Unlink 0x43DF50 fixes it when the tail is removed
};
struct Gof2ObjATransitionEvents {                      // 100 bytes: script transition-rule events of the current frame (embedded at Obj+0); rebuilt by Obj_CollectTransitionRuleEvents on every action start
 int count;  // +0x00 [traced] TransitionEventList_Link 0x43DF90: refuses at count >= 5; Obj_CollectTransitionRuleEvents 0x43E030 zeroes it
 struct Gof2ObjAEventNode nodePool[5];  // +0x04 [traced] TransitionEventList_Link 0x43DF90: node k = events[2k+1..2k+2]; 5 nodes (count limit)
 struct Gof2ObjAEventList inputRules;  // +0x2C [traced] category 0 (TransitionEventList_Link 0x43DF90 case 0 -> events+11); run per tick by sub_4CDDB0 0x4CDDB0 through ObjEvent_InputRuleDispatch 0x436C10 (events 11/13/121/197/201)
 struct Gof2ObjAEventList overlapRules;  // +0x34 [traced] category 2 (case 2 -> events+13); run by Obj_SpawnFrameEffectRecord 0x4327F0-era overlap pass via ObjEvent_OverlapRuleDispatch 0x43CE30 (event 14, sub_43CC10)
 struct Gof2ObjAEventList conditionalJumps;  // +0x3C [traced] category 1 (case 1 -> events+15); sub_4CDDB0 0x4CDDB0 runs `sub_436560(obj+60, ObjEvent_ConditionalRuleDispatch)` (events 200/210)
 struct Gof2ObjAEventList tickRules;  // +0x44 [traced] category 3 (case 3 -> events+17); Obj_ScriptTick 0x434AD0 runs `sub_436560(this+68, ObjEvent_TickRuleDispatch)` (events 220/255)
 struct Gof2ObjAEventList forcedJumps;  // +0x4C [inferred] category 4 (case 4 -> events+19); run from Obj_SetActionAndRunScript 0x431B90 through sub_436560 (callback in ebx); head only traced via the Link switch
 struct Gof2ObjAEventList endRules;  // +0x54 [inferred] category 7 (case 7 -> events+21); same run site as forcedJumps; head only traced via the Link switch
 int clearedSpare;  // +0x5C [unused] [traced writes only] zeroed by Obj_InitDefaults 0x431000 (+92) and Obj_ResetForCmdBroadcast; no category links into it and no reader exists (all [reg+5Ch] operands in Obj-pointer functions scanned)
 int * singleSlotTimedRule;  // +0x60 [inferred] category 6 (TransitionEventList_Link 0x43DF90 case 6: events[24] = record, single slot); zeroed by Obj_InitDefaults 0x431000 / Obj_ResetForCmdBroadcast; reader only inferred (RBO RboP3 singleSlotTimedRule analog)
};
struct Gof2ObjAFrameEnterEvents {                      // 196 bytes: script frame-enter events of the current frame (embedded at Obj+100); rebuilt by Obj_CollectFrameEnterEvents
 int count;  // +0x00 [traced] FrameEnterEventList_Link 0x43E120: refuses at count >= 20; Obj_CollectFrameEnterEvents 0x43E1B0 zeroes it (Obj_RunFrameScriptLists passes obj+100)
 struct Gof2ObjAEventNode nodePool[20];  // +0x04 [traced] FrameEnterEventList_Link 0x43E120: node k = events[2k+1..2k+2], 20 nodes (count limit)
 struct Gof2ObjAEventList actions;  // +0xA4 [traced] category 0 (FrameEnterEventList_Link 0x43E120 case 0 -> events+41 = obj+264); frame-enter actions
 struct Gof2ObjAEventList onHitAttackerMods;  // +0xAC [inferred] category 1 (case 1 -> events+43 = obj+272); RBO onHitAttackerMods analog; head traced via the Link switch
 struct Gof2ObjAEventList chanceActions;  // +0xB4 [inferred] category 4 (case 4 -> events+45 = obj+280); RBO chanceActions analog; head traced via the Link switch
 struct Gof2ObjAEventList hitEventActions;  // +0xBC [traced] category 5 (case 5 -> events+47 = obj+288); run in Obj_SetActionAndRunScript 0x431B90-era hit pass: `sub_436560(obj+288, ObjEvent_FrameEnterCat5Dispatch)` (event 50, sub_43B880)
};
struct Gof2ObjAOverlayRectNode {                      // 28 bytes: one overlay-rectangle node (singly linked, from g_ObjSubNodePool); drawn as a screen-space quad by DrawHelper_DrawOverlayRects
 struct Gof2ObjAOverlayRectNode * next;  // +0x00 [traced] Obj_ExecOverlayRectOps 0x438D60 case 1 (v4[0] = old head) / DrawHelper_DrawOverlayRects `v5 = *v5`
 int followObjX;  // +0x04 [traced] DrawHelper_DrawOverlayRects 0x49ECB0: non-zero -> x = obj.x + ownerOffset else x = 0; script value of Obj_ExecOverlayRectOps 0x438D60 case 1
 int followObjY;  // +0x08 [traced] DrawHelper_DrawOverlayRects 0x49ECB0: non-zero -> y base = obj.y (rect top is added to it) else y = rect top
 int left;  // +0x0C [traced] DrawHelper_DrawOverlayRects 0x49ECB0: quad x origin (v10[0]); node[3] copied from script by Obj_ExecOverlayRectOps 0x438D60
 int top;  // +0x10 [traced] DrawHelper_DrawOverlayRects 0x49ECB0: quad y origin; node[4]
 int width;  // +0x14 [traced] DrawHelper_DrawOverlayRects 0x49ECB0: quad width (x + width); node[5]
 int height;  // +0x18 [traced] DrawHelper_DrawOverlayRects 0x49ECB0: quad height; node[6]
};
struct Gof2ObjAParentLink {                      // 80 bytes: parent/child link behaviour of a child object (frame-enter setup ops of Obj_ExecStateCmdList 0x438660; consumed by Obj_RunParentLinkRulesOnChildren/Obj_RunParentLinkAction); RBO RboP4FollowState+RboP4ParentLink analog
 int landingInhibit;  // +0x00 [traced] setup op 11 (Obj_ExecStateCmdList 0x438660 case 11 -> obj[88]); Obj_ApplyLandingFrameJump 0x430A40 skips the below-ground landing correction while non-zero; Obj_ResetOnDetach 0x4390D0 sets 1 on attach / 0 on release (RBO expiryInhibit analog)
 int ticksWithParent;  // +0x04 [traced] setup op 0 flags 0x10000/0x20000 (Obj_ExecStateCmdList 0x438660 case 0 -> obj[89]); sub_434A80 (Obj_InheritTickRateFromAncestor) walks parents while it is non-zero; Obj_TickTimersRecursive 0x49E120 tests it (a2 == 1 && i[89] == 1)
 Gof2ObjAFacingFollow facingFollow;  // +0x08 [traced] setup op 14 -> obj[90]; Obj_RunParentLinkRulesOnChildren 0x435130 switch on 1/2/3 (copy / track / opposite)
 int lastParentFacing;  // +0x0C [traced] setup op 14 stores parent+688 (obj[91]); Obj_RunParentLinkRulesOnChildren 0x435130 compares it with the parent's current facing (init -1 in Obj_InitDefaults)
 int flipRelativeXOnFlip;  // +0x10 [traced] setup op 14 -> obj[92]; Obj_ApplyFacingFlipToLinkedChild 0x434F10 negates local X (obj[1148]) when non-zero
 int flipVelocityOnFlip;  // +0x14 [traced] setup op 14 -> obj[93]; Obj_ApplyFacingFlipToLinkedChild 0x434F10 calls the mover flip callback (obj+740) when non-zero
 unsigned int onParentLandedAction;  // +0x18 [traced] setup op 8 -> obj[94]; run through Obj_RunParentLinkAction 0x435070 when the parent's landedLatch (+684) == 1 (Obj_RunParentLinkRulesOnChildren 0x435130)
 unsigned int onParentEventAction;  // +0x1C [traced] setup op 9 -> obj[95]; run through Obj_RunParentLinkAction 0x435070 in the event pass
 unsigned int onParentReactionAction;  // +0x20 [traced] setup op 10 -> obj[96]; run when parent+1900 (reaction state) != -1
 int detachPattern;  // +0x24 [traced] setup op 12 -> obj[97]; Obj_RunParentLinkAction 0x435070 mask 0x20 stores it as pendingPattern
 int detachFrame;  // +0x28 [traced] setup op 12 -> obj[98]; Obj_RunParentLinkAction 0x435070 mask 0x20 stores it as pendingFrame
 int isFollower;  // +0x2C [traced] setup op 2 -> obj[99]; Obj_RaiseHitStopFrames 0x496BE0 / sub_496C30/60/B0 climb parents while it is non-zero to find the hit-stop owner
 int forwardsEvents;  // +0x30 [traced] setup op 3 -> obj[100]; sub_43C1A0 and sub_43C420/43C460 keep propagating contact events to the parent while it is non-zero
 Gof2ObjAPropagateFlags propagateFlags;  // +0x34 [traced] setup op 28 -> obj[101]; ObjTree_RaiseStrikeInvulnWindow 0x43EB30 tests 1/2, sub_43EAE0 0x43EAE0/43EAB0/43EA60 test 0x10/0x20
 int attachedToParent;  // +0x38 [traced] setup op 15 -> obj[102]; cleared by Obj_RunParentLinkAction 0x435070 mask 0x10; HitJudge_ApplyKnockDirAndFacing 0x435B10 / sub_43EBF0 climb parents while non-zero
 int hitCreditToParent;  // +0x3C [traced] setup op 16 -> obj[103]; cleared by Obj_RunParentLinkAction mask 0x10; HitJudge_ResolveTick 0x4996A0 climbs parents while non-zero to credit the hit
 Gof2ObjAFadeFlags fadeFlags;  // +0x40 [traced] setup op 13 -> obj[104]; Obj_StartDetachFade 0x434F90 bit 1 = default fade, bit 2 = custom; Obj_TickTimersRecursive 0x49E120 bit 0x10000 kills at lifetime end
 int fadeFrames;  // +0x44 [traced] setup op 13 -> obj[105]; Obj_StartDetachFade 0x434F90 custom-duration divisor
 int lifetimeFrames;  // +0x48 [traced] setup op 13 -> obj[106]; Obj_StartDetachFade 0x434F90 copies it into the lifetime timer
 int hurtDelegatedToParent;  // +0x4C [traced] setup op 17 -> obj[107]; HitJudge_CollectPairs 0x4978B0 / sub_495B60 / sub_49BB70 climb parents while non-zero to find the hurt-receiving object
};
struct Gof2ObjAScreenBoundState {                      // 32 bytes: edge/bound state (RBO RboP4ScreenBound + wallContactFlags analog); extents live in Gof2ObjABoundRect at Obj+488
 Gof2ObjAEdgeMode edgeMode;  // +0x00 [traced] setup op 5 -> obj[110]; Obj_UpdateScreenBoundContacts 0x49D780 switch (0 screen, 1/0x7FFFFFFE stage, 0x7FFFFFFF none, low bits 2/4/8/0x10 runtime/mode); InitDefaults/ctor = 0x7FFFFFFF
 Gof2ObjAWallContactFlags wallContactFlags;  // +0x04 [traced] Obj_UpdateScreenBoundContacts 0x49D780 ORs 0x10..0x800 (stage/screen beyond/at edge); Obj_ResolveWallContactLatches 0x49E570 masks them; read by Obj_ScriptTick 0x434AD0 (0x2A00A0/0x150050 tests)
 Gof2ObjABoundLatchFlags boundLatchFlags;  // +0x08 [traced] Obj_UpdateScreenBoundContacts 0x49D780 ORs 0x1000000/0x2000000/0x4000000/0x10000000/0x20000000; consumed in Obj_ResolveWallContactLatches 0x49E570
 Gof2ObjAVertBoundFlags vertBoundFlags;  // +0x0C [traced] Obj_UpdateScreenBoundContacts 0x49D780 tail: bit1 ceiling check, bit2 floor check, bits 4/8 despawn (sets destroyRequested)
 int ceilingY;  // +0x10 [traced] Obj_UpdateScreenBoundContacts 0x49D780: y compared with it when vertBoundFlags&1
 int floorY;  // +0x14 [traced] Obj_UpdateScreenBoundContacts 0x49D780: y compared with it when vertBoundFlags&2
 Gof2ObjAVertContactFlags vertContactFlags;  // +0x18 [traced] Obj_UpdateScreenBoundContacts 0x49D780 ORs 0x1000/0x4000/0x8000
 Gof2ObjAScreenBoundFlags screenBoundFlags;  // +0x1C [traced] Obj_UpdateScreenBoundContacts 0x49D780: &3 enable off-screen despawn checks, 0x1000 facing-dependent (Obj_ResolveWallContactLatches 0x49E570)
};
struct Gof2ObjACameraFocus {                      // 16 bytes: camera focus contribution of the object (RBO RboP4CameraFocus analog); read by Camera_ComputeTargetFromFighters
 int useActorPos;  // +0x00 [traced] setup op 4 -> obj[118]; Camera_ComputeTargetFromFighters 0x4A22D0 `if (v4[118] != 0) use actor pos`; Obj_ResetOnDetach 1/0, Obj_SpawnFrameEffectRecord link flag
 Gof2ObjACameraFocusMode mode;  // +0x04 [traced] Camera_ComputeTargetFromFighters 0x4A22D0 switch (1 xy, 2 actor, 3 clamped actor, default skipped); Obj_ResetOnDetach 0x4390D0 3/0
 int x;  // +0x08 [traced] Camera_ComputeTargetFromFighters 0x4A22D0 case 1: v5 = obj[120]
 int y;  // +0x0C [traced] Camera_ComputeTargetFromFighters 0x4A22D0 case 1: v6 = obj[121]
};
struct Gof2ObjABoundRect {                      // 16 bytes: object extents relative to its origin (a Win32 RECT, SetRect at construction)
 int left;  // +0x00 [traced] Obj_UpdateScreenBoundContacts 0x49D780 `x + [122] > screenRight` (left extent); SetRect target (Obj_ConstructPoolSlot)
 int top;  // +0x04 [traced] Obj_UpdateScreenBoundContacts 0x49D780 `y + [123] > floorY` despawn test
 int right;  // +0x08 [traced] Obj_UpdateScreenBoundContacts 0x49D780 `x + [124] < screenLeft`
 int bottom;  // +0x0C [traced] Obj_UpdateScreenBoundContacts 0x49D780 `y + [125] < ceilingY` despawn test
};
struct Gof2ObjABufferedJump {                      // 16 bytes: buffered action jump latched by rule events (RBO RboP3BufferedJump analog)
 Gof2ObjABufferedJumpKind kind;  // +0x00 [traced] ObjEvent_InputRuleBufferJump 0x4365D0 `if (a1[148] == -1)` latch gate; sets 0/1; Obj_TestRelativePosition 0x436470 / ObjEvent_ConditionalRuleSelectJump set 2
 int patternOrRule;  // +0x04 [inferred] ObjEvent_InputRuleBufferJump 0x4365D0: a1[149] = rule index (kind 1) or pattern; write-only in the Obj functions scanned (consumer not found)
 int frame;  // +0x08 [inferred] ObjEvent_InputRuleBufferJump 0x4365D0: a1[150] = target frame; write-only (consumer not found)
 int flip;  // +0x0C [inferred] ObjEvent_InputRuleBufferJump 0x4365D0: a1[151] = facing to force (-1 unchanged); write-only (consumer not found)
};
struct Gof2ObjAMoveVec {                      // 48 bytes: 2D velocity/acceleration block stepped by MoveVec_ApplyToPosition (sub_441000); 12 ints
 int pctX;  // +0x00 [traced] MoveVec_ApplyToPosition 0x441000 `a4 * pct * vel / 100`; Mover_StartVelocity / Obj_ResetMoveTracks memset then =100
 int pctY;  // +0x04 [traced] same for Y
 int velX;  // +0x08 [traced] MoverVelocity_Step 0x4410E0: velX += accX every 256 timer units; MoveVec_InitFields 0x441140 flag 2 sets it
 int velY;  // +0x0C [traced] MoverVelocity_GetVelY 0x440FD0 returns it (vec+12); MoveVec_InitFields flag 4
 int accX;  // +0x10 [traced] MoveVec_InitFields 0x441140 flag 8 sets it; MoverVelocity_Step 0x4410E0 adds it to velX
 int accY;  // +0x14 [traced] MoveVec_InitFields 0x441140 flag 0x10
 int subPosX;  // +0x18 [traced] MoveVec_ApplyToPosition 0x441000: sub-pixel remainder, x += subPosX/128; subPosX %= 128
 int subPosY;  // +0x1C [traced] same for Y
 int velAccumX;  // +0x20 [traced] MoveVec_ApplyToPosition 0x441000: accumulates tick*pct*vel/100, subPosX += accum/256, accum %= 256; zeroed by MoveVec_InitFields flag 2
 int velAccumY;  // +0x24 [traced] same for Y
 int accTimerX;  // +0x28 [traced] MoverVelocity_Step 0x4410E0: when <= 0 velX += accX and timer += 256; then timer -= tickRateX
 int accTimerY;  // +0x2C [traced] same for Y
};
struct Gof2ObjAMoverLimitedPayload {                      // 68 bytes: payload of mover kind 2: velocity with 'stop accelerating at limit' (RBO MoverLimitedAccel analog)
 Gof2ObjAMoverLimitFlags limitFlags;  // +0x00 [traced] MoverLimited_Step 0x4412A0: bit0 X limit armed, bit1 Y limit armed, cleared when the limit is reached
 int accelSignX;  // +0x04 [traced] MoverLimited_Step 0x4412A0 `v8 = a5[10]`: sign selecting which side of limitX stops
 int speedLimitX;  // +0x08 [traced] MoverLimited_Step 0x4412A0 a5[11]: velX is clamped to it then accX = 0
 int accelSignY;  // +0x0C [traced] a5[12]
 int speedLimitY;  // +0x10 [traced] a5[13]
 struct Gof2ObjAMoveVec vec;  // +0x14 [traced] MoverLimited_CopyOutVel 0x441200 copies mover+56 (48 bytes); Obj_ApplyFrameBrake 0x435280 memsets mover+56
};
struct Gof2ObjAMoverPathPayload {                      // 28 bytes: payload of mover kind 4: follow a motion-history ring (path)
 int pathIndex;  // +0x00 [traced] MoverPath_Init 0x441430 a2 -> result[9]; ScriptVm_LoadArgsForObj case 1/0 reads (obj+748)
 int outX;  // +0x04 [traced] MoverPath_Init 0x441430 result[10] / MoverPath_Step 0x441370 `*a1 = a5[10]`
 int outY;  // +0x08 [traced] MoverPath_Step 0x441370 `*a2 = a5[11]`
 void * path;  // +0x0C [traced] MoverPath_Init 0x441430 result[12] = followed object's motion-history ring (obj+296 of the target); read by ScriptVm_LoadArgsForObj via obj+760
 unsigned int clampFlags;  // +0x10 [traced] MoverPath_Step 0x441370: bit0 clamp X distance, bit1 clamp Y
 int maxDX;  // +0x14 [traced] MoverPath_Step 0x441370 a5[14]
 int maxDY;  // +0x18 [traced] MoverPath_Step 0x441370 a5[15]
};
struct Gof2ObjAMoverScriptPayload {                      // 16 bytes: payload of mover kind 5: ScriptVm callback decides the position
 int scriptKind;  // +0x00 [traced] MoverScript_Step 0x4414B0 `ScriptVm_CallSyncScript(obj+64600, a5[9], a5[10])`
 int scriptId;  // +0x04 [traced] a5[10]
 void * ownerObj;  // +0x08 [traced] MoverScript_Step 0x4414B0 a5[11]: object whose position/pattern is handed to the script
 int skipSetup;  // +0x0C [inferred] MoverScript_Step 0x4414B0 `if (a5[12] == 0)` fill script args; set from a2[2] by MoverScript_Init
};
union Gof2ObjAMoverPayload {                      // 68 bytes: kind-dependent payload (union)
 struct Gof2ObjAMoveVec velocity;  // +0x00 Mover_StartVelocity 0x4411B0 memset/fill of mover+36 (kind 1)
 struct Gof2ObjAMoverLimitedPayload limited;  // +0x00 MoverLimited_Step 0x4412A0 (kind 2)
 struct Gof2ObjAMoverPathPayload path;  // +0x00 MoverPath_Init 0x441430 (kind 4)
 struct Gof2ObjAMoverScriptPayload script;  // +0x00 MoverScript_Init 0x441590 (kind 5)
 unsigned char raw[68];  // +0x00 largest member (limited) sets the size
};
struct Gof2ObjAMover {                      // 104 bytes: one motion track of the object (3 per Obj; RBO RboMover analog but with 5 callbacks); stepped with (&obj.x, &obj.y, tick rates, mover)
 unsigned char unused_00[12];  // +0x00 [unused] no constructor writes +0..+11 and no mover callback reads them; operand scans of [reg+712..723], [reg+824..835], [reg+932..943] in Obj functions only find address-of uses
 Gof2ObjAMoverStepFn step;  // +0x0C [traced] Obj_ApplyLandingFrameJump 0x430A40 `(*(a1+724))(&x,&y,w,h,a1+712)`; stored by Mover_StartVelocity / MoverLimited / MoverPath_Init / MoverScript_Init
 Gof2ObjAMoverCopyVelFn copyOutVelocity;  // +0x10 [traced] Obj_ApplyFrameMoveFlags 0x434950 `(v1[182])(v1+178, buf)`; MoverVelocity_CopyOutVel / MoverLimited_CopyOutVel
 Gof2ObjAMoverLoadVelFn loadVelocity;  // +0x14 [traced] Obj_ApplyFrameMoveFlags 0x434950 `(v1[183])(v1+178, buf)`
 Gof2ObjAMoverGetVelYFn getVelocityY;  // +0x18 [traced] Obj_ScriptTick 0x434AD0 `(*(this+736))(this+712) > 0` (falling test)
 Gof2ObjAMoverFlipFn flipHorizontal;  // +0x1C [traced] Obj_ApplyFacingFlipToLinkedChild 0x434F10 `(v1[185])(v1+178)`; negates velX/accX
 Gof2ObjAMoverKind kind;  // +0x20 [traced] ScriptVm_LoadArgsForObj 0x43D110 `*(a1+744) == 4`; Obj_ResetMoveTracks writes 1; MoverLimited 2, MoverPath 4, MoverScript 5
 union Gof2ObjAMoverPayload payload;  // +0x24 [traced] Mover_StartVelocity 0x4411B0 vec at +36; MoverLimited vec at +56 (Obj_ApplyFrameBrake 0x435280)
};
struct Gof2ObjAColorFlash {                      // 32 bytes: timed additive colour flash on the object (RBO RboP2ColorFlash analog)
 Gof2ObjAFlashMode mode;  // +0x00 [traced] Obj_ExecColorFlashOps 0x43A2C0 `a2[259] = *v5` (0 blink sets period 2, 1 fade)
 int addR;  // +0x04 [traced] Obj_ExecColorFlashOps 0x43A2C0 a2[260] = table byte_5A2460[3*idx]
 int addG;  // +0x08 [traced] a2[261]
 int addB;  // +0x0C [traced] a2[262]
 int blinkPeriod;  // +0x10 [traced] Obj_ExecColorFlashOps 0x43A2C0 a2[263] = 2 in blink mode; Obj_SetActionAndRunScript sets 2
 int elapsed;  // +0x14 [traced] Obj_TickTimersRecursive 0x49E120 `++i[264]` while remaining != 0
 int remaining;  // +0x18 [traced] Obj_TickTimersRecursive 0x49E120 `i[265] = v31 - 1`; Obj_InitDefaults zeroes (+1060)
 int total;  // +0x1C [traced] Obj_ExecColorFlashOps 0x43A2C0 a2[266] = duration (fade denominator)
};
struct Gof2ObjARamp {                      // 20 bytes: scalar ramp stepped by Ramp_Step (RBO RboP2Ramp analog)
 int velocity;  // +0x00 [traced] Ramp_Step 0x4415D0 `this[3] += *this`
 int accel;  // +0x04 [traced] Ramp_Step 0x4415D0 `v6 = this[1]; velocity += accel`
 int velocityLimit;  // +0x08 [traced] Ramp_Step 0x4415D0 clamp (returns |0x10/0x20)
 int value;  // +0x0C [traced] Ramp_Step 0x4415D0 24.8 fixed-point accumulator
 int valueLimit;  // +0x10 [traced] Ramp_Step 0x4415D0: hit returns 1/2 and clamps
};
struct Gof2ObjAAlphaFx {                      // 28 bytes: alpha ramp (RBO RboP2RampFx analog)
 Gof2ObjAFxUpdateFn update;  // +0x00 [traced] Obj_TickEffectBlocks 0x430BE0 calls `(*(a1+1068))(a1+1068)`; Fx_NoOpReturn0 or RampFx_Step
 int value;  // +0x04 [traced] RampFx_Step 0x441630 `a1[1] = a1[5]/256` (alpha 0..255; Obj_InitDefaults 255)
 struct Gof2ObjARamp ramp;  // +0x08 [traced] Obj_StartDetachFade 0x434F90 writes vel/accel/limits/value at obj+1076..
};
struct Gof2ObjAZoomFx {                      // 56 bytes: zoom effect (RBO RboP2ZoomFx analog); 256 = 1.0
 Gof2ObjAFxUpdateFn update;  // +0x00 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1096))(a1+1096)`
 Gof2ObjAZoomGetXYFn getXY;  // +0x04 [traced] DrawHelper_Draw 0x49EF60 `(*(a2+1100))(a2+1096,&x,&y)`; ZoomFx_GetStaticXY returns rampX.velocity
 int valueX;  // +0x08 [traced] sub_441D60 0x441D60 a1[2] = rampX.value/256
 int valueY;  // +0x0C [traced] sub_441D60 0x441D60 a1[3]
 struct Gof2ObjARamp rampX;  // +0x10 [traced] Obj_ApplyFrameBlendAlphaZoom 0x435620 stores the static zoom in rampX.velocity (obj+1112)
 struct Gof2ObjARamp rampY;  // +0x24 [traced] sub_441D60 0x441D60 second ramp (a1[9..13])
};
struct Gof2ObjARgbFx {                      // 80 bytes: 3-channel colour effect (modulate or additive tint); RBO RboP2RgbFx analog
 Gof2ObjAFxUpdateFn update;  // +0x00 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1152))(a1+1152)` / +1232; RgbFx_UpdateNoOp (returns 1) or RgbFx_StepRamps
 int unused_04;  // +0x04 [unused] no reader found (RBO RgbFx unused_04 analog); only RgbFx_StepRamps/StartRamps use +8..
 int rgb[3];  // +0x08 [traced] Obj_InitDefaults 0x431000 +1160/1164/1168 = 255 (modulate), +1240/1244/1248 = 0 (additive); Draw_ObjTreeLayerWithAfterimages 0x49F850 copies them
 struct Gof2ObjARamp ramp[3];  // +0x14 [traced] RgbFx_StartRamps 0x441900 / RgbFx_StepRamps 0x4417E0: three per-channel ramps (+20/+40/+60), value/256 -> rgb[i]
};
struct Gof2ObjARotFx {                      // 32 bytes: single-axis rotation effect (RBO RboP2RotFx analog); angle is 8.8 degrees
 int angle;  // +0x00 [traced] DrawHelper_Draw 0x49EF60 `*(a2+1328)/256`; sub_49C7D0 stores 70*256
 Gof2ObjAFxUpdateFn update;  // +0x04 [traced] Obj_TickEffectBlocks 0x430BE0 `(*(a1+1332))(a1+1328)`; Fx_NoOpReturn0 by default
 int baseAngle;  // +0x08 [inferred] sub_49C7D0 0x49C7D0 stores 17920 at +8 together with +0; Obj_InitDefaults zeroes it
 unsigned char unused_0C[20];  // +0x0C [unused] no Obj-based access to +12..+31 of any rotation block (scan of [reg+1340..1359], [reg+1372..1391], [reg+1404..1423] hits only non-Obj structs: sub_4CA020 scene, sub_41C800, sub_4AF5C0); the RBO accum/angVel fields do not exist in GOF2 (update fn is always the no-op)
};
struct Gof2ObjAFlickerGate {                      // 16 bytes: draw flicker gate (RBO RboP2FlickerGate analog)
 int phase;  // +0x00 [unused] zeroed by Obj_InitDefaults 0x431000 only (+1312); no reader in any Obj-pointer function (operand scan of [reg+520h..52Fh])
 int period;  // +0x04 [unused] zeroed by Obj_InitDefaults 0x431000 only (+1316); no reader (scan)
 int visibleTicks;  // +0x08 [unused] zeroed by Obj_InitDefaults 0x431000 only (+1320); no reader (scan)
 int spare;  // +0x0C [unused] zeroed by Obj_InitDefaults 0x431000 only (+1324); no reader (scan)
};
struct Gof2ObjAInvulnWindows {                      // 16 bytes: two pattern-bound invulnerability windows (frames left = -1/-2 permanent)
 int strikePattern;  // +0x00 [traced] Obj_ResetHanteiBoxObjects 0x4356A0: window cleared when the pattern (+620) differs; sub_433A60 0x433A60 sets 33
 int strikeFrames;  // +0x04 [traced] Obj_TickTimersRecursive 0x49E120 `i[379]--`; HitJudge_CollectPairs 0x4978B0 `m[379] == 0` gate
 int clashPattern;  // +0x08 [traced] Obj_ResetHanteiBoxObjects 0x4356A0 (a1[380])
 int clashFrames;  // +0x0C [traced] Obj_TickTimersRecursive 0x49E120 `i[381]--`; HitJudge_ResolveSecondaryBoxClashes 0x49ACB0 `v3[381] != 0` gate
};
