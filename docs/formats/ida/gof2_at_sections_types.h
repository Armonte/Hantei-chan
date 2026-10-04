// GOF2 (Glove on Fight 2) pattern-area sections 3..8 (attack records, legacy tables, script lists, effect-spawn records) and PAT v4 types, IDA-parsable (idc.parse_decls).
// Evidence per field: docs/formats/gof2_at_sections.md. Tags: T = reader traced in GOF2.exe.i64 and meaning proven by its behaviour, I = reader traced but meaning only inferred,
// E = editor-only (nonzero in the data, no reader anywhere in the exe), U = unused (zero in every shipped record AND no reader). Enum members are prefixed G2AT_/G2EFF_/G2PAT_ so they never collide with other enums.
// Sizes: Gof2AtRecord 0xEC (section 3), Gof2Section4Record 0x1C, Gof2Section5Record 0x14, Gof2ScriptListEntry 0x14 (sections 6/7), Gof2EffectSpawnRecord 0x60 (section 8), Gof2PatPart 0x5C.
enum Gof2AtHitClass : unsigned int { G2AT_HC_LIGHT_0=0, G2AT_HC_LIGHT_1=1, G2AT_HC_LIGHT_2=2, G2AT_HC_MEDIUM_3=3, G2AT_HC_MEDIUM_4=4, G2AT_HC_MEDIUM_5=5, G2AT_HC_HEAVY_6=6, G2AT_HC_HEAVY_7=7, G2AT_HC_CRUMPLE_8=8, G2AT_HC_SWEEP_9=9, G2AT_HC_LAUNCH_10=10, G2AT_HC_LAUNCH_11=11, G2AT_HC_LAUNCH_12=12, G2AT_HC_LAUNCH_13=13, G2AT_HC_MEDIUM_14=14, G2AT_HC_LAUNCH_15=15, G2AT_HC_LIGHT_16=16, G2AT_HC_NO_REACTION=17 };
enum Gof2AtSparkKind : unsigned int { G2AT_SPARK_DEFAULT_0=0, G2AT_SPARK_DEFAULT_1=1, G2AT_SPARK_DEFAULT_2=2, G2AT_SPARK_DEFAULT_6=6, G2AT_SPARK_BIG_ROTATING=8, G2AT_SPARK_DEFAULT_11=11 };
enum Gof2AtDrawOrderMode : unsigned int { G2AT_DRAWORD_VICTIM_FRONT=0, G2AT_DRAWORD_VICTIM_FRONT_ANCHORED=1, G2AT_DRAWORD_ATTACKER_FRONT=2, G2AT_DRAWORD_RESET_ONLY=300, G2AT_DRAWORD_NONE=0xFFFFFFFF };
enum Gof2AtHitEffectKind : unsigned int { G2AT_HITFX_FROM_ATTACKER_FLAGS=0, G2AT_HITFX_KIND_1=1, G2AT_HITFX_KIND_2=2, G2AT_HITFX_KIND_3=3, G2AT_HITFX_KIND_4=4, G2AT_HITFX_KIND_5=5 };
enum Gof2AtKnockDirMode : unsigned int { G2AT_KDIR_ATTACKER_FACING=0, G2AT_KDIR_ATTACKER_FACING_REVERSED=1, G2AT_KDIR_BY_POSITION=2, G2AT_KDIR_BY_POSITION_REVERSED=3, G2AT_KDIR_NONE=4 };
enum Gof2AtFacingMode : unsigned int { G2AT_FACE_BY_POSITION=0, G2AT_FACE_BY_POSITION_REVERSED=1, G2AT_FACE_ATTACKER_FACING=2, G2AT_FACE_ATTACKER_FACING_REVERSED=3, G2AT_FACE_KEEP_VICTIM=4 };
enum Gof2AtHitFlags : unsigned int { G2AT_HF_SCREEN_SHAKE=0x1, G2AT_HF_NO_HUD_DAMAGE_TRACE=0x2, G2AT_HF_IGNORE_HIT_PROTECTION=0x4000, G2AT_HF_NO_HITSTOP=0x10000 };
enum Gof2AtReactionFlags : unsigned int { G2AT_RF_SUPPRESS_SPECIAL_DAMAGE_FLAG=0x2, G2AT_RF_BLOCK_RECOVER_A=0x10, G2AT_RF_BLOCK_RECOVER_B=0x20, G2AT_RF_UNREAD_0200=0x200, G2AT_RF_NO_SPARK_NO_POPUP=0x1000000, G2AT_RF_HALVE_KNOCK_SCALE=0x8000000 };
enum Gof2AtGuardMask : unsigned int { G2AT_GM_STAND=0x1, G2AT_GM_CROUCH=0x2, G2AT_GM_BIT4=0x4, G2AT_GM_UNGUARDABLE_IN_MAIN_PASS=0x10000 };
enum Gof2AtKind : unsigned int { G2AT_KIND_NORMAL=0, G2AT_KIND_SPECIAL=1 };
enum Gof2AtTargetStance : unsigned int { G2AT_TS_GROUND=0x1, G2AT_TS_CROUCH=0x2, G2AT_TS_AIR=0x4, G2AT_TS_HIT_REACTION_STATES=0x8 };
enum Gof2EffFacingCmd : unsigned int { G2EFF_FACE_KEEP=0, G2EFF_FACE_OPPOSITE_ATTACKER=1, G2EFF_FACE_SAME_AS_ATTACKER=2, G2EFF_FACE_FORCE_0=3, G2EFF_FACE_FORCE_1=4 };
enum Gof2EffStateMode : unsigned int { G2EFF_STATE_FROM_YARARE_TABLE=0, G2EFF_STATE_EXPLICIT=1, G2EFF_STATE_HIT_REACTION=2 };
enum Gof2EffLinkMode : unsigned int { G2EFF_LINK_NONE=0, G2EFF_LINK_COPY_CONTEXT=1, G2EFF_LINK_FLAG_ON=2, G2EFF_LINK_MODE0=3, G2EFF_LINK_MODE1=4, G2EFF_LINK_MODE2=5, G2EFF_LINK_MODE3=6 };
enum Gof2PatPartFlip : unsigned int { G2PAT_FLIP_X=0x1, G2PAT_FLIP_Y=0x2, G2PAT_FORCE_ADDITIVE_BLEND=0x100, G2PAT_UNREAD_10000=0x10000 };
struct Gof2AtRecord {                  // 236 bytes; section 3 of the pattern area, indexed by frame +0xC8 (CAppHanteiKougeki_BuildFromFrame: attackRecords + 236*idx -> CAppHanteiKougeki+0xC -> Obj+0x620)
 Gof2AtHitClass hitClass;              // +0x00 T: victim reaction class (HitJudge_StartVictimReaction -> HitClass_ToStandReactionState picks victim pattern 28..39; 17 = no reaction/no damage scaling: HitJudge_ApplyHit, sub_495890)
 unsigned int hitStopLevel;            // +0x04 T: 0..6 index into the hit-stop tables g_ControllerManage[78] (attacker) / [79] (victim) / [82] (counter bonus) in HitJudge_ApplyHitStop 0x496CE0
 Gof2AtSparkKind sparkKind;            // +0x08 I: only ==8 is tested (HitJudge_ApplyHit, HitJudge_CollectPairs): selects the big rotating spark (Effect_SpawnHitSpark kind 2); every other value uses kind (attackKind != 0)
 unsigned int unused_0C;               // +0x0C U: zero in all 997 records, no reader (RBO had the gate mode here)
 unsigned int damage;                  // +0x10 T: base damage; HitJudge_ComputeDamage 0x494FF0 uses -damage as the HP delta, HitJudge_ApplyHit scores it and feeds the HUD trace (0..1800 in data)
 unsigned int editorOnly_14;           // +0x14 E: 0x64 in 6 records, no reader
 unsigned int editorOnly_18;           // +0x18 E: nonzero in 590 records (0..800, RBO guard damage), no reader in GOF2 (chip damage is derived from damage)
 Gof2AtDrawOrderMode drawOrderMode;    // +0x1C I: != -1 resets the victim link context and calls SideDrawOrder_BringToFront (0/1 victim front, 2 attacker front); ==1 also anchors the victim (HitJudge_ApplyHit tail)
 Gof2AtHitEffectKind hitEffectKind;    // +0x20 I: 0 = derive 1..3 from attacker flag bits 1/2/4 else 1..5 selects the effect started by HitJudge_StartHitEffect 0x4317D0 (HitJudge_ResolveTick)
 unsigned int editorOnly_24;           // +0x24 E: 1 in 9 records, no reader
 unsigned int hitSoundBankSelect;      // +0x28 I: 0 = SystemSE bank (g_SystemSeScriptBank), nonzero = attacker character bank (HitJudge_RunHitSoundCommand 0x49D1A0); 0 in all data
 int hitSoundId;                       // +0x2C I: >= 0 triggers ScriptVm kind 2 / id 2 whose command record is executed on the attacker (HitJudge_RunHitSoundCommand); -1 = none
 unsigned int hitSoundVariant;         // +0x30 I: second argument of the same script call (0 or 2 in data)
 Gof2AtKnockDirMode knockDirMode;      // +0x34 T: HitJudge_SetKnockDirection 0x435A80 (via HitJudge_ApplyKnockDirAndFacing): victim knock direction sign
 Gof2AtFacingMode victimFacingMode;    // +0x38 T: HitJudge_SetVictimFacing 0x4359E0: victim pending facing (a3[477]); 4 keeps the victim facing
 unsigned int unused_3C[4];            // +0x3C U: zero in all records, no reader (RBO element/damage-type fields)
 Gof2AtHitFlags hitFlags;              // +0x4C T: bit0 screen shake (HitJudge_ApplyHit), bit1 skips HUD damage trace sub_4A05B0, 0x4000 bypasses victim hit protection (HitJudge_CollectPairs), 0x10000 skips hit-stop (HitJudge_ApplyHitStop)
 Gof2AtReactionFlags reactionFlags;    // +0x50 T: bit1 (HitJudge_ApplyHit v29, HitJudge_StartVictimReaction), 0x10/0x20 -> Victim_SetRecoverBlockFlags, 0x1000000 no spark/popup (HitJudge_ApplyHit), 0x8000000 -> HitJudge_ApplyKnockDirAndFacing; 0x200 present in data but unread
 unsigned int editorOnly_54;           // +0x54 E: 0x645 in 3 records, no reader (RBO hit-script enable)
 unsigned int unused_58[5];            // +0x58 U: zero in all records, no reader (+0x58..+0x6B)
 unsigned int clashRecoilState;        // +0x6C T: decimal packed frame*1000+pattern; the attacker enters it after a secondary-box contact (HitJudge_ResolveSecondaryBoxClashes 0x49ACB0); word +0x6E != 0 moves the boxes to the secondary list (CAppHanteiKougeki_BuildFromFrame)
 unsigned int unused_70[2];            // +0x70 U: zero in all records, no reader (RBO hit skill id)
 unsigned int hitScriptId;             // +0x78 T: ScriptVm kind 4 id of the on-hit command list run by HitJudge_ApplyHit (0 = none; 1 in one record)
 Gof2AtGuardMask guardMask;            // +0x7C T: bit0 stand-guardable, bit1 crouch-guardable, bit2 extra armor bit, 0x10000 not guardable in the main pass (HitJudge_DefenderGuardsAttack 0x496930, HitJudge_DefenderArmorAbsorbs 0x497180, ObjScan_StoreAttackGuardInfo, sub_43BA00)
 Gof2AtKind attackKind;                // +0x80 T: 0 normal, 1 special: damage class tables (HitJudge_ComputeDamage), armor check, guard rule, script condition (sub_43BA00), spark kind
 unsigned int screenShakeId;           // +0x84 T: ScreenYure script entry (g_ScreenYureScriptBank) started by HitJudge_ApplyHit when hitFlags.SCREEN_SHAKE (100/102/105 in data)
 unsigned int unused_88[2];            // +0x88 U: zero in all records, no reader
 Gof2AtTargetStance targetStanceMask;  // +0x90 T: victim stance classes this attack may hit (victim obj+660 & mask); bit3 also hits victims already in hit-reaction states 28..39 (HitJudge_CollectPairs, HitJudge_ResolveSecondaryBoxClashes)
 unsigned int unused_94[12];           // +0x94 U: zero in all records, no reader (+0x94..+0xC3)
 unsigned int hitScriptGate;           // +0xC4 T: read by HitJudge_CollectPairs (non-zero runs the kind-4 list for a fixed-damage override); zero in all 997 records
 unsigned int unused_C8[9];            // +0xC8 U: zero in all records, no reader (+0xC8..+0xEB)
};
struct Gof2Section4Record {            // 28 bytes; section 4, indexed (0-based) by frame +0xEC[0] when frame +0xE8 == 1; NO reader in the exe (view+0x14 and the frame fields are never read)
 unsigned int kind;                    // +0x00 E: 1 (chr_select files) or 0xFF (SYSTEMEFFECT)
 unsigned int valueA;                  // +0x04 E: 0, 0x100 or 0x200 (looks like a start zoom, 256 = 1.0)
 unsigned int valueB;                  // +0x08 E: 0 or 0x100 (looks like an end zoom)
 unsigned int ticks;                   // +0x0C E: 0, 1 or 30 (looks like a duration)
 unsigned int unused_10[3];            // +0x10 U: zero in all 7 records, no reader
};
struct Gof2Section5Record {            // 20 bytes; section 5, indexed (0-based) by frame +0xFC[3] when frame +0xF8 == 1; NO reader in the exe (view+0x18 is never read)
 unsigned int kind;                    // +0x00 E: 1 in all 3 records
 unsigned int unused_04[4];            // +0x04 U: zero in all records, no reader
};
struct Gof2ScriptListEntry {           // 20 bytes; sections 6 and 7: five script ids, 0 = skip, record 0 is an all-zero dummy (frame +0xBC / +0xC0 pick the record, 0 = none)
 unsigned int scriptId[5];             // +0x00 T: Obj_CollectFrameEnterEvents 0x43E1B0 (section 6, ScriptVm kind 0) / Obj_CollectTransitionRuleEvents 0x43E030 (section 7, kind 1) read slots 0..4 in order; id indexes the character .FOB index table of that kind
};
struct Gof2EffectSpawnRecord {         // 96 bytes; section 8, indexed (1-based) by frame +0x190, record 0 is a dummy; Obj_SetActionAndRunScript spawns it when dword +0x00 != 0 (Obj_SpawnFrameEffectRecord 0x4327F0 re-poses every held target object)
 unsigned int enable;                  // +0x00 T: non-zero = record active (gate in Obj_SetActionAndRunScript); 1 in all 729 records
 int targetOffsetX;                    // +0x04 T: held target X offset from the attacker hold point (multiplied by the facing sign)
 int targetOffsetY;                    // +0x08 T: held target Y offset
 Gof2EffFacingCmd facingCmd;           // +0x0C T: target facing change (switch in Obj_SpawnFrameEffectRecord; 0 keep, 1 opposite, 2 same, 3/4 force)
 Gof2EffStateMode stateMode;           // +0x10 T: how the target state is chosen; modes 1 and 2 also detach the link after the spawn
 int stateArg0;                        // +0x14 T: mode 0 YarareData table index, mode 1 target pattern, mode 2 hit class (sub_435E10)
 int stateArg1;                        // +0x18 T: mode 1 target frame (-1 = start), mode 2 victim facing mode (HitJudge_SetVictimFacing)
 int stateArg2;                        // +0x1C T: mode 2 knock direction mode (HitJudge_SetKnockDirection); read only in mode 2
 unsigned int unused_20[3];            // +0x20 U: zero in all records, no reader
 Gof2EffLinkMode linkMode;             // +0x2C T: switch after the state change (copy link context / set flags on the target obj+472/+476)
 unsigned int unused_30[5];            // +0x30 U: zero in all records, no reader
 int throwDamage;                      // +0x44 T: non-zero runs Obj_ApplyEffectRecordThrowDamage 0x4326C0: damage handed to the target through the ScriptVm kind-0/id-7 table (500/400/50/900 in data)
 unsigned int eventScriptId;           // +0x48 T: ScriptVm kind 5 id; the returned list is only skipped over (no effect); 1 in 3 records
 int throwScriptArg;                   // +0x4C T: argument of the kind-0/id-7 call (Obj_ApplyEffectRecordThrowDamage); zero in all records
 unsigned int unused_50[4];            // +0x50 U: zero in all records, no reader
};
struct Gof2PatHeader {                 // 24 bytes at PAT start; never read by the loader (only decrypted when xorFlag)
 unsigned int version;                 // +0x00 E: 4 in all 64 PATs (RBO 3)
 unsigned int byteOrderMarker;         // +0x04 E: 0x01234567 in all 64 PATs
 unsigned int unused_08[4];            // +0x08 U: zero, not read
};
struct Gof2PatFileHead {               // 72028 bytes: header, 2000 pose offsets, 2000 names, image offset (CharacterLoad state 6 reads exactly 72028 bytes)
 struct Gof2PatHeader header;          // +0x0000 E: see Gof2PatHeader
 unsigned int poseOffset[2000];        // +0x0018 T: absolute PAT offset of the pose's first part; 0 = absent; part index = (off-72028)/92 (CharacterLoad state 6 loop to 8068 = 4*2000+68)
 char poseName[64000];                // +0x1F58 E: 2000 x 32-byte CP932 labels, never read
 unsigned int imageAreaOffset;         // +0x11958 T: PAT offset of the texture area; poses occupy [72028, imageAreaOffset) packed in 3680-byte blocks (CharacterLoad state 6 stores size-72028 at chara+60)
};
struct Gof2PatPart {                   // 92 bytes; field-for-field RboPatPart (draw code verified: DrawSort_Run 0x440520 + PosePart_* helpers), pose = 40 parts
 int x;                                // +0x00 T: PosePart_ComputeGeometry 0x43FE50 (lerped toward the next pose)
 int y;                                // +0x04 T: same
 int destWidth;                        // +0x08 T: same (current part only)
 int destHeight;                       // +0x0C T: same
 Gof2PatPartFlip flipFlags;            // +0x10 T: bit0/1 mirror (PosePart_GetFlippedSrcRect 0x4402A0); byte +0x11 != 0 forces the additive blend state (DrawSort_Run); 0x10000 never read
 int scaleXPermille;                   // +0x14 T: PosePart_ComputeGeometry
 int scaleYPermille;                   // +0x18 T: same
 int rotation10000;                    // +0x1C T: same (10000 = full turn, Math_LerpAngle10000ToDeg 0x43FD00)
 unsigned int modulateArgb;            // +0x20 T: PosePart_ModulateColor 0x440000 (bytes A,R,G,B in memory order; 0xFFFFFFFF = unchanged)
 unsigned int addRgb;                  // +0x24 T: PosePart_AddColor 0x440170 (bytes R,G,B; +0x27 never read, zero)
 int textureIndex;                     // +0x28 T: DrawSort_Run texture slot lookup (0xFFFF = clip part, skipped by DrawSort_BubbleByPriority)
 int srcX;                             // +0x2C T: full dword read (RBO int16 + unused hi word); 1/256 of the texture
 int srcY;                             // +0x30 T: same
 int srcW;                             // +0x34 T: dword; 0 = part slot unused (DrawSort_BubbleByPriority)
 int srcH;                             // +0x38 T: dword; 0 = part slot unused
 unsigned int layer;                   // +0x3C T: full dword << 8 is the sort key (RBO byte)
 int originX;                          // +0x40 T: PosePart_ComputeGeometry pivot X
 int originY;                          // +0x44 T: pivot Y
 unsigned int unused_48[5];            // +0x48 U: zero in all 1233240 parts, no reader
};
struct Gof2AttackBoxSetPrefix {        // first 16 bytes of CAppHanteiKougeki (embedded in Obj at +0x614, vtable 0x573CB8); the rest (box pointer list +0x10, slot numbers +0x30, hit limiter +0x50/+0x54, flags) is not typed here
 void *vtable;                         // +0x00 T: vf0 returns the primary box count (this+4), vf8 the secondary count (this+8)
 unsigned int primaryCount;            // +0x04 T: attack boxes that hit through HitJudge_CollectPairs; moved to secondaryCount when the record word +0x6E != 0 (CAppHanteiKougeki_BuildFromFrame)
 unsigned int secondaryCount;          // +0x08 T: boxes tested by HitJudge_ResolveSecondaryBoxClashes
 struct Gof2AtRecord *record;          // +0x0C T: attackRecords + 236*frame.attackRecordIdx (read through Obj+0x620 = obj+1568 by every hit routine)
};
