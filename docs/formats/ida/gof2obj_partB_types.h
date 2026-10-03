// GOF2 Obj part B types (bytes 0x620..0xC3F of the 0x1264-byte pool slot). Evidence tags: T = traced, I = inferred, U = unused.
enum Gof2ObjHitFlagBits : unsigned int { G2OBJ_HF_SELF=1, G2OBJ_HF_FROM_DESCENDANT=2 };
enum Gof2ObjRecoverFlags : unsigned int { G2OBJ_RECOVER_ARMED_A=1, G2OBJ_RECOVER_ARMED_B=2, G2OBJ_RECOVER_BLOCKED_A=0x10, G2OBJ_RECOVER_BLOCKED_B=0x20 };
enum Gof2ObjPushMode : unsigned int { G2OBJ_PUSH_SPLIT_OVERLAP=0, G2OBJ_PUSH_DISPLACE_OTHER=1, G2OBJ_PUSH_MEET_AT_MIDPOINT=2, G2OBJ_PUSH_DISPLACE_SELF_BY_OTHER_FACING=3, G2OBJ_PUSH_DISPLACE_SELF_HEIGHT_AWARE=4 };
enum Gof2ObjReactionKind : unsigned int { G2OBJ_RK_HIT=0, G2OBJ_RK_GUARD=1 };
enum Gof2ObjKnockXMode : unsigned int { G2OBJ_KNOCKX_NORMAL=0, G2OBJ_KNOCKX_PLUS20=1, G2OBJ_KNOCKX_MINUS20=2 };
enum Gof2ObjKnockYMode : unsigned int { G2OBJ_KNOCKY_NORMAL=0, G2OBJ_KNOCKY_TIER_A=1, G2OBJ_KNOCKY_TIER_B=2 };
enum Gof2ObjKnockFlags : unsigned int { G2OBJ_KNOCKF_HALVE_X=1 };
struct Gof2ObjPushBoxSet {
 void * vtable; // +0x00 T CAppHanteiKasanari vftable 0x573CE0 (vf0 sub_414300 returns the count)
 unsigned int count; // +0x04 T CAppHanteiKasanari__vf5 0x449620 sets 0 and 1 when frame.kasanariBoxIdx != -1
 struct Gof2BoxRect * box; // +0x08 T vf5 stores view->boxes[kasanariBoxIdx]; vf6 0x449680 / vf2 0x4496D0 read the four shorts (mirrored when facing flips)
 int slot; // +0x0C T vf5 writes 0 at +12+count (single slot)
};
struct Gof2ObjHurtBoxSet {
 void * vtable; // +0x00 T CAppHanteiYarare vftable 0x573D00 (vf0 CWSServerClient_GetSocket returns +8)
 void * frameParamBlock; // +0x04 T CAppHanteiYarare__vf6 0x4494F0 stores &frame.editorOnly_130 (frame+0x130); HitJudge_DefenderGuardsAttack reads guard mode at +4 of it, HitJudge_DefenderArmorAbsorbs armor mode at +8
 unsigned int count; // +0x08 T vf6 builds 0..6 boxes; vf4 0x4494D0 clears it; read by vf0 and the box walkers
 unsigned char unused_0C[4]; // +0x0C U no Yarare method and no Obj-base operand touches class +12
 struct Gof2BoxRect * box[6]; // +0x10 T vf6 stores view->boxes[hurtBoxIdx[i]]; CAppHanteiYarare__vf2 0x449460 reads box[a3]
 int slot[6]; // +0x28 T vf6 stores the frame slot number (0..5); HitJudge_CollectPairs reads it (1 << slot)
 unsigned int hitSlotMask; // +0x40 T HitJudge_CollectPairs |= 1 << slot for each hurt box that was hit (ancestors get the mask << 8); CAppHanteiYarare__vf3 0x43E3C0 clears it
};
struct Gof2ObjAttackBoxSet {
 void * vtable; // +0x00 T CAppHanteiKougeki vftable 0x573CB8
 unsigned int primaryCount; // +0x04 T see Gof2AttackBoxSetPrefix
 unsigned int secondaryCount; // +0x08 T see Gof2AttackBoxSetPrefix
 struct Gof2AtRecord * attackRecord; // +0x0C T CAppHanteiKougeki_BuildFromFrame 0x449250 (obj+1568)
 struct Gof2BoxRect * box[8]; // +0x10 T BuildFromFrame: view->boxes[frame.attackBoxIdx[i]] for each non -1 entry
 int slot[8]; // +0x30 T BuildFromFrame: frame slot number 0..7 of box[i]
 unsigned int hitLimitFlags; // +0x50 T BuildFromFrame copies frame.hitLimitFlags (+0x48) on ATKEV_BEGIN
 unsigned int hitLimitCount; // +0x54 T BuildFromFrame copies frame.hitLimitCount; sub_436280 compares it with +0x58
 unsigned int hitsLandedForLimit; // +0x58 I only sub_436280 reads it, nothing writes it
 int attackEndLatch; // +0x5C T vf5 0x449230 and vf6 0x43E2A0 clear it, no reader
 unsigned char unused_60[8]; // +0x60 U no Kougeki method and no Obj-base operand touches +0x60..+0x67
 int attackActive; // +0x68 T BuildFromFrame sets 1 on ATKEV_BEGIN, vf3 0x449220 clears it, no reader
 int boxJitterX; // +0x6C T vf7 0x43E320 Rng_Range(ControllerManage+1324); vf2 0x43E370 widens the rect
 int boxJitterY; // +0x70 T vf7 Rng_Range(ControllerManage+1328); vf2 widens the rect
 int moveHitCount; // +0x74 T -1 disabled; ++ per connected hit (HitJudge_ApplyHit), reset by Obj_ResetHanteiBoxObjects
 int counterHitSeen; // +0x78 T ObjHitFlags_RecordHit 0x43C210 sets 1 for variant 1
 Gof2ObjHitFlagBits hitFlags; // +0x7C T ObjHitFlags_RecordHit variant 0 (self 1, ancestors 2)
 Gof2ObjHitFlagBits counterHitFlags; // +0x80 T ObjHitFlags_RecordHit variant != 0
 Gof2ObjHitFlagBits guardStunHitFlags; // +0x84 T ObjHitFlags_RecordGuardStunHit 0x43C2A0 variant 0
 Gof2ObjHitFlagBits guardCrushHitFlags; // +0x88 T ObjHitFlags_RecordGuardStunHit variant != 0
 Gof2ObjHitFlagBits clashFlags; // +0x8C T ObjHitFlags_RecordClash 0x43C320
 Gof2ObjHitFlagBits guardedFlags; // +0x90 T ObjHitFlags_RecordGuarded 0x43C360
 unsigned int counterHitStanceMask; // +0x94 T victim stance mask partner of counterHitFlags (ancestors <<8)
 unsigned int hitStanceMask; // +0x98 T partner of hitFlags
 unsigned int guardStunHitStanceMask; // +0x9C T partner of guardStunHitFlags
 unsigned int guardCrushHitStanceMask; // +0xA0 T partner of guardCrushHitFlags
 unsigned int clashStanceMask; // +0xA4 T partner of clashFlags
 unsigned int guardedStanceMask; // +0xA8 T partner of guardedFlags
 int clashState; // +0xAC T ObjHitFlags_MarkAttackClash 0x43C420: 1 self, 2 ancestors
 int framesSinceHit; // +0xB0 T -1 at reset, 0 on hit/guard/clash, ++ per tick (Obj_TickTimersRecursive 0x49E120)
 int alreadyHitVictim[2]; // +0xB4 T one-hit-per-victim latch indexed side+team (HitJudge_ApplyHit, ObjScan_IsBlockableAttackFrame 0x432420)
};
