# GOF2 Obj part B (0x620..0xC3F, decimal 1568..3135)

Files: `gof2obj_partB_fields.py` (67 rows, contiguous 1568..4023), `gof2obj_partB_types.h` (also created in the IDB, parse_decls 0 errors).

## Layout discovered
* 1556..1743 CAppHanteiKougeki (188 B, starts in part A). 1568..1743 listed flat; one-struct form = `Gof2ObjAttackBoxSet`.
* 1744..1759 CAppHanteiKasanari = 16 B push box set (`Gof2ObjPushBoxSet`); 1760..1775 are four separate Obj dwords (body push parameters).
* 1776..4063 is ONE embedded object (vtable CAppHanteiYarare, size 2288): `Yarare_ResetHitState` 0x43E3D0, `Yarare_CanTechRecover`, `Yarare_RegisterHit` take `this = Obj+1776`
  (class offset = Obj offset - 1776, e.g. `memset(a2+50, 0, 0x800)` = Obj+1976). Class +0..+67 = hurt box set (`Gof2ObjHurtBoxSet`), +68..+199 = victim/hit-reaction state, +200..+2247 = hit history, +2248.. = part C.
* 1976..4023: `hitHistoryByAttackerPattern[2][256]` (2048 B) - owned here although it ends at 4023 (first-byte rule). Part C starts at 4024.

## Method
Scanned every instruction of all 5000 functions for `[reg+disp]` with disp 1568..3135 (186 functions), classified Obj users vs scene/CRT structs that merely share offsets
(CharaSelect/Config scene structs, 0x508xxx CRT, the afterimage heap object behind obj+524, sub_45Exxx 3D structs), and decompiled the Obj users.
Class-relative accesses of the embedded objects were followed through their vtable methods (Kougeki 9 methods, Kasanari, Yarare) and the helpers called with `lea ecx,[obj+6F0h]`.

## Counts
67 rows / 2456 bytes (1568..4023): 64 traced, 2 inferred (hitsLandedForLimit, gaugeBonusGranted), 1 unused row (unused_674[8]) plus `unused_0C` inside Gof2ObjHurtBoxSet.
Several traced rows are "writer only / reader only" and say so (attackEndLatch, attackActive, guardCrushLatch, knockSpeedXMode).

## Unused proofs
* 1652..1659 (Kougeki +0x60/+0x64): no Kougeki vtable method touches them; no Obj-base operand with disp 1652/1656 outside scene structs.
* Yarare +0x0C (1788): same, all five Yarare methods + Yarare_* helpers skip it; no Obj-base disp 1788 hit that is an Obj (only scene/afterimage hits).

## Key semantics
* Hit-result flags (1676..1724): per contact kind (hit / guard-stun hit + crush / clash / guarded) a flag word (self=1, ancestor chain obj+560 =2) plus a stance mask (victim stanceClass, ancestors <<8). Script transition rules (`Obj_HitFlagRuleMatches` 0x42F5B0) combine them with the Etc/Sousai/Tobi equivalents (+4152, +4200, +4112, part C).
* Reaction block 1844..1975: filled by Victim_StartHitReactionFromScript / Victim_StartGuardReactionFromScript (script bank g_ControllerManage[111] kind 0 id 2/1), HitJudge_ApplyHit, HitJudge_ApplyKnockDirAndFacing, Victim_EnterReactionState; consumed by Obj_ScriptTick, HitJudge_CommitPendingReactions, HitJudge_ResolveGuardPassAndCrush.
* Guard crush: sub_440E60 (guard gauge) -> Victim_MarkGuardCrush sets 1916/1920 -> HitJudge_ResolveGuardPassAndCrush starts state 56 (ground) or 37 (air).

## Renamed functions (26)
ObjHitFlags_RecordHit 0x43C210, ObjHitFlags_RecordGuardStunHit 0x43C2A0, ObjHitFlags_RecordClash 0x43C320, ObjHitFlags_RecordGuarded 0x43C360, ObjHitFlags_MarkAttackClash 0x43C420,
Obj_ApplyContactEventList 0x43C530, Obj_AnyHitFlagSet 0x42F570, Obj_HitFlagRuleMatches 0x42F5B0, Victim_StartHitReactionFromScript 0x43D9F0, Victim_StartGuardReactionFromScript 0x43DBE0,
Obj_ResetVictimReactionState 0x430770, Obj_ResolveBodyPushOverlap 0x49DB20, HitJudge_CommitPendingReactions 0x49A090, HitJudge_ResolveGuardPassAndCrush 0x4964D0, Victim_MarkGuardCrush 0x496EA0,
HitJudge_ScaleDamageByComboHits 0x495890, Victim_ExtendHitstun 0x49A2F0, Obj_TickTimersRecursive 0x49E120, Obj_TestAirRecoverableHit 0x4339F0, Obj_CanTechRecover 0x433990, Yarare_ResetHitState 0x43E3D0,
Yarare_CanTechRecover 0x43E4B0, Yarare_RegisterHit 0x43E500, HitJudge_ResolveHitsOnGuardingVictim 0x495B60, Obj_ExecStateCmdList 0x438660, Victim_ResolveFacingOverride 0x43D5B0.

## Caveats
Names of the contact-kind flags (guardStunHit/guardCrushHit/clash/guarded) come from the callers (sub_495B60 victim states 21/22/23/54/55/57 + guard gauge result, sub_49B2E0, HitJudge_OnAttackGuarded), not from a script table; confidence medium.
The `alreadyHitVictim[2]` array is indexed by side+team; a team index of 1 would address +1744 (never observed; assumed 0).
Flag enums are plain enums in the IDB (bitmask flag could not be set through the Python API here).
