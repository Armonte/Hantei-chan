# GOF2 Obj.bufferedJump (+0x250): dead latch, and the 0x250 HUD adds

Analysed in GOF2.exe.i64 (2026-10-03). Editor rule: the editor must NOT change game behaviour. Do not "fix" the missing -1 init, do not emulate a latch; treat the block as write-only scratch.

## bufferedJump (Gof2Obj +0x250..+0x25F, struct Gof2ObjABufferedJump)
Note the block starts at +0x250 (kind); +0x254/+0x258/+0x25C are unused_04/08/0C.

Writers (full `[reg+disp]` scan of .text for 0x250/0x254/0x258/0x25C):
* ObjEvent_InputRuleBufferJump 0x4365D0: gate `cmp [ebx+250h], -1` at 0x436762; only inside the gate it stores -1 to +4/+8/+0xC, kind=0 or 1, and the payload (0x43676c..0x4367b3). Event type 1 only.
* ObjEvent_ConditionalRuleSelectJump 0x436280 (0x43642c..0x43643e) and Obj_TestRelativePosition 0x436470 (0x436501..0x436517): unconditionally store kind=2, payload, +0xC=-1.
* Nothing ever stores -1 to +0x250. Obj_InitDefaults 0x431000, Obj_ConstructPoolSlot 0x435850, Obj_ResetTransientStateOnActionStart 0x4306d0 (only memset of moveUseRecord), Obj_ResetOnDetach 0x4390D0, ObjPool_Reset/AllocIntoOwnerList/FreeObjAndUnlink contain no access and no bulk fill of the Obj.

Readers: only the gate at 0x436762. No instruction reads +0x254/+0x258/+0x25C, and kind is never switched on or fed to an action. (Other hits for those displacements are scene/capture/UDP structs, g_BattleState+0x250 and DefStatus +0x250/254/258/25C loads in Obj_SetActionAndRunScript / Obj_StartPlayerRecTimerFromDefStatusScript, none are Obj.)

Value at creation: g_ObjPool (0x789060, 5000 x 0x1264) lies beyond the raw size of .data (raw 0x1F000, ends 0x5A9000; virtual 0x188E2A0), i.e. zero-filled bss. IDA shows 0xFFFFFFFF there only because the bytes are unloaded (is_loaded false). Obj_ConstructPoolSlot (static eh-vector ctor) does not touch the field. So every slot starts with kind = 0 (= G2OBJA_BJUMP_DIRECT), and slot reuse does not reset it.

Conclusions
1. The latch can never fire: kind==0 is not -1, so the gate in ObjEvent_InputRuleBufferJump is false from the first call; in that case the code just `return 1` (event consumed, the same return as after latching). A stale 0 is never "valid" either, because nothing reads kind as a value.
2. After the first kind=2 write from the other two events it is permanently 2, also != -1.
3. Verdict: an original-game initialisation oversight (RBO analog RboP3BufferedJump uses -1 = none and has a consumer, sub_41E500; GOF2 dropped the consumer and the init) that is observationally dead code. Even if the gate were true the stores would affect nothing, since no code reads the result. No behaviour to replicate; the 0/1/2 writes and the gate have no effect on gameplay.
(Caveat: a whole-Obj copy/memcpy would not read the field in any way that matters; none was found anyway.)

## sub_47EEB0 and the two `add reg, 250h` sites (0x47F01C, 0x47F0D2)
sub_47EEB0 is not an Obj function. It is now `SideHudPopup_DrawOne(SideHudPopup *this, int sideIndex)`, called from `SideHudPopup_DrawAll` (was sub_47ED00, loops sides 0..1 over g_SideHudPopup, stride 0x80, 2 entries = 0x65B6B0..0x65B7B0, then g_RoundJudge). It draws the combo popup: shownHits (3 digits, CDrawComboCount), flowHits (3 digits, CDrawComboFlow, alpha*flowAlphaScale/255), shownDamage (5 digits, CDrawComboDamage), plus the banner quad HudPopup_AddBannerQuad (was sub_47ED40). The two `add reg, 250h` are on `this->slideX` (this+4): 592 is the SCREEN X of the right-hand (side 1) mirrored counter and damage numbers (hits at x+592-width, flow at +485 (0x1E5), banner +556 (0x22C)). Not Gof2Obj offsets.
IDB: struct `SideHudPopup` (0x80) created and applied as `SideHudPopup g_SideHudPopup[2]`; renamed sub_479300 DrawNumber_WithShadow, sub_4A03D0 SideHudPopup_RegisterHit, sub_4A02F0 SideHudPopup_ShowIfComboActive, sub_4A0490 SideHudPopup_ResetAll, sub_4A04F0 SideHudPopup_ClearHitLatches, sub_479450/479560 CDrawComboCount_ctor/CDrawComboInfo_ctor. Fields +0x00..0x2C are traced from the tick/draw/register functions; former hist[], flag30, unk6C/78/7C are now proven and renamed (lastDamageDealt, lastAttackBaseDamage, comboEndedLatch, gotHitLatch, hitContactLatch, guardedHitsDealt, attackerMask -> victimSideMask, hitsB/damageB -> flowRunHits/flowRunDamage); see docs/formats/ida/gof2_hud_types.h.
