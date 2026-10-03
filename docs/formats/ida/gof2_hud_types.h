// GOF2 side combo-popup state. g_SideHudPopup[2] at 0x65B6B0 (stride 0x80, followed by g_RoundJudge at 0x65B7B0); index = Gof2Obj.sideIndex of the ATTACKER (the side that is dealing the combo).
// Evidence: SideHudPopup_DrawAll 0x47ED00 / DrawOne 0x47EEB0 (reader), SideHudPopup_Tick 0x4A0350 + TickAll 0x4A0570 (animation), SideHudPopup_RegisterHit 0x4A03D0 + ShowIfComboActive 0x4A02F0 (writers),
// SideHudPopup_ResetAll 0x4A0490, SideHudPopup_ClearHitLatches 0x4A04F0 (called once per logic tick from CSceneGame_LogicTick 0x4CEAB9), HitJudge_RegisterHitForHudPopupAndCounters 0x4A05B0,
// HudPopup_ClearSideBitsForPlayerChara 0x4A06A0, HudPopup_ArmSidePopupOnce 0x4A0720, sub_4A0750 (loop over both entries), HudPopupBoard_SyncSidePopup 0x47AFC0.
// Reader/writer census: IDA xrefs to 0x65B6B0..0x65B7B0 plus a raw byte scan of .text for every 32-bit address 0x65B680..0x65B7C0 (same function set), plus every function that receives a SideHudPopup* (RegisterHit, ShowIfComboActive, Tick, DrawOne).
struct SideHudPopup {                  // 0x80 bytes
 int state;                            // +0x00 0 = hidden, 1 = shown (slides in, then holds), 2 = fading out (alpha -32/tick). Writers: ShowIfComboActive (=1), Tick (->2, ->0), ResetAll. Readers: DrawAll (skip when 0), Tick, ShowIfComboActive, ClearSideBits
 int slideX;                           // +0x04 screen X slide offset: starts -32 (ShowIfComboActive), +8/tick to 0; +8/tick while fading. DrawOne adds it to the base X (0 side) or 592 (mirrored side)
 int alpha;                            // +0x08 0..255 popup alpha: 255 on show, -32/tick in state 2 (state -> 0 at <= 0). DrawOne alpha for hits and damage
 int holdTimer;                        // +0x0C frames the popup stays at state 1 once the combo broke: -1 = held while combo runs, set to 60 by ShowIfComboActive/ClearSideBits/sub_4A0750 on combo end; Tick counts down, 0 -> state 2
 int shownHits;                        // +0x10 hit count drawn (copy of runningHits when combo >= 2, DrawOne clamps to 999)
 int shownDamage;                      // +0x14 damage drawn (copy of runningDamage, DrawOne clamps to 99999)
 int shownFlag6;                       // +0x18 1 = draw the banner quad (HudPopup_AddBannerQuad); written 1 by ShowIfComboActive (=armed) and ArmSidePopupOnce, 0 on reset
 int bannerSlideX;                     // +0x1C banner X slide: -64 on arm (ArmSidePopupOnce / ApplyHit), Tick adds 8/tick up to 0 while shownFlag6 == 1; DrawOne adds it to slideX+200
 int flowAlphaScale;                   // +0x20 0..255 alpha scale of the flow counter (DrawOne: alpha*flowAlphaScale/255); 255 on RegisterHit when flowRunHits >= 2, TickAll subtracts 4/tick while flowTimer > 0
 int flowTimer;                        // +0x24 -1 = flow counter held, 60 = start fading (set by RegisterHit/ClearHitLatches/ClearSideBits/sub_4A0750), TickAll counts it down while fading flowAlphaScale
 int flowHits;                         // +0x28 flow (hits since this side was last hit) drawn by DrawOne (clamped to 999); copy of flowRunHits
 int flowDamage;                       // +0x2C copy of flowRunDamage on RegisterHit; no reader (DrawOne draws flowHits only)
 int comboEndedLatch;                  // +0x30 was flag30. 1 = this combo just ended (set by ShowIfComboActive else-branch when state==1, ClearSideBits when the victim mask empties, sub_4A0750); 0 when a >=2 combo starts. Only reader: HudPopupBoard_SyncSidePopup (of the opposing board) which then clears its +0xA8 cell flag and zeroes the latch
 int lastDamageDealt[5];               // +0x34 was hist[0..4]. Shift history of RegisterHit arg a2 (applied damage); [0] newest. WRITE-ONLY: shifted in RegisterHit, cleared in ResetAll; no reader anywhere
 int lastAttackBaseDamage[5];          // +0x48 was hist[5..9]. Shift history of RegisterHit arg a4 (attackRecord->damage of the hit, Gof2AtRecord+0x10); [0] newest. WRITE-ONLY, no reader
 unsigned int victimSideMask;          // +0x5C was attackerMask. Bit per VICTIM side hit by this combo (RegisterHit |= 1<<victimSide); HudPopup_ClearSideBitsForPlayerChara clears a victim's bit when it recovers; empty mask ends the combo (clears +0x5C..+0x68)
 int armed;                            // +0x60 1 once the air-hit banner was armed (ArmSidePopupOnce, ApplyHit airHitLatch path); cleared on mask empty; guards re-arming
 int runningHits;                      // +0x64 combo hit count (saturating at -1 on wrap); readers: ShowIfComboActive (>=2 shows popup), HudPopupBoard_Tick, sub_47E980, AddAttackerGaugeOnHitByAttackKind
 int runningDamage;                    // +0x68 combo damage total (saturating at -1)
 int gotHitLatch;                      // +0x6C was unk6C. Set 1 on the VICTIM side's popup by HitJudge_RegisterHitForHudPopupAndCounters (this side was hit this tick). Only consumer: ClearHitLatches, which clears flowRunHits/flowRunDamage, starts flowTimer=60 if -1, then zeroes it
 int flowRunHits;                      // +0x70 was hitsB. Hits strung since this side was last hit; ++ in RegisterHit, zeroed by ClearHitLatches when gotHitLatch; copied into flowHits when >= 2
 int flowRunDamage;                    // +0x74 was damageB. Damage dealt since this side was last hit; += a2 in RegisterHit, zeroed with flowRunHits
 int hitContactLatch;                  // +0x78 was unk78. Set 1 when a hit resolves involving this side's player chara (ResolveHitsOnGuardingVictim: guarding victim; ApplyHit: attacker and victim; Obj_ExecCmdListBetween: defender). Only consumer: ClearHitLatches (gates the guardedHitsDealt clear), then zeroes it. No other reader
 unsigned int guardedHitsDealt;        // +0x7C was unk7C. Count of guarded hits this side's player chara landed (ResolveHitsOnGuardingVictim: ++ on the ATTACKER's popup, saturating at -1). Only reader/consumer: none (cleared in ClearHitLatches when hitContactLatch, and ResetAll); never read for a decision
};
