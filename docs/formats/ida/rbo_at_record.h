// RBO "AT" (attack data) record, HAN2RBO section 3, 120 bytes (0x78), indexed by frame record +200.
// Engine access: actor+1872 (RboAttackState.at, state = actor+1812) set by Actor_CollectAttackBoxes2 (0x440F40).
// Proof functions are in rbo_at_record.md. All values little-endian int32 unless noted.
#pragma once
#include <cstdint>
#include <cstddef>

enum RboHitClass : int32_t {  // +0x00; groups drive the victim reaction anim in Hit_SetVictimReactionAnim (0x444340)
  RBO_HC_FLINCH_0 = 0, RBO_HC_FLINCH_1 = 1, RBO_HC_FLINCH_2 = 2,
  RBO_HC_MEDIUM_3 = 3, RBO_HC_MEDIUM_4 = 4, RBO_HC_MEDIUM_5 = 5,
  RBO_HC_HEAVY_6 = 6, RBO_HC_HEAVY_7 = 7, RBO_HC_HEAVY_8 = 8,
  RBO_HC_SWEEP_9 = 9,
  RBO_HC_LAUNCH_10 = 10, RBO_HC_LAUNCH_11 = 11, RBO_HC_LAUNCH_12 = 12, RBO_HC_LAUNCH_13 = 13,
  RBO_HC_MEDIUM_14 = 14, RBO_HC_LAUNCH_15 = 15, RBO_HC_FLINCH_16 = 16,
  RBO_HC_SPECIAL_NOREACT = 17
};
enum RboHitSfxSet : int32_t { RBO_HITSFX_0, RBO_HITSFX_1, RBO_HITSFX_2, RBO_HITSFX_3, RBO_HITSFX_4, RBO_HITSFX_5, RBO_HITSFX_6 };
enum RboAtGateMode : int32_t { RBO_GATE_NORMAL = 0, RBO_GATE_1 = 1, RBO_GATE_2 = 2, RBO_GATE_3 = 3, RBO_GATE_ONLY_VS_SPECIAL_GUARD = 4 };
enum RboAtKnockDirMode : int32_t { RBO_KDIR_ATTACKER_FACING = 0, RBO_KDIR_REVERSED = 1, RBO_KDIR_BY_POSITION = 2, RBO_KDIR_BY_POSITION_REVERSED = 3, RBO_KDIR_NONE = 4 };
enum RboAtFacingMode : int32_t { RBO_FACE_BY_POSITION = 0, RBO_FACE_BY_POSITION_REVERSED = 1, RBO_FACE_ATTACKER = 2, RBO_FACE_ATTACKER_REVERSED = 3, RBO_FACE_VICTIM = 4 };
enum RboAtElementSource : int32_t { RBO_ELEM_ATTACKER_PRIMARY = 0, RBO_ELEM_ATTACKER_SECONDARY = 1, RBO_ELEM_FIXED = 2 };
enum RboAtDamageMode : int32_t { RBO_DMGMODE_NORMAL = 0, RBO_DMGMODE_HEAL = 1, RBO_DMGMODE_STATUS_DOT = 3, RBO_DMGMODE_4 = 4 };
enum RboAtDamageType : int32_t { RBO_DT_0 = 0, RBO_DT_1 = 1, RBO_DT_2 = 2, RBO_DT_3 = 3, RBO_DT_6 = 6, RBO_DT_7 = 7, RBO_DT_MAGIC = 8, RBO_DT_TRUE = 10 };
enum RboGuardOutcome : int32_t { RBO_GUARD_BLOCKED = 0, RBO_GUARD_HIT = 1, RBO_GUARD_JUST = 2, RBO_GUARD_3 = 3, RBO_GUARD_INVULN = 4, RBO_GUARD_INVULN_B = 5 };
enum RboAtFlags76 : uint32_t {  // +0x4C
  RBO_AT76_PLAY_EXTRA_SFX = 0x1, RBO_AT76_UNREAD_0002 = 0x2, RBO_AT76_HIT_TEAMMATES = 0x10, RBO_AT76_CAN_HIT_SELF = 0x20,
  RBO_AT76_HIT_BOTH_TEAMS = 0x40, RBO_AT76_SELF_ONLY_TARGET = 0x80, RBO_AT76_UNREAD_0100 = 0x100,
  RBO_AT76_PUSHBACK_GATE_INVERT = 0x400, RBO_AT76_UNREAD_2000 = 0x2000, RBO_AT76_SUPPRESS_HIT_SFX = 0x10000,
  RBO_AT76_RANGED_ATTACK = 0x100000
};
enum RboAtFlags80 : uint32_t {  // +0x50
  RBO_AT80_UNREAD_0001 = 0x1, RBO_AT80_NON_LETHAL = 0x2, RBO_AT80_UNREAD_0010 = 0x10, RBO_AT80_UNREAD_0020 = 0x20,
  RBO_AT80_UNREAD_0200 = 0x200, RBO_AT80_NO_JUST_GUARD = 0x1000, RBO_AT80_UNEVADABLE = 0x2000,
  RBO_AT80_NO_DAMAGE_POPUP = 0x1000000, RBO_AT80_UNEVADABLE0000 = 0x20000000, RBO_AT80_UNREAD_40000000 = 0x40000000
};
enum RboHitSkillId : int32_t {  // +0x70; value -> handler (HitSkillNN_SlotXX) -> script slot run via Skill_RunScriptSlot(0, slot)
  RBO_HITSKILL_NONE = 0,
  RBO_HITSKILL_09 = 9,   // 0x458E80 slot 0x0E (+ slot 0x05 -> status 3 on victim)
  RBO_HITSKILL_10 = 10,  // 0x458FF0 slot 0x0F
  RBO_HITSKILL_14 = 14,  // 0x459260 slot 0x13
  RBO_HITSKILL_17 = 17,  // 0x4593E0 slot 0x16
  RBO_HITSKILL_18 = 18,  // 0x459460 slot 0x17
  RBO_HITSKILL_22 = 22,  // 0x459B20 slot 0x1F
  RBO_HITSKILL_25 = 25,  // 0x459890 slot 0x22 (status 24)
  RBO_HITSKILL_28 = 28,  // 0x459C80 slot 0x25
  RBO_HITSKILL_31 = 31,  // 0x459DE0 slot 0x28 (status 30)
  RBO_HITSKILL_34 = 34,  // 0x45A690 slot 0x2B
  RBO_HITSKILL_43 = 43,  // 0x45A980 slot 0x34
  RBO_HITSKILL_44 = 44,  // 0x459FB0 slots 0x35/0x36 (status 43)
  RBO_HITSKILL_47 = 47,  // 0x45A110 slots 0x39/0x3A (status 46, 600 frames)
  RBO_HITSKILL_52 = 52,  // 0x45A450 slots 0x3F/0x40 (status 51)
  RBO_HITSKILL_54 = 54,  // 0x45AAA0 slot 0x42
  RBO_HITSKILL_57 = 57,  // 0x45B340 slot 0x45 (sign flip by 0x45B0A0)
  RBO_HITSKILL_69 = 69,  // 0x45BA00 slot 0x52 (status 68)
  RBO_HITSKILL_71 = 71,  // 0x45BCA0 slot 0x54 (status 70)
  RBO_HITSKILL_73_NOHANDLER = 73, RBO_HITSKILL_75_NOHANDLER = 75  // present in data, fall to default (no handler)
};

#pragma pack(push, 1)
struct RboAtRecord {
  RboHitClass        hit_class;               // +0x00
  RboHitSfxSet       hit_sfx_set;             // +0x04
  int32_t            hit_spark_kind;          // +0x08
  RboAtGateMode      hit_gate_mode;           // +0x0C
  int32_t            base_power;              // +0x10
  int32_t            unused_14;               // +0x14
  int32_t            guard_damage;            // +0x18
  int32_t            status_duration;         // +0x1C
  int32_t            victim_shake_mode;       // +0x20
  int32_t            reserved_flag_24;        // +0x24
  int32_t            sound_bank_select;       // +0x28
  int32_t            hit_sound_id;            // +0x2C
  int32_t            hit_sound_random_range;  // +0x30
  RboAtKnockDirMode  knock_dir_mode;          // +0x34
  RboAtFacingMode    victim_facing_mode;      // +0x38
  RboAtElementSource element_source;          // +0x3C
  int32_t            fixed_element;           // +0x40
  RboAtDamageType    damage_type;             // +0x44
  RboAtDamageMode    damage_mode;             // +0x48
  RboAtFlags76       target_fx_flags;         // +0x4C
  RboAtFlags80       damage_rule_flags;       // +0x50
  int32_t            hit_script_enable;       // +0x54
  int32_t            hit_script_type;         // +0x58
  int32_t            hit_script_id;           // +0x5C
  int32_t            unused_60;               // +0x60
  int32_t            unused_64;               // +0x64
  int32_t            unused_68;               // +0x68
  uint16_t           skill_param_a;           // +0x6C
  uint16_t           skill_param_b;           // +0x6E
  RboHitSkillId      hit_skill_id;            // +0x70
  int32_t            unused_74;               // +0x74
};
#pragma pack(pop)

static_assert(sizeof(RboAtRecord) == 0x78, "RboAtRecord size");
#define RBO_AT_CHK(f, o) static_assert(offsetof(RboAtRecord, f) == (o), "RboAtRecord::" #f)
RBO_AT_CHK(hit_class, 0x00); RBO_AT_CHK(hit_sfx_set, 0x04); RBO_AT_CHK(hit_spark_kind, 0x08); RBO_AT_CHK(hit_gate_mode, 0x0C);
RBO_AT_CHK(base_power, 0x10); RBO_AT_CHK(unused_14, 0x14); RBO_AT_CHK(guard_damage, 0x18); RBO_AT_CHK(status_duration, 0x1C);
RBO_AT_CHK(victim_shake_mode, 0x20); RBO_AT_CHK(reserved_flag_24, 0x24); RBO_AT_CHK(sound_bank_select, 0x28);
RBO_AT_CHK(hit_sound_id, 0x2C); RBO_AT_CHK(hit_sound_random_range, 0x30); RBO_AT_CHK(knock_dir_mode, 0x34);
RBO_AT_CHK(victim_facing_mode, 0x38); RBO_AT_CHK(element_source, 0x3C); RBO_AT_CHK(fixed_element, 0x40);
RBO_AT_CHK(damage_type, 0x44); RBO_AT_CHK(damage_mode, 0x48); RBO_AT_CHK(target_fx_flags, 0x4C);
RBO_AT_CHK(damage_rule_flags, 0x50); RBO_AT_CHK(hit_script_enable, 0x54); RBO_AT_CHK(hit_script_type, 0x58);
RBO_AT_CHK(hit_script_id, 0x5C); RBO_AT_CHK(unused_60, 0x60); RBO_AT_CHK(unused_64, 0x64); RBO_AT_CHK(unused_68, 0x68);
RBO_AT_CHK(skill_param_a, 0x6C); RBO_AT_CHK(skill_param_b, 0x6E); RBO_AT_CHK(hit_skill_id, 0x70); RBO_AT_CHK(unused_74, 0x74);
#undef RBO_AT_CHK
