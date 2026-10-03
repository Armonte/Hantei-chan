// Exported from the RBO IDB (idc type library) by tools/ida/export_decls.py logic. Evidence per field: rbo_at_record.md / rbo_scripts_pat.md.
enum RboHitClass : unsigned int { RBO_HC_FLINCH_0=0x0, RBO_HC_FLINCH_1=0x1, RBO_HC_FLINCH_2=0x2, RBO_HC_MEDIUM_3=0x3, RBO_HC_MEDIUM_4=0x4, RBO_HC_MEDIUM_5=0x5, RBO_HC_HEAVY_6=0x6, RBO_HC_HEAVY_7=0x7, RBO_HC_HEAVY_8=0x8, RBO_HC_SWEEP_9=0x9, RBO_HC_LAUNCH_10=0xA, RBO_HC_LAUNCH_11=0xB, RBO_HC_LAUNCH_12=0xC, RBO_HC_LAUNCH_13=0xD, RBO_HC_MEDIUM_14=0xE, RBO_HC_LAUNCH_15=0xF, RBO_HC_FLINCH_16=0x10, RBO_HC_SPECIAL_NOREACT=0x11 };
enum RboHitSfxSet : unsigned int { RBO_HITSFX_0=0x0, RBO_HITSFX_1=0x1, RBO_HITSFX_2=0x2, RBO_HITSFX_3=0x3, RBO_HITSFX_4=0x4, RBO_HITSFX_5=0x5, RBO_HITSFX_6=0x6 };
enum RboAtGateMode : unsigned int { RBO_GATE_NORMAL=0x0, RBO_GATE_1=0x1, RBO_GATE_2=0x2, RBO_GATE_3=0x3, RBO_GATE_ONLY_VS_SPECIAL_GUARD=0x4 };
enum RboAtKnockDirMode : unsigned int { RBO_KDIR_ATTACKER_FACING=0x0, RBO_KDIR_REVERSED=0x1, RBO_KDIR_BY_POSITION=0x2, RBO_KDIR_BY_POSITION_REVERSED=0x3, RBO_KDIR_NONE=0x4 };
enum RboAtFacingMode : unsigned int { RBO_FACE_BY_POSITION=0x0, RBO_FACE_BY_POSITION_REVERSED=0x1, RBO_FACE_ATTACKER=0x2, RBO_FACE_ATTACKER_REVERSED=0x3, RBO_FACE_VICTIM=0x4 };
enum RboAtElementSource : unsigned int { RBO_ELEM_ATTACKER_PRIMARY=0x0, RBO_ELEM_ATTACKER_SECONDARY=0x1, RBO_ELEM_FIXED=0x2 };
enum RboAtDamageMode : unsigned int { RBO_DMGMODE_NORMAL=0x0, RBO_DMGMODE_HEAL=0x1, RBO_DMGMODE_STATUS_DOT=0x3, RBO_DMGMODE_4=0x4 };
enum RboAtDamageType : unsigned int { RBO_DT_0=0x0, RBO_DT_1=0x1, RBO_DT_2=0x2, RBO_DT_3=0x3, RBO_DT_6=0x6, RBO_DT_7=0x7, RBO_DT_MAGIC=0x8, RBO_DT_TRUE=0xA };
enum RboGuardOutcome : unsigned int { RBO_GUARD_BLOCKED=0x0, RBO_GUARD_HIT=0x1, RBO_GUARD_JUST=0x2, RBO_GUARD_3=0x3, RBO_GUARD_INVULN=0x4, RBO_GUARD_INVULN_B=0x5 };
enum RboAtFlags76 : unsigned int { RBO_AT76_PLAY_EXTRA_SFX=0x1, RBO_AT76_UNREAD_0002=0x2, RBO_AT76_HIT_TEAMMATES=0x10, RBO_AT76_CAN_HIT_SELF=0x20, RBO_AT76_HIT_BOTH_TEAMS=0x40, RBO_AT76_SELF_ONLY_TARGET=0x80, RBO_AT76_UNREAD_0100=0x100, RBO_AT76_PUSHBACK_GATE_INVERT=0x400, RBO_AT76_UNREAD_2000=0x2000, RBO_AT76_SUPPRESS_HIT_SFX=0x10000, RBO_AT76_RANGED_ATTACK=0x100000 };
enum RboAtFlags80 : unsigned int { RBO_AT80_UNREAD_0001=0x1, RBO_AT80_NON_LETHAL=0x2, RBO_AT80_UNREAD_0010=0x10, RBO_AT80_UNREAD_0020=0x20, RBO_AT80_UNREAD_0200=0x200, RBO_AT80_NO_JUST_GUARD=0x1000, RBO_AT80_UNEVADABLE=0x2000, RBO_AT80_NO_DAMAGE_POPUP=0x1000000, RBO_AT80_UNREAD_20000000=0x20000000, RBO_AT80_UNREAD_40000000=0x40000000 };
enum RboHitSkillId : unsigned int { RBO_HITSKILL_NONE=0x0, RBO_HITSKILL_09=0x9, RBO_HITSKILL_10=0xA, RBO_HITSKILL_14=0xE, RBO_HITSKILL_17=0x11, RBO_HITSKILL_18=0x12, RBO_HITSKILL_22=0x16, RBO_HITSKILL_25=0x19, RBO_HITSKILL_28=0x1C, RBO_HITSKILL_31=0x1F, RBO_HITSKILL_34=0x22, RBO_HITSKILL_43=0x2B, RBO_HITSKILL_44=0x2C, RBO_HITSKILL_47=0x2F, RBO_HITSKILL_52=0x34, RBO_HITSKILL_54=0x36, RBO_HITSKILL_57=0x39, RBO_HITSKILL_69=0x45, RBO_HITSKILL_71=0x47, RBO_HITSKILL_73_NOHANDLER=0x49, RBO_HITSKILL_75_NOHANDLER=0x4B };
struct RboAtRecord {
 RboHitClass hit_class; // +0x0 
 RboHitSfxSet hit_sfx_set; // +0x4 
 int hit_spark_kind; // +0x8 
 RboAtGateMode hit_gate_mode; // +0xC 
 int base_power; // +0x10 
 int unused_14; // +0x14 
 int guard_damage; // +0x18 
 int status_duration; // +0x1C 
 int victim_shake_mode; // +0x20 
 int reserved_flag_24; // +0x24 
 int sound_bank_select; // +0x28 
 int hit_sound_id; // +0x2C 
 int hit_sound_random_range; // +0x30 
 RboAtKnockDirMode knock_dir_mode; // +0x34 
 RboAtFacingMode victim_facing_mode; // +0x38 
 RboAtElementSource element_source; // +0x3C 
 int fixed_element; // +0x40 
 RboAtDamageType damage_type; // +0x44 
 RboAtDamageMode damage_mode; // +0x48 
 RboAtFlags76 target_fx_flags; // +0x4C 
 RboAtFlags80 damage_rule_flags; // +0x50 
 int hit_script_enable; // +0x54 
 int hit_script_type; // +0x58 
 int hit_script_id; // +0x5C 
 int unused_60; // +0x60 
 int unused_64; // +0x64 
 int unused_68; // +0x68 
 unsigned __int16 skill_param_a; // +0x6C 
 unsigned __int16 skill_param_b; // +0x6E 
 RboHitSkillId hit_skill_id; // +0x70 
 int unused_74; // +0x74 
};
struct RboScriptListEntry {
 int script_id[5]; // +0x0 
};
