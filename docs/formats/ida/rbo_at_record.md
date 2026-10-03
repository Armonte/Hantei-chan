# RBO AT (attack data) record, 120 bytes (struct RboAtRecord)

HAN2RBO section 3, stride 0x78, indexed by frame record +200. `Actor_CollectAttackBoxes2` (0x440F40) sets
`actor+1872 = patternArea.at + 120*frame[+200]`; actor+1812 is the attack state (`RboAttackState`, `.at` at +0x3C).
IDA types created in rbo.exe: `RboAtRecord` (0x78), `RboAttackState` (partial), `RboMultiHitGate`, enums `RboHitClass`, `RboHitSfxSet`,
`RboAtGateMode`, `RboAtKnockDirMode`, `RboAtFacingMode`, `RboAtElementSource`, `RboAtDamageMode`, `RboAtDamageType`, `RboGuardOutcome`,
`RboHitSkillId`, bitfield enums `RboAtFlags76`, `RboAtFlags80`. C++ header: `rbo_at_record.h`.

Value statistics: measured over every section-3 record yielded by `han2_files('rbo')` (8612 records, 1228 distinct). Confidence: T = traced in
a decompiled consumer, I = inferred (usage seen but semantics by behaviour/data only).

## Field map

| Off | Size | Name | Type | Proof | Conf | Behaviour |
|---|---|---|---|---|---|---|
| 0x00 | 4 | hit_class | RboHitClass 0..17 | Hit_SetVictimReactionAnim 0x444340; Hit_SetVictimGuardAnim 0x443290; Combat_ResolveDamageHit 0x444530 | T | selects the victim reaction animation group (23/24/25/26/7) and which hit class is stored in victim+1948; 17 = no reaction anim (victim+1940=-1) and special in 444530 (only if guard gauge<=0) and 444340 |
| 0x04 | 4 | hit_sfx_set | 0..6 | Hit_PlayHitSfx 0x4442C0, Hit_PlayGuardHitSfx 0x443230 | T | index into sound tables dword_48A4B0 / dword_48A4CC / dword_48A4E8 / dword_48A504; also scales hit-shake duration (dword_48A4CC) in Hit_ApplyDamageAndSkillMods |
| 0x08 | 4 | hit_spark_kind | 0..14 | Combat_ResolveDamageHit 0x444530 | I | `!= 14` spawns the hit flash (sub_43C410); 14 = none. Other values only distinguished by data, no other reader |
| 0x0C | 4 | hit_gate_mode | RboAtGateMode 0..4 | Combat_ResolveGuardContacts 0x4433E0 | T/I | value 4: contact only counts when guard outcome is JUST or INVULN; others connect normally |
| 0x10 | 4 | base_power | int32 (-9999..5000) | Hit_ApplyDamageAndSkillMods 0x443DA0, Hit_ComputeFinalDamage 0x443A00 | T | base damage fed into the damage formula; negated when damage_mode==HEAL; also status magnitude when damage_mode==3 |
| 0x14 | 4 | unused_14 | int32 | no reader | T(unused) | zero in all records, no reader |
| 0x18 | 4 | guard_damage | int32 | Hit_GuardDamageForOutcome 0x442F90, Hit_CalcGuardDamage 0x4431E0 | T | damage to the victim guard gauge (+2040 block) ; x3 on outcome HIT, 0 for just/invuln; x2/3 if attacker+40 & 0x40; gauge<=0 -> outcome 3 |
| 0x1C | 4 | status_duration | int32 frames | Hit_ApplyDamageAndSkillMods 0x443DA0 -> sub_45B620 | T | duration of the status 1003/58 applied when damage_mode==3 |
| 0x20 | 4 | victim_shake_mode | 0..4 | 0x443DA0 | T(==1) | `==1` starts the victim shake object (Actor_StartHitShake 0x441E40, 15*(dword_48A4CC[sfx]+1)/10); values 2-4 are never tested |
| 0x24 | 4 | reserved_flag_24 | 0/1 | no reader | I | nonzero in 24 records (so not "unused"), but no reader found in any AT consumer |
| 0x28 | 4 | sound_bank_select | 0/1 | Hit_PlaySoundFromAt 0x442C60 | T | 0 = global bank base 201, nonzero = character-script bank (attacker+2028 +8) |
| 0x2C | 4 | hit_sound_id | int32 (-1 = none) | 0x444530 (`>=0` gate) -> 0x442C60 | T | sound id offset within the bank |
| 0x30 | 4 | hit_sound_random_range | int32 | 0x442C60 | T | if nonzero adds Rng_Range(n) to hit_sound_id |
| 0x34 | 4 | knock_dir_mode | RboAtKnockDirMode 0..4 | Hit_SetKnockbackAndFacing 0x442CE0 | T | victimHitInfo+40 (knock direction sign): 0 attacker facing, 1 reversed, 2/3 by relative x (3 reversed), 4 none |
| 0x38 | 4 | victim_facing_mode | RboAtFacingMode 0..4 | 0x442CE0 | T | victimHitInfo+44 (facing sign): 0/1 by position (1 reversed), 2 attacker facing, 3 reversed, 4 victim facing; only when victim+88==0 |
| 0x3C | 4 | element_source | RboAtElementSource 0..2 | Hit_PickElementIndex 0x443D70 | T | 0 -> attacker stats+108, 1 -> stats+112, 2 -> fixed_element |
| 0x40 | 4 | fixed_element | 0..5 | 0x443D70 | T | literal element index used when element_source==2 (nonzero only then) |
| 0x44 | 4 | damage_type | RboAtDamageType 0..10 | Hit_ComputeFinalDamage 0x443A00 | T/I | 10 = true damage (no variance, uses victim stats 136/140); 8 = magic stat set (attacker stats 100..108); else physical set (88..96); also column index of the resistance table dword_2659128 |
| 0x48 | 4 | damage_mode | RboAtDamageMode | 0x443DA0 | T | 1 = base_power negated (heal; result<0 => outcome 3); 3 = apply timed status instead of damage; 0/4 normal |
| 0x4C | 4 | target_fx_flags | RboAtFlags76 | see flag table | T | |
| 0x50 | 4 | damage_rule_flags | RboAtFlags80 | see flag table | T | |
| 0x54 | 4 | hit_script_enable | 0/1 | 0x444530 | T | gate for the on-hit script call |
| 0x58 | 4 | hit_script_type | 0/2 | 0x444530 -> sub_418790 | T | script type index (first index of sub_425100 lookup in the character script table actor+2028+28308) |
| 0x5C | 4 | hit_script_id | int32 | 0x444530 (`!=0` gate) | T | script id within the type (e.g. 1105, 1507) |
| 0x60 | 4 | unused_60 | int32 | no reader | T(unused) | zero in all records |
| 0x64 | 4 | unused_64 | int32 | no reader | T(unused) | zero in all records |
| 0x68 | 4 | unused_68 | int32 | no reader | T(unused) | zero in all records |
| 0x6C | 2 | skill_param_a | u16 | 0x443DA0 -> HitSkill handlers | T | script argument 3 (LOWORD of dword +108) |
| 0x6E | 2 | skill_param_b | u16 | 0x443DA0 -> HitSkill handlers | T | script argument 2 (HIWORD) |
| 0x70 | 4 | hit_skill_id | RboHitSkillId | 0x443DA0 switch via byte_444254 | T | on-hit skill hook, see table |
| 0x74 | 4 | unused_74 | int32 | no reader | T(unused) | zero in all records |

Unused-field search method: AT access only happens through actor+1872 (3 functions have the 0x750 displacement: 0x41E810 (passes pointer to a
script only), 0x440F40, 0x4433E0) and through the attack state at actor+1812 (all users of the 0x714 displacement: 0x41EED0, 0x43EDC0, 0x441060,
0x441850, 0x441A00, 0x4433E0, 0x444530, 0x4448D0, 0x4449D0, 0x444B10). The state pointer is only passed to the Hit_* / Combat_* functions in
0x442000-0x445200 and 0x45B0E0 (which builds a synthetic 120-byte AT on the stack, v11[], for status damage ticks: class 2, sfx 2, +60=2, +64=7, +68=10, +76=0x10000);
every one of them was decompiled and read (functions in 0x440C00-0x445200 and 0x458000-0x45C000 were read in full). Only `imul 120` is in 0x440F40 so the AT is never copied elsewhere.

## Flag words

RboAtFlags76 (+0x4C):
| Bit | Name | Proof |
|---|---|---|
| 0x1 | PLAY_EXTRA_SFX | 0x444530 -> Hit_PlayExtraImpactSfx 0x442C00 (sound dword_48A558) |
| 0x10 | HIT_TEAMMATES | Hit_TeamTargetTest 0x443960: with 0x10 only same-team victims (unless 0x40); without it same-team is rejected |
| 0x20 | CAN_HIT_SELF | 0x443960 and 0x4433E0: self-hit allowed |
| 0x40 | HIT_BOTH_TEAMS | 0x443960 (with 0x10) |
| 0x400 | PUSHBACK_GATE_INVERT | Hit_PushbackAllowed 0x4426D0: wall pushback only if (attacker+40 & 0xA0)==0x20 xor flag clear |
| 0x10000 | SUPPRESS_HIT_SFX | 0x4442C0 / 0x443230 skip the attacker hit sfx |
| 0x2, 0x80, 0x100, 0x2000, 0x100000 | UNREAD_* | present in data (5, 1, 3, 1, 46 distinct records), no reader found |

RboAtFlags80 (+0x50):
| Bit | Name | Proof |
|---|---|---|
| 0x2 | NON_LETHAL | Hit_ApplyHpDamage 0x443CF0 (HP clamps to 1 instead of death); 0x444340 (class 17 + this = early exit) |
| 0x1000 | NO_JUST_GUARD | Hit_ClassifyGuardOutcome 0x443330 skips the just-guard window |
| 0x1000000 | NO_DAMAGE_POPUP | 0x443DA0 (`v17`): no damage number / hit-gauge FX / HP bar update |
| 0x1, 0x10, 0x20, 0x200, 0x2000, 0x20000000, 0x40000000 | UNREAD_* | present in data, no reader found |

## Enum tables

RboHitClass (+0x00), grouped by 0x444340 (victim posture a3+2072: 1 stand, 2 crouch/air, 4 forced): group A {0,1,2,16} -> anim 23 (stand) / 25;
group B {3,4,5,14} -> 24 (stand) / 25; group C {6,7,8,10,11,12,13,15} -> 26 (knock-down/launch); 9 -> 7/26 (sweep); 17 -> special, no reaction, kept in victim+1948.
Names `RBO_HC_FLINCH/MEDIUM/HEAVY/SWEEP/LAUNCH_n` are group labels, not original names (I).

RboGuardOutcome (return of 0x443330): 0 blocked, 1 normal hit, 2 just-guard window, 3 guard broken (set by 0x4431B0), 4 invulnerable (victim+1900 != 0), 5 same with -2.

RboAtDamageMode: 0 normal, 1 heal (negate base_power), 3 timed status (Hit_ApplyDamageAndSkillMods -> sub_45B620: status 58 with value base_power for status_duration), 4 treated as normal (8 records).

RboHitSkillId (+0x70): value-9 indexes the byte table byte_444254[0x3F]; table[i] -> case (0..17), 18 = no handler (also any value >= 72 and 0). Every handler
runs a skill script slot via Skill_RunScriptSlot (0x4584E0) with args (attacker id, base_power, skill_param_b, skill_param_a) and takes script result 63 as the new
damage, and some apply a status/reaction to the victim:

| Value | Case | Function | Script slot | Extra |
|---|---|---|---|---|
| 9 | 0 | HitSkill09_Slot0E 0x458E80 | 0x0E | slot 5 may apply status 3 (sub_456F70) |
| 10 | 1 | HitSkill10_Slot0F 0x458FF0 | 0x0F | |
| 14 | 2 | HitSkill14_Slot13 0x459260 | 0x13 | |
| 17 | 3 | HitSkill17_Slot16 0x4593E0 | 0x16 | |
| 18 | 4 | HitSkill18_Slot17 0x459460 | 0x17 | |
| 22 | 5 | HitSkill22_Slot1F 0x459B20 | 0x1F | |
| 25 | 6 | HitSkill25_Slot22 0x459890 | 0x22 | status 24 (sub_457050) |
| 28 | 7 | HitSkill28_Slot25 0x459C80 | 0x25 | |
| 31 | 8 | HitSkill31_Slot28 0x459DE0 | 0x28 | status 30 |
| 34 | 9 | HitSkill34_Slot2B 0x45A690 | 0x2B | |
| 43 | 10 | HitSkill43_Slot34 0x45A980 | 0x34 | |
| 44 | 11 | HitSkill44_Slot35 0x459FB0 | 0x35,0x36 | status 43 |
| 47 | 12 | HitSkill47_Slot39 0x45A110 | 0x39,0x3A | status 46 (600), also zeroes secondary damage |
| 52 | 13 | HitSkill52_Slot3F 0x45A450 | 0x3F,0x40 | status 51 |
| 54 | 14 | HitSkill54_Slot42 0x45AAA0 | 0x42 | |
| 57 | 15 | HitSkill57_Slot45 0x45B340 | 0x45 | sign flipped by 0x45B0A0; sets flag 0x10 on the damage call |
| 69 | 16 | HitSkill69_Slot52 0x45BA00 | 0x52 | status 68, no damage change |
| 71 | 17 | HitSkill71_Slot54 0x45BCA0 | 0x54 | status 70 |
| other (0, 73, 75, ...) | 18 | none | | plain damage |

Other enums: RboAtKnockDirMode / RboAtFacingMode / RboAtElementSource / RboAtDamageType values are in the field table; damage_type observed values 0,1,2,3,6,7,8,10; element
index tables are dword_265735C (attacker element x victim race x victim size, 11 x 11 x 11) and dword_2659128.

## Renamed functions (rbo.exe)

Combat_ResolveGuardContacts 0x4433E0; Combat_ResolveDamageHit 0x444530; Combat_ScanDamageHits_OwnerList 0x4448D0 / _AllLists 0x444990;
Combat_CountContactHit 0x4449D0; Combat_ScanContactHits_OwnerList 0x444B10 / _AllLists 0x444BD0; Hit_ApplyDamageAndSkillMods 0x443DA0; Hit_ComputeFinalDamage 0x443A00;
Hit_ApplyHpDamage 0x443CF0; Hit_PickElementIndex 0x443D70; Hit_SetKnockbackAndFacing 0x442CE0; Hit_GuardDamageForOutcome 0x442F90;
Hit_ApplyGuardGaugeDamage 0x4431B0; Hit_CalcGuardDamage 0x4431E0; Hit_ClassifyGuardOutcome 0x443330; Hit_RegisterHitMask 0x4430D0; Hit_RecordEvadedHit 0x442F20;
Hit_RollEvade 0x442E50; Hit_EvadeBonusFromStatus18 0x4595C0; Hit_PlayGuardHitSfx 0x443230; Hit_PlayHitSfx 0x4442C0; Hit_SetVictimGuardAnim 0x443290;
Hit_SetVictimReactionAnim 0x444340; Hit_ApplyWallPushback 0x442710; Hit_PushbackAllowed 0x4426D0; Hit_TeamTargetTest 0x443960; Hit_MarkTargetHit 0x4439C0;
Hit_PlaySoundFromAt 0x442C60; Skill_RunScriptSlot 0x4584E0; Hit_ApplyStatusDamageSyntheticAt 0x45B0E0; Hit_MultiHitGateLatch 0x442960 / _Ready 0x4429A0 / _Advance 0x4429E0;
Actor_StartHitShake 0x441E40; Actor_RecordKillerOnDeath 0x442150; Hit_PlayHitVoiceByOutcome 0x442C20; Hit_PlayExtraImpactSfx 0x442C00; the 18 HitSkillNN_SlotXX handlers above.
