// RboActor part 3 (+0x500..+0x77F) nested types, IDA-parsable (idc.parse_decls). Evidence: docs/formats/ida/actor_part3.md
struct RboActor;
enum RboP3ContactKind : unsigned int { CK_HIT=1, CK_GUARD=2, CK_SCRIPTED=4, CK_CHILD_HIT=0x100, CK_CHILD_GUARD=0x200, CK_CHILD_SCRIPTED=0x400, CK_STRUCK_THIS_FRAME=0x10000 };
enum RboP3BufferedJumpKind : int { BJ_NONE=-1, BJ_DIRECT=0, BJ_RULE_INDEX=1, BJ_BUTTON_DIRECT=2, BJ_KIND3=3, BJ_FORCED=4 };
enum RboP3GateFlags : unsigned int { GATE_FRAME_LATCH=1, GATE_COUNTED=2, GATE_RELATCH_AFTER_LIMIT=4 };
enum RboP3ArmorDeathFlags : unsigned int { ADF_KEEP_ARMOR_ON_LETHAL=1 };
struct RboP3BufferedJump {
 RboP3BufferedJumpKind kind;           // +0x00 sub_41E020/41FA50/41FFA0 write, sub_41E500 consumes; -1 = none
 int patternOrRule;                    // +0x04 target pattern (kinds 0/2/3/4, low 16 bits) or rule index (kind 1, %1000 into rule table)
 int frame;                            // +0x08 target frame (low 16 bits)
 int flip;                             // +0x0C target flip byte (-1 = unchanged), low 8 bits
};
struct RboP3RuleNode {
 struct RboP3RuleNode * next;          // +0x00 Actor_LinkTransitionRuleEvent 0x41F7F0 (*node = head)
 int * eventRecord;                    // +0x04 node[1] = script event record
};
struct RboP3EnterNode {
 struct RboP3EnterNode * next;         // +0x00 Actor_LinkFrameEnterEvent 0x41D9B0 (*node = head)
 unsigned char unused_04[4];           // +0x04 node[1] never written/read (Actor_RunFrameEnterActions reads i[2] and *i only)
 int * eventRecord;                    // +0x08 node[2] = script event record
};
struct RboP3RuleHead {
 struct RboP3RuleNode * head;          // +0x00 list head
 unsigned char unused_04[4];           // +0x04 slots are 8 bytes apart, only the head dword is touched
};
struct RboP3TransitionHeads {
 struct RboP3RuleHead inputRules;      // +0x00 category 0 (RBO_TR_CAT_INPUT_RULES) Actor_RunInputTransitionRules 0x420F10
 struct RboP3RuleHead overlapRules;     // +0x08 category 2 (SPAWN_EVENTS): event 14 run by sub_445130 while the kasanari box is active (sub_41F560)
 struct RboP3RuleHead conditionalJumps;// +0x10 category 1 Actor_RunConditionalJumpList 0x420460
 struct RboP3RuleHead tickRules;       // +0x18 category 3 sub_41E500 (cases 0x14/0x32/0x34/0xC4/0xC6/0xC7/0xCA)
 struct RboP3RuleHead forcedJumps;     // +0x20 category 4 Actor_RunForcedJumpRules 0x41F780, Actor_RunTimedJumpRules 0x420520
 struct RboP3RuleHead endRules;        // +0x28 category 7 Actor_RunEndTransitionRules 0x420DC0
};
struct RboP3EnterHead {
 struct RboP3EnterNode * head;         // +0x00 list head
 unsigned char unused_04[8];           // +0x04 slots are 12 bytes apart, only the head dword is touched
};
struct RboP3FrameEnterHeads {
 struct RboP3EnterHead actions;        // +0x00 category 0 Actor_RunFrameEnterActions 0x41D8E0 (runs on frame entry)
 struct RboP3EnterHead onHitAttackerMods; // +0x0C category 1, walked by Combat_ResolveDamageHit (events 252/253) for the attacker
 struct RboP3EnterHead chanceActions;  // +0x18 category 4 Actor_RunChanceFrameEnterActions 0x41D950 (event 242)
};
struct RboP3KillerRecord {
 int trackKiller;                      // +0x00 set by frame-enter op 24 / Actor_Respawn; gate in Actor_RecordKillerOnDeath 0x442150
 struct RboActor * killer;             // +0x04 attacker that dealt the lethal hit (Actor_RecordKillerOnDeath, read sub_43C670)
 int deathHandled;                     // +0x08 0 when recorded, 1 after sub_4419A0 cleanup; passed to CallItemCreate script arg 9 (sub_43C750)
 int killerRecorded;                   // +0x0C 1 when trackKiller was set at death (sub_43F9F0 gate)
 int killerPlayerId;                   // +0x10 killer->playerId (+0x3C); sub_43F9F0 credits only when 0
 int killerCharSlot;                   // +0x14 killer +0x40 value; arg to sub_431050 (kill credit)
};
struct RboP3ArmorState {
 int armorActive;                      // +0x00 1 = incoming hits do not interrupt (Hit_SetVictimReactionAnim), -1/0 off; sub_441800 derives from timer
 int chancePercent;                    // +0x04 frame-enter op 0 sets; sub_443890 rolls Rng(100) < value; -2 per success (Combat_ResolveDamageHit)
 int strainCount;                      // +0x08 ++ on strain-armor success; sub_4438C0 lowers the success chance by 2 per count
 int timerFrames;                      // +0x0C frame-enter op 1 / sub_421B40 / sub_4591A0 set; sub_440550 decrements to 0
};
struct RboP3AttackHitState {
 int attackActive;                     // +0x00 Actor_SetAttackActive 0x441020 (*a2 = a3), set 1 when attack boxes are collected
 RboP3ContactKind contactKinds;        // +0x04 sub_4198E0 ORs kind bits; tested in Actor_InputMaskMatches 0x41DBB0 (mask 0x101/0x202/0x404)
 unsigned int contactStance;           // +0x08 victim stanceClass per kind (nibble0 hit, byte1 guard, nibble4 scripted); Actor_InputMaskMatches
 unsigned int hitMemory[4];            // +0x0C per victim playerId (0..3), bit (1<<slot): already struck; cleared by sub_440FE0
 int hitCount;                         // +0x1C ++ per connecting contact (Combat_CountContactHit 0x4449D0), read by sub_443770/443830
};
struct RboP3MultiHitGate {
 RboP3GateFlags modeFlags;             // +0x00 copied from AT record +0x48 by HitLimiter_SetFlagsAndCount 0x440F00
 int total;                            // +0x04 copied from AT record +0x4C (hit limit)
 int cur;                              // +0x08 hits counted (Hit_MultiHitGateAdvance 0x4429E0)
 int extra;                            // +0x0C secondary counter (mode 0 / mode 2 paths)
 int latched;                          // +0x10 Hit_MultiHitGateLatch 0x442960: 1 once latched
 int latchFrame;                       // +0x14 frame counter (sub_431420) captured when latched
};
struct RboP3HurtTimers {
 int graceTimerA;                      // +0x00 frame-enter op 5 sets; sub_440550 decrements; cleared with intangibleTimer in sub_43FD50 (no reader found)
 int intangibleTimer;                  // +0x04 0 = hittable; >0 frames (30 after hit sub_444DF0, 180 sub_43DFF0, 240 respawn); -2 = permanent; scans skip when != 0
 int hitNullifyTimer;                  // +0x08 Hit_ClassifyGuardOutcome 0x443330: non-zero => hit resolves as INVULN (-2 => INVULN_B); frame-enter op 6
};
