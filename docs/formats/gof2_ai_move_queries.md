# GOF2 AI move-timing queries (0x466000..0x46B400), pattern 16/19, PlayerRec timer, status-effect SEs

Everything below is backed by the exe (IDA `GOF2.exe.i64`) AND the shipped character data (`data05` over `data02` `.DT2`). The truth tables were produced by
**emulating the real predicates** with unicorn (`tools/gof2/emu_ai_queries.py`, raw output `tools/gof2/ai_truth/*.json`, table generator
`tools/gof2/gen_ai_move_queries_md.py`, DT2 reader `tools/gof2/dt2_pattern_dump.py`); no pseudocode was hand-transcribed.
No move-name strings exist anywhere in the shipped data (`.DT2` pattern-name area `+0x38` has size 0 in every `.DT2` of data02/data05; the engine never names patterns),
so moves are identified by pattern id, input and data (attack record), not by name.

## 0. Keys

* Chara id (`*(charaContext + 0xFC6C)`; the decompiler shows `charaContext + 16155` because it is an int index): 0 KANAE, 1 DATE, 2 IMAGAWA, 3 AMIY, 4 MAEDA, 5 SANADA, 6 TOKUGAWA, 7 CYOSOKABE, 8 TAKEDA, 9 UESUGI, 10 NEGAI, 11 KATSUKO.
  Evidence: AI name table 0x5A75E8.. pairs `KA/Kanae FU/Date PA/Imagawa AM/Amiy MI/Maeda MA/Sanada AS/Tokugawa YU/Cyosokabe MD/Takeda RU/Uesugi NE/Negai KT/Katsuko`
  and the voice-name table 0x599AEC (`Kanae 0, Futaba 1, Papeko 2, Ammy 3, Minami 4, Makoto 5, Ashita 6, Yukie 7, Madoi 8, Ruki 9, Negai 10, Katsuko 11`, same order; FU=Futaba=Date, PA=Papeko=Imagawa).
* Input -> pattern (`sub_433220`, `Obj_ResolveActionFromButtonsAndDirection`): button bit 0x100 gives the EVEN pattern, 0x200 the ODD one. Direction code (after mirroring bits 1/2 by facing):
  0 -> 60/61, 1 -> 62/63, 9 -> 64/65, 8 -> 66/67, 10 -> 68/69, 2 -> 70/71, 6 -> 72/73, 4 -> 74/75, 5 -> 76/77, buttons only (no dir, in cancel state) 78/79.
  Data: even 60..78 = attackKind 0, median damage 80 (107 of 107 chars x patterns); odd 61..79 = attackKind 1, median damage 462 (103 of 110). So **even = weak, odd = strong**.
* guardMask (attack record +0x7C, `Gof2AtGuardMask`): bit0 stand guard can block, bit1 crouch guard can block, bit2 extra. **5/1 = high (stand-guard only), 2/6 = low (crouch-guard only), 3/7 = mid (either)**.
* Ticks: the engine's frame durations (`frame.duration`); "ticks to hit" = sum of durations from the range frame to the next attack frame (negative = after the last attack frame).

## 1. Result: what each query checks

| IDB name (new) | was | Meaning (proved by the tables in section 4) |
|---|---|---|
| `Obj_IsAtHighAttackTellKeyFrameByChara` 0x4668C0 | `Obj_IsAtMoveKeyFrameByChara` | early-warning key frame (4..54 ticks before the first active frame, usually 1 frame wide, some need frameTicks>=N) of attack patterns whose first hit has guardMask 5/1 (high). Pair of the next one. |
| `Obj_IsInHighAttackPreHitWindowByChara` 0x466F50 | `Obj_IsInMoveFrameRangeByChara` | final pre-hit window (1..9 ticks before the first active frame, through it) for high attacks (guardMask 5/1) |
| `Obj_IsInLowAttackEarlyWindowByChara` 0x467A10 | `Obj_IsInMoveFrameWindow2ByChara` | early window (6..63 ticks, median 13) for low attacks (guardMask 2/6) |
| `Obj_IsInLowAttackPreHitWindowByChara` 0x467C50 | `Obj_IsInMoveFrameRange3ByChara` | final pre-hit window (0..22 ticks, ends at the first active frame) for low attacks |
| `Obj_IsInMidAttackEarlyWindowByChara` 0x468100 | `Obj_IsInMoveFrameWindow4ByChara` | early window (3..117 ticks, median 14) for mid attacks (guardMask 3/7) |
| `Obj_IsInMidAttackPreHitWindowByChara` 0x468840 | `Obj_IsInMoveFrameRange5ByChara` | final pre-hit window (0..28 ticks) for mid attacks; multi-hit moves also cover up to 24 ticks after the hit |
| `Obj_IsInWeakAttackPreHitWindowByChara` 0x469270 | `Obj_IsInMoveFrameRange6ByChara` | pre-hit window (0..24 ticks) of the weak (even 60..76, attackKind 0) normals plus some specials, any guard height |
| `Obj_IsInPostHitRecoveryWindowByChara` 0x4698F0 | `Obj_IsInMoveFrameRange7ByChara` | from ~2 ticks after the last attack frame to the end of the move (241 ranges), only caller `CComWorkBase__vf30` = probabilistic punish roll |
| `Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Set` 0x466660 | `...StartupWindowA` | pattern 16 (chara 0,9: 19) with frameNo <= first attack frame (+/-1); sets A/B/C split the 16/19 attacks per character: A = mostly guardMask 5 (3 exceptions: KANAE 19 and UESUGI 19 mask 7, MAEDA 16 mask 1), B = guardMask 6, C = guardMask 7; AI answers A with command 0x404, B with 0x408, C with a mix (live-confirmed roles, see gof2_live_evidence.md) |
| `Obj_IsInPat16Or19AtkStartup_ReplyGuardDir8Set` 0x466770 | `...StartupWindowB` | see above |
| `Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Or8Set` 0x466850 | `...StartupWindowC` | see above |

Timeline of one multi-hit move (KANAE pattern 107, hits at frames 7 and 18) shows how they chain: Window4 f2, Range5 f3-7 (hit), KeyFrame-style gap f8-13, Range f14-18 (hit), Range7 f19-29.
The consumers are the AI: `CComWorkBase__vf9` (case 0x101 high -> guard command 0x404, 0x102 low -> 0x408, 0x103 ...), `__vf21`, `__vf29`, `__vf5`, `CComWorkNormal__vf13/29`, `__vf30`;
0x400 is the guard button (patterns 49/50 are entered while it is held alone), dir 8 = crouching direction (pattern 66/67 starts from a lowered hurtbox). **LIVE CORRECTION (gof2_live_evidence.md): 0x404 / 0x408 are NOT plain stand/crouch guard. From neutral, Guard+dir4 enters pattern 15 and Guard+dir8 enters pattern 18 (the 15->16 / 18->19 guard-attack moves); only Guard with no dir 4/8 bit gives guard patterns 49/50. The AI emits these commands to start the 15/18 move as its answer.**

Related predicates (data-verified): `Obj_IsInEvenAttackPatternSetByChara` 0x466000 = weak normals (60,62..78) + per-chara extras; `Obj_IsInOddAttackPatternSetByChara` 0x4661F0 = strong normals (61,63..79) + extras;
`Obj_IsPatternInHitstunRange49To57` 0x466590 is a misnomer: 49..57 are the **guard** patterns (49/50 entered while 0x400 is held, 51 on release, 54/57 tested by `Obj_IsPattern54Or57`); hit reactions are 28..39 (gof2_at_sections.md).

## 2. Patterns 15/16/18/19 (the "Pattern16Or19" question)

* `Obj_ProcessInputAndStateTransitions` 0x433D00 (was `Obj_ProcessInputAndStateTransitions`; the per-tick input/state controller) while button 0x400 is held and the character is in a guard state: dir 4/5/6 -> `Obj_RequestPattern(15)`, dir 8/9/A -> pattern 18 (also dir 1 -> 9, dir 2 -> 11), gated by a gauge check (`DefStatus+48` / `+52`) and by the flags `obj+0x11D8` / `+0x11DC` (`stanceClashLatchAction16` / `stanceClashLatchAction19`, renamed from `hitDuringAction16/19` after the live clash run): with Guard+dir4/5/6 pattern 15 is only requested while `+0x11D8 == 0`, with dir 8/9/A pattern 18 only while `+0x11DC == 0`.
* Those two flags are set only in `Obj_ApplyContactEventList` 0x43C845/0x43C851, in the contact-event type 8 branch, after `ObjHitFlags_MarkAttackClash`, when the contact category `(obj+0x5DC & 0xF0) == 0x10` and the current pattern of the object whose attack connected is 16 (0x10) / 19 (0x13). **Live-confirmed (gof2_live_evidence.md, section Clash latches): the latch is set on the ATTACKER, when its 16/19 attack hits the opponent's counter stance (15/18, the stance has clash boxes and answers with its own 16/19). It is NOT set by a normal hit, nor by two attacks trading (16 vs 16, 19 vs 19, 16 vs 19).** Once set it lasts until `Obj_ResetTransientStateOnActionStart` (it survives the hit reactions that follow); while set the attacker cannot re-enter stance 15 (16 latch) / 18 (19 latch) with Guard+dir. Patterns 16,17,19,20 are also the only states for which the tail of `Obj_ProcessInputAndStateTransitions` re-opens input rules when `clashState != 0`.
* Data: patterns 16 and 19 are present in all 12 characters, hold one (Imagawa two) attack record each, always attackKind 1 (special). Per character the first attack frame equals the frame limit of the matching set (A/B/C) within +/-1 (e.g. KANAE 16: attack f4, set B limit 3; KANAE 19: attack f3, set A limit 4). Pattern 17 / 20 are the recovery-only tails of 16 / 19 (identical duration list from the attack frame on).
* guardMask of the first hit: pattern 16 = 5 for 8 chars, 1 for 1, 6 for 1 (KANAE), 7 for 2; pattern 19 = 6 for 6 chars, 7 for 4, none for 2; so **16 ~ high version, 19 ~ low version** of each character's Guard-button follow-up attack. DATE 19 and TAKEDA 19 have no attack record; UESUGI 16 (guardMask 7) is not in any set.
* Exact move names are not recoverable from data (see header). Live trace obtained (gof2_live_evidence.md): Guard+dir4/5/6 -> 15 -> 16 -> 17, Guard+dir8/9/A -> 18 -> 19 (-> 20 not seen); 16/19 also begin when an enemy attack connects while 15 is in its f3 window. Latches `obj+0x11D8/0x11DC` are now confirmed live: set on the attacker when its 16/19 attack connects with the opposing 15/18 stance (see above).

## 3. Related findings (items c and d of the review)

### 3.1 `BattlePlayerRec` (g_BattlePlayerRec, 2 x 140 bytes) rec[29] / rec[30]

IDB type `BattlePlayerRec` (140 bytes) created and applied to `g_BattlePlayerRec[2]` (was `char[88]`). Fields: `[22]` isNonDefaultKind, `[23]` cooldownTicks (decremented per tick), `[24]` setupKind, `[25]` displayState (1 shown, 2 clearing), `[26]` displayAlpha (255), `[27]` timerActive, `[28]` extTimerActive, **`[29]` timerTicksRemaining**, **`[30]` timerTicksInitial**, `[31]` extTimerTicksRemaining, `[32]` extTimerTicksInitial, `[33]` lingerTicks.

* rec[29] (+0x74) readers (all of them; whole exe scanned for +74h/+78h in every function that touches the array, plus the absolute refs 0x1E055F4 / 0x1E05680): `PlayerRec_TickTimers` 0x4A3780 (decrement once per tick while `timerActive`; at 0 `lingerTicks = DefStatus+548`, then everything clears), `PlayerRec_DrawAll` 0x47FB90 (`!= 0` gate: when 0 the number blinks via `blinkTick`), `PlayerRec_DrawTimerDigits` 0x480090 (passes it to `PlayerRec_TimerTicksToDigits` 0x4828A0 = ticks/120 -> `SS.cc`, clamped 0..11999), `PlayerRec_ClearExpiredTimersIfOverLimit` 0x4A39C0 (`timerActive==1 && ticks==0`). Writers: `Obj_StartPlayerRecTimerFromDefStatusScript` 0x4383D0 (value of DefStatus bank1 fn1 script, +DefStatus+544 when a4==1, x side modifiers, min 1), the clear functions (`Obj_ExecPlayerRecSetupOps` case 3, `Obj_ResetOnDetach`, `PlayerRec_ResetForRound`, `PlayerRec_ClearTimerStateByPtr/BySide`, `PlayerRec_ClearTimersForFlaggedSides`).
  **Meaning: a countdown in logic ticks (120 per second), displayed above the character as SS.cc.**
* rec[30] (+0x78): **no reader anywhere** (no instruction, no absolute reference, no register-relative read in any function touching the array); written once with the same value as rec[29] in `Obj_StartPlayerRecTimerFromDefStatusScript` -> write-only "initial duration". rec[32] likewise write-only (`Obj_RunDefStatusScript2UpdatePlayerRec` stores the same value into [31] and [32]).

### 3.2 Status-effect SE ticks (`SharedMod_TickRepeatRandomBoxEffect*`)

**Live-confirmed (debugger int3 on `Sound_RestartBufferAtVolume`; all six slots and their calling functions, plus the 39 loaded buffers matched by wav data size to SYSTEMSE.FOB order): see gof2_live_evidence.md.**

All four plus the two `ObjTintFx_TickHitFlash*` ticks: every `period` ticks spawn effect object N at a random point of the object's boxes (`Obj_SpawnEffectObjAtRandomBoxPoint`) and **restart** a DirectSound one-shot (`Sound_RestartBufferAtVolume` 0x456FC0: Stop, SetCurrentPosition 0, SetVolume, Play) unless that buffer is still playing (GetStatus & 1).
Sound id = `g_SoundSourceBuffers` slot (unk_72EC30 = 0). `SystemSE.Fob` kind 1 id 0 (loaded with arg0 = 101 by `sub_457F10`) is the hit-SE group; entry i gets slot 101+i, so slots 134..139 are entries 33..38:

| effect obj | tick function | slot | wav |
|---|---|---|---|
| 91 | `ObjTintFx_TickHitFlashCycle` (period 6) | 134 | `se_burn.wav` |
| 92 | `ObjTintFx_TickHitFlashFixed` (period 12) | 138 | `se_freeze.wav` |
| 93 | `SharedMod_TickRepeatRandomBoxEffect93_SeShock` | 136 | `se_shock.wav` |
| 115 | `..Effect115_SeBurnDot` | 135 | `se_burn_dot.wav` |
| 116 | `..Effect116_SeFreezeDot` | 139 | `se_freeze_dot.wav` |
| 117 | `..Effect117_SeShockDot` | 137 | `se_shock_dot.wav` |

(SystemSE.Fob entries 33..38: `se_burn, se_burn_dot, se_shock, se_shock_dot, se_freeze, se_freeze_dot`.)

## 4. Truth tables (emulated, frames 0..99, ticks 0..200)

### Obj_IsAtHighAttackTellKeyFrameByChara 0x4668C0 (was `Obj_IsAtMoveKeyFrameByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 69 | 4-4 | 19 | 10 | 5 | 600 | 1 | 16..16 |
| 0 KANAE | 77 | 5-5 | 19 | 9 | 5 | 550 | 1 | 9..9 |
| 0 KANAE | 84 | 1-4 | 19 | 9 | 5 | 60 | 0 | 16..11 |
| 0 KANAE | 107 | 8-13 | 30 | 7,18 | 3 | 200 | 0 | 21..13 |
| 0 KANAE | 108 | 8-13 | 30 | 7,18 | 3 | 200 | 0 | 21..13 |
| 1 DATE | 77 | 9-9 | 36 | 14 | 1 | 450 | 1 | 13..13 |
| 2 IMAGAWA | 69 | 4-4(t>=9) | 18 | 8 | 5 | 450 | 1 | 24..24 |
| 2 IMAGAWA | 74 | 4-6 | 0 | - | - | - | - | - |
| 3 AMIY | 65 | 6-8 | 26 | 14 | 1 | 300 | 1 | 18..11 |
| 4 MAEDA | 77 | 6-9 | 24 | 15 | 5 | 700 | 1 | 23..10 |
| 4 MAEDA | 88 | 4-4(t>=15) | 16 | - | - | - | - | - |
| 5 SANADA | 63 | 5-5(t>=6) | 27 | 9,15 | 1 | 150 | 1 | 21..21 |
| 5 SANADA | 67 | 7-7(t>=3) | 24 | 12 | 1 | 600 | 1 | 26..26 |
| 5 SANADA | 73 | 4-4(t>=5) | 15 | 7 | 5 | 500 | 1 | 18..18 |
| 6 TOKUGAWA | 63 | 8-10 | 32 | 14,16,18 | 1 | 250 | 1 | 19..7 |
| 6 TOKUGAWA | 73 | 34-35 | 57 | 40,41,42 | 5 | 500 | 1 | 8..6 |
| 6 TOKUGAWA | 75 | 15-21 | 37 | 23,24 | 5 | 800 | 1 | 9..2 |
| 6 TOKUGAWA | 79 | 4-4 | 24 | 8 | 5 | 500 | 1 | 10..10 |
| 7 CYOSOKABE | 61 | 7-8 | 22 | 12 | 1 | 500 | 1 | 11..10 |
| 7 CYOSOKABE | 65 | 3-3(t>=15),4-4 | 30 | 7,21 | 1 | 0 | 1 | 20..4 |
| 7 CYOSOKABE | 75 | 4-4 | 14 | 7 | 1 | 300 | 1 | 21..21 |
| 7 CYOSOKABE | 118 | 7-13 | 35 | 18 | 5 | 1280 | 1 | 54..12 |
| 8 TAKEDA | 67 | 5-5(t>=6),6-7 | 22 | 11 | 5 | 366 | 1 | 20..11 |
| 8 TAKEDA | 69 | 2-2(t>=15) | 28 | 6,12,16 | 1 | 150 | 1 | 20..20 |
| 8 TAKEDA | 73 | 8-12 | 32 | 18 | 5 | 400 | 1 | 16..8 |
| 8 TAKEDA | 75 | 8-12 | 32 | 18 | 5 | 400 | 1 | 16..8 |
| 8 TAKEDA | 77 | 6-7 | 19 | 10 | 5 | 333 | 1 | 13..9 |
| 8 TAKEDA | 124 | 18-20 | 54 | 23,24,25,26,27,28,29 | 5 | 30 | 0 | 10..6 |
| 9 UESUGI | 63 | 4-4(t>=9) | 14 | 7 | 5 | 525 | 1 | 19..19 |
| 9 UESUGI | 69 | 6-6,21-21 | 33 | 12,13,14,26 | 5 | 250 | 1 | 31..10 |
| 9 UESUGI | 77 | 7-7(t>=5) | 17 | 10 | 5 | 450 | 1 | 16..16 |
| 9 UESUGI | 110 | 4-4 | 19 | 7,8,9 | 1 | 700 | 1 | 30..30 |
| 10 NEGAI | 77 | 4-4 | 23 | 7,8,9 | 5 | 750 | 1 | 6..6 |
| 10 NEGAI | 81 | 0-2 | 15 | 7 | 5 | 200 | 1 | 9..7 |
| 10 NEGAI | 82 | 0-1 | 14 | 6 | 5 | 200 | 1 | 8..7 |
| 10 NEGAI | 88 | 4-4 | 40 | 7,10,15,20,26 | 5 | 80 | 0 | 7..7 |
| 10 NEGAI | 110 | 9-10,87-93 | 121 | 15,27,42,54,66,76,86,99 | 5 | 150 | 0 | all |
| 11 KATSUKO | 63 | 2-2(t>=10) | 20 | 5 | 5 | 500 | 1 | 23..23 |
| 11 KATSUKO | 71 | 3-3(t>=10) | 52 | 7,8,42 | 5 | 0 | 0 | 23..23 |
| 11 KATSUKO | 75 | 5-9 | 28 | 14,15,16 | 5 | 600 | 1 | 15..9 |

### Obj_IsInHighAttackPreHitWindowByChara 0x466F50 (was `Obj_IsInMoveFrameRangeByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 69 | 5-10 | 19 | 10 | 5 | 600 | 1 | 8..0 |
| 0 KANAE | 75 | 4-7 | 20 | 7,8 | 5 | 350 | 1 | 5..0 |
| 0 KANAE | 77 | 6-9 | 19 | 9 | 5 | 550 | 1 | 3..0 |
| 0 KANAE | 79 | 1-2 | 14 | 3 | 1 | 550 | 1 | 15..1 |
| 0 KANAE | 84 | 5-9 | 19 | 9 | 5 | 60 | 0 | 5..0 |
| 0 KANAE | 101 | 6-9 | 15 | 9 | 1 | 600 | 1 | 7..0 |
| 0 KANAE | 104 | 4-8 | 29 | 15 | 2 | 900 | 1 | 25..15 |
| 0 KANAE | 107 | 14-18 | 30 | 7,18 | 3 | 200 | 0 | 9..0 |
| 0 KANAE | 108 | 14-18 | 30 | 7,18 | 3 | 200 | 0 | 9..0 |
| 0 KANAE | 110 | 39-46 | 56 | 9,10,46 | 2 | 0 | 0 | 9..0 |
| 1 DATE | 69 | 9-13 | 25 | 13 | 3 | 500 | 1 | 5..0 |
| 1 DATE | 75 | 4-8 | 25 | 8 | 5 | 400 | 1 | 8..0 |
| 1 DATE | 77 | 10-14 | 36 | 14 | 1 | 450 | 1 | 7..0 |
| 1 DATE | 79 | 3-4 | 13 | 4 | 5 | 350 | 1 | 1..0 |
| 1 DATE | 97 | 14-17 | 28 | 17 | 5 | 500 | 1 | 5..0 |
| 1 DATE | 98 | 14-17 | 28 | 17 | 5 | 500 | 1 | 5..0 |
| 1 DATE | 110 | 5-10,16-18 | 33 | 10,15,20 | 1 | 200 | 1 | 7..3 |
| 2 IMAGAWA | 65 | 14-19 | 29 | 9,13,19 | 7 | 50 | 0 | 9..0 |
| 2 IMAGAWA | 69 | 5-8 | 18 | 8 | 5 | 450 | 1 | 6..0 |
| 2 IMAGAWA | 74 | 7-11 | 0 | - | - | - | - | - |
| 2 IMAGAWA | 83 | 0-3 | 12 | 3 | 5 | 150 | 0 | 7..0 |
| 2 IMAGAWA | 91 | 5-8 | 18 | 7,8 | 5 | 300 | 1 | 4..0 |
| 3 AMIY | 63 | 5-8 | 21 | 8 | 5 | 300 | 1 | 5..0 |
| 3 AMIY | 65 | 9-14 | 26 | 14 | 1 | 300 | 1 | 9..0 |
| 3 AMIY | 82 | 7-8 | 16 | 8 | 5 | 150 | 0 | 1..0 |
| 3 AMIY | 89 | 6-9 | 24 | 7,9 | 1 | 150 | 0 | 1..0 |
| 3 AMIY | 99 | 3-4 | 14 | 2,4 | 6 | 300 | 1 | 3..0 |
| 3 AMIY | 100 | 0-3 | 15 | 3 | 5 | 500 | 1 | 12..0 |
| 3 AMIY | 101 | 0-3 | 17 | 3 | 7 | 550 | 1 | 7..0 |
| 3 AMIY | 102 | 0-3 | 15 | 3 | 5 | 550 | 1 | 10..0 |
| 4 MAEDA | 63 | 8-13 | 21 | 13 | 1 | 600 | 1 | 7..0 |
| 4 MAEDA | 68 | 11-15 | 16 | 4,10 | 3 | 60 | 0 | -2..-28 |
| 4 MAEDA | 69 | 5-6 | 16 | 6 | 1 | 500 | 1 | 2..0 |
| 4 MAEDA | 77 | 10-15 | 24 | 15 | 5 | 700 | 1 | 9..0 |
| 4 MAEDA | 87 | 7-8 | 16 | - | - | - | - | - |
| 4 MAEDA | 88 | 5-8 | 16 | - | - | - | - | - |
| 4 MAEDA | 118 | 17-21 | 30 | 5,16,21 | 7 | 400 | 1 | 14..0 |
| 5 SANADA | 63 | 6-9 | 27 | 9,15 | 1 | 150 | 1 | 3..0 |
| 5 SANADA | 67 | 8-12 | 24 | 12 | 1 | 600 | 1 | 6..0 |
| 5 SANADA | 69 | 7-10 | 32 | 10,16 | 5 | 100 | 0 | 5..0 |
| 5 SANADA | 73 | 5-7 | 15 | 7 | 5 | 500 | 1 | 2..0 |
| 6 TOKUGAWA | 61 | 16-17 | 28 | 15,17 | 3 | 200 | 1 | 2..0 |
| 6 TOKUGAWA | 63 | 11-14 | 32 | 14,16,18 | 1 | 250 | 1 | 5..0 |
| 6 TOKUGAWA | 67 | 5-8 | 19 | 8 | 5 | 450 | 1 | 4..0 |
| 6 TOKUGAWA | 73 | 36-42 | 57 | 40,41,42 | 5 | 500 | 1 | 4..0 |
| 6 TOKUGAWA | 75 | 22-24 | 37 | 23,24 | 5 | 800 | 1 | 1..0 |
| 6 TOKUGAWA | 79 | 5-8 | 24 | 8 | 5 | 500 | 1 | 5..0 |
| 7 CYOSOKABE | 61 | 9-12 | 22 | 12 | 1 | 500 | 1 | 3..0 |
| 7 CYOSOKABE | 63 | 16-17 | 32 | 9,10,11,12,13,14,15,17 | 7 | 300 | 1 | 1..0 |
| 7 CYOSOKABE | 65 | 5-7 | 30 | 7,21 | 1 | 0 | 1 | 2..0 |
| 7 CYOSOKABE | 71 | 20-25 | 43 | 24,25 | 1 | 600 | 1 | 4..0 |
| 7 CYOSOKABE | 75 | 5-6 | 14 | 7 | 1 | 300 | 1 | 5..1 |
| 7 CYOSOKABE | 118 | 14-18 | 35 | 18 | 5 | 1280 | 1 | 7..0 |
| 8 TAKEDA | 67 | 8-11 | 22 | 11 | 5 | 366 | 1 | 5..0 |
| 8 TAKEDA | 69 | 3-6 | 28 | 6,12,16 | 1 | 150 | 1 | 4..0 |
| 8 TAKEDA | 73 | 13-18 | 32 | 18 | 5 | 400 | 1 | 7..0 |
| 8 TAKEDA | 75 | 13-18 | 32 | 18 | 5 | 400 | 1 | 7..0 |
| 8 TAKEDA | 77 | 8-10 | 19 | 10 | 5 | 333 | 1 | 5..0 |
| 8 TAKEDA | 113 | 19-21 | 58 | 12,13,14,19,20,21 | 5 | 0 | 0 | 0..0 |
| 8 TAKEDA | 124 | 21-29 | 54 | 23,24,25,26,27,28,29 | 5 | 30 | 0 | 4..0 |
| 9 UESUGI | 63 | 5-7 | 14 | 7 | 5 | 525 | 1 | 3..0 |
| 9 UESUGI | 69 | 8-14,22-26 | 33 | 12,13,14,26 | 5 | 250 | 1 | 8..0 |
| 9 UESUGI | 70 | 4-5 | 18 | 5,13 | 5 | 100 | 0 | 1..0 |
| 9 UESUGI | 75 | 1-3 | 0 | - | - | - | - | - |
| 9 UESUGI | 76 | 8-12 | 22 | 9,10,11,12,13 | 5 | 50 | 0 | 3..0 |
| 9 UESUGI | 77 | 8-10 | 17 | 10 | 5 | 450 | 1 | 4..0 |
| 9 UESUGI | 110 | 5-9 | 19 | 7,8,9 | 1 | 700 | 1 | 6..0 |
| 10 NEGAI | 61 | 8-11 | 25 | 11 | 5 | 600 | 1 | 6..0 |
| 10 NEGAI | 77 | 5-9 | 23 | 7,8,9 | 5 | 750 | 1 | 2..0 |
| 10 NEGAI | 79 | 8-10 | 24 | 9,10 | 5 | 500 | 1 | 1..0 |
| 10 NEGAI | 81 | 3-7 | 15 | 7 | 5 | 200 | 1 | 5..0 |
| 10 NEGAI | 82 | 2-6 | 14 | 6 | 5 | 200 | 1 | 6..0 |
| 10 NEGAI | 87 | 5-7 | 26 | 7,10,15 | 5 | 100 | 0 | 3..0 |
| 10 NEGAI | 88 | 5-7 | 40 | 7,10,15,20,26 | 5 | 80 | 0 | 4..0 |
| 10 NEGAI | 110 | 11-15,94-99 | 121 | 15,27,42,54,66,76,86,99 | 5 | 150 | 0 | all |
| 11 KATSUKO | 63 | 3-5 | 20 | 5 | 5 | 500 | 1 | 5..0 |
| 11 KATSUKO | 71 | 4-8 | 52 | 7,8,42 | 5 | 0 | 0 | 5..0 |
| 11 KATSUKO | 75 | 10-16 | 28 | 14,15,16 | 5 | 600 | 1 | 8..0 |
| 11 KATSUKO | 79 | 4-9 | 19 | 7,8,9 | 5 | 550 | 1 | 6..0 |

### Obj_IsInLowAttackEarlyWindowByChara 0x467A10 (was `Obj_IsInMoveFrameWindow2ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 67 | 10-12 | 26 | 18 | 2 | 550 | 1 | 13..9 |
| 1 DATE | 88 | 0-1 | 13 | 4 | 6 | 150 | 1 | 13..11 |
| 2 IMAGAWA | 118 | 12-25 | 43 | 11,29,30,31,32 | 5 | 800 | 1 | 38..9 |
| 3 AMIY | 67 | 3-3(t>=9) | 18 | 8 | 3 | 350 | 1 | 24..24 |
| 3 AMIY | 94 | 5-5(t>=6),6-6 | 19 | 10 | 6 | 350 | 1 | 28..10 |
| 4 MAEDA | 81 | 6-7 | 21 | - | - | - | - | - |
| 4 MAEDA | 82 | 5-5(t>=10),6-7 | 21 | - | - | - | - | - |
| 4 MAEDA | 114 | 3-12 | 31 | 16,17 | 2 | 1800 | 1 | 63..6 |
| 7 CYOSOKABE | 67 | 6-6(t>=8),7-8 | 20 | 12 | 2 | 550 | 1 | 20..5 |
| 9 UESUGI | 65 | 4-4(t>=10) | 14 | 7 | 2 | 550 | 1 | 20..20 |
| 9 UESUGI | 114 | 7-7 | 57 | 16,21,25,28,32,43 | 2 | 150 | 1 | 11..11 |
| 10 NEGAI | 65 | 6-6 | 29 | 11 | 6 | 500 | 1 | 7..7 |
| 11 KATSUKO | 67 | 4-4 | 20 | 9 | 6 | 400 | 1 | 10..10 |

### Obj_IsInLowAttackPreHitWindowByChara 0x467C50 (was `Obj_IsInMoveFrameRange3ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 67 | 13-18 | 26 | 18 | 2 | 550 | 1 | 7..0 |
| 0 KANAE | 102 | 9-12 | 18 | 12 | 2 | 600 | 1 | 9..0 |
| 0 KANAE | 104 | 10-15 | 29 | 15 | 2 | 900 | 1 | 9..0 |
| 0 KANAE | 110 | 5-10 | 56 | 9,10,46 | 2 | 0 | 0 | 8..0 |
| 1 DATE | 63 | 5-11 | 22 | 11,12 | 2 | 350 | 1 | 9..0 |
| 1 DATE | 65 | 2-2(t>=12),3-4 | 39 | 3,4,18 | 2 | 0 | 1 | 16..0 |
| 1 DATE | 88 | 2-4 | 13 | 4 | 6 | 150 | 1 | 3..0 |
| 1 DATE | 110 | 11-15 | 33 | 10,15,20 | 1 | 200 | 1 | 4..0 |
| 2 IMAGAWA | 67 | 3-6 | 17 | 6 | 6 | 400 | 1 | 5..0 |
| 2 IMAGAWA | 118 | 25-32 | 43 | 11,29,30,31,32 | 5 | 800 | 1 | 9..0 |
| 3 AMIY | 67 | 4-8 | 18 | 8 | 3 | 350 | 1 | 8..0 |
| 3 AMIY | 93 | 6-10 | 19 | 10 | 6 | 50 | 0 | 8..0 |
| 3 AMIY | 94 | 7-10 | 19 | 10 | 6 | 350 | 1 | 4..0 |
| 3 AMIY | 99 | 0-2 | 14 | 2,4 | 6 | 300 | 1 | 6..0 |
| 4 MAEDA | 81 | 8-13 | 21 | - | - | - | - | - |
| 4 MAEDA | 82 | 8-13 | 21 | - | - | - | - | - |
| 4 MAEDA | 114 | 13-17 | 31 | 16,17 | 2 | 1800 | 1 | 5..0 |
| 5 SANADA | 65 | 10-14 | 23 | 14 | 2 | 500 | 1 | 6..0 |
| 6 TOKUGAWA | 63 | 17-18 | 32 | 14,16,18 | 1 | 250 | 1 | 1..0 |
| 7 CYOSOKABE | 67 | 5-6 | 20 | 12 | 2 | 550 | 1 | 22..20 |
| 9 UESUGI | 65 | 5-7 | 14 | 7 | 2 | 550 | 1 | 7..0 |
| 9 UESUGI | 71 | 8-11 | 22 | 11 | 6 | 550 | 1 | 4..0 |
| 9 UESUGI | 114 | 8-16 | 57 | 16,21,25,28,32,43 | 2 | 150 | 1 | 8..0 |
| 10 NEGAI | 65 | 7-11 | 29 | 11 | 6 | 500 | 1 | 4..0 |
| 11 KATSUKO | 67 | 5-9 | 20 | 9 | 6 | 400 | 1 | 6..0 |

### Obj_IsInMidAttackEarlyWindowByChara 0x468100 (was `Obj_IsInMoveFrameWindow4ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 61 | 6-7 | 33 | 10,23 | 3 | 800 | 1 | 41..33 |
| 0 KANAE | 63 | 7-8 | 23 | 13 | 3 | 600 | 1 | 9..8 |
| 0 KANAE | 65 | 5-5 | 20 | 8,9,10 | 3 | 500 | 1 | 8..8 |
| 0 KANAE | 107 | 2-2 | 30 | 7,18 | 3 | 200 | 0 | 10..10 |
| 0 KANAE | 108 | 2-2 | 30 | 7,18 | 3 | 200 | 0 | 10..10 |
| 1 DATE | 71 | 4-4(t>=6) | 31 | 9,22 | 7 | 800 | 1 | 68..68 |
| 2 IMAGAWA | 63 | 4-7,12-17 | 50 | 9,10,11,17,40 | 7 | 0 | 0 | 16..0 |
| 2 IMAGAWA | 73 | 26-26 | 44 | 13,14,15,16,17,31,32,33,34,35 | 3 | 50 | 1 | 10..10 |
| 2 IMAGAWA | 77 | 10-14 | 27 | 18 | 7 | 550 | 1 | 25..18 |
| 2 IMAGAWA | 110 | 4-12 | 36 | 17,18,19,20,21,22,23,24 | 3 | 200 | 0 | 23..7 |
| 2 IMAGAWA | 111 | 0-8 | 27 | 10,11,12,13,14,15,16,17,18 | 7 | 100 | 0 | 39..3 |
| 3 AMIY | 69 | 6-6 | 24 | 10 | 7 | 300 | 1 | 9..9 |
| 4 MAEDA | 65 | 2-2(t>=16) | 14 | 5 | 3 | 775 | 1 | 23..23 |
| 4 MAEDA | 67 | 4-4(t>=8) | 20 | 7 | 3 | 650 | 1 | 18..18 |
| 4 MAEDA | 68 | 5-5 | 16 | 4,10 | 3 | 60 | 0 | 10..10 |
| 4 MAEDA | 77 | 10-14 | 24 | 15 | 5 | 700 | 1 | 9..1 |
| 4 MAEDA | 86 | 0-4 | 22 | 10 | 7 | 500 | 1 | 40..13 |
| 4 MAEDA | 110 | 7-10 | 27 | 15,16 | 2 | 0 | 0 | 55..8 |
| 4 MAEDA | 118 | 3-16 | 30 | 5,16,21 | 7 | 400 | 1 | 3..0 |
| 5 SANADA | 61 | 2-2(t>=10) | 12 | 5 | 7 | 500 | 1 | 19..19 |
| 5 SANADA | 63 | 10-12 | 27 | 9,15 | 1 | 150 | 1 | 22..8 |
| 5 SANADA | 71 | 7-11,31-34 | 54 | 17,40 | 3 | 800 | 1 | 24..8 |
| 5 SANADA | 75 | 2-2(t>=15) | 14 | 5 | 7 | 350 | 1 | 23..23 |
| 5 SANADA | 110 | 42-43 | 69 | 7,47,48,49 | 1 | 50 | 0 | 12..11 |
| 5 SANADA | 114 | 7-15 | 27 | - | - | - | - | - |
| 6 TOKUGAWA | 61 | 9-11 | 28 | 15,17 | 3 | 200 | 1 | 9..5 |
| 6 TOKUGAWA | 71 | 13-15 | 33 | 21,22 | 3 | 500 | 1 | 14..6 |
| 6 TOKUGAWA | 97 | 32-37,43-47,53-57 | 63 | - | - | - | - | - |
| 7 CYOSOKABE | 69 | 2-2(t>=12),7-8 | 49 | 6,29 | 3 | 0 | 0 | 30..114 |
| 7 CYOSOKABE | 73 | 4-7 | 44 | 10,32 | 3 | 0 | 1 | 20..15 |
| 7 CYOSOKABE | 77 | 2-2(t>=10) | 15 | 5 | 3 | 450 | 1 | 25..25 |
| 8 TAKEDA | 61 | 10-10 | 25 | 15 | 7 | 450 | 1 | 8..8 |
| 8 TAKEDA | 69 | 7-8,13-13 | 28 | 6,12,16 | 1 | 150 | 1 | 14..7 |
| 8 TAKEDA | 71 | 8-11 | 29 | 16 | 3 | 400 | 1 | 13..9 |
| 8 TAKEDA | 120 | 16-18 | 37 | 20,21,22 | 7 | 850 | 1 | 21..15 |
| 9 UESUGI | 67 | 8-8 | 21 | 14,15 | 3 | 475 | 1 | 10..10 |
| 10 NEGAI | 63 | 7-7(t>=6) | 23 | 9 | 7 | 650 | 1 | 13..13 |
| 10 NEGAI | 67 | 7-8 | 38 | 15 | 7 | 550 | 1 | 10..8 |
| 10 NEGAI | 71 | 9-10 | 33 | 16 | 7 | 650 | 1 | 10..8 |
| 11 KATSUKO | 65 | 7-7 | 23 | 14 | 7 | 500 | 1 | 12..12 |
| 11 KATSUKO | 69 | 8-8 | 27 | 13,14,15 | 7 | 450 | 1 | 17..17 |

### Obj_IsInMidAttackPreHitWindowByChara 0x468840 (was `Obj_IsInMoveFrameRange5ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 61 | 8-10,20-23 | 33 | 10,23 | 3 | 800 | 1 | 5..0 |
| 0 KANAE | 63 | 9-13 | 23 | 13 | 3 | 600 | 1 | 7..0 |
| 0 KANAE | 65 | 6-10 | 20 | 8,9,10 | 3 | 500 | 1 | 2..0 |
| 0 KANAE | 71 | 3-7 | 33 | 7,24 | 3 | 300 | 1 | 28..0 |
| 0 KANAE | 86 | 4-9 | 28 | 9,10,11,12 | 7 | 600 | 1 | 7..0 |
| 0 KANAE | 87 | 4-9 | 28 | 9,10,11,12 | 7 | 400 | 1 | 8..0 |
| 0 KANAE | 100 | 3-8 | 14 | 8 | 3 | 600 | 1 | 8..0 |
| 0 KANAE | 107 | 3-7 | 30 | 7,18 | 3 | 200 | 0 | 6..0 |
| 0 KANAE | 108 | 3-7 | 30 | 7,18 | 3 | 200 | 0 | 6..0 |
| 1 DATE | 61 | 4-9 | 21 | 9 | 7 | 400 | 1 | 6..0 |
| 1 DATE | 67 | 5-7 | 31 | 7 | 3 | 400 | 1 | 5..0 |
| 1 DATE | 71 | 5-9,19-22 | 31 | 9,22 | 7 | 800 | 1 | 8..0 |
| 1 DATE | 93 | 0-2 | 14 | 2 | 7 | 100 | 0 | 3..0 |
| 1 DATE | 110 | 19-20 | 33 | 10,15,20 | 1 | 200 | 1 | 1..0 |
| 1 DATE | 118 | 8-12 | 35 | 12,13,14 | 7 | 500 | 1 | 7..0 |
| 2 IMAGAWA | 61 | 6-10 | 19 | 10 | 7 | 400 | 1 | 8..0 |
| 2 IMAGAWA | 63 | 8-17 | 50 | 9,10,11,17,40 | 7 | 0 | 0 | 8..0 |
| 2 IMAGAWA | 65 | 6-13 | 29 | 9,13,19 | 7 | 50 | 0 | 7..0 |
| 2 IMAGAWA | 71 | 3-6 | 32 | 6,23 | 7 | 0 | 1 | 6..0 |
| 2 IMAGAWA | 73 | 8-17,27-35 | 44 | 13,14,15,16,17,31,32,33,34,35 | 3 | 50 | 1 | 10..0 |
| 2 IMAGAWA | 77 | 15-18 | 27 | 18 | 7 | 550 | 1 | 6..0 |
| 2 IMAGAWA | 110 | 12-24 | 36 | 17,18,19,20,21,22,23,24 | 3 | 200 | 0 | 7..0 |
| 2 IMAGAWA | 111 | 8-18 | 27 | 10,11,12,13,14,15,16,17,18 | 7 | 100 | 0 | 3..0 |
| 3 AMIY | 69 | 7-10 | 24 | 10 | 7 | 300 | 1 | 5..0 |
| 3 AMIY | 97 | 0-2 | 12 | 2 | 7 | 600 | 1 | 9..0 |
| 3 AMIY | 98 | 0-2 | 10 | 2 | 7 | 500 | 1 | 9..0 |
| 4 MAEDA | 61 | 2-4 | 12 | 4 | 7 | 600 | 1 | 7..0 |
| 4 MAEDA | 65 | 3-5 | 14 | 5 | 3 | 775 | 1 | 3..0 |
| 4 MAEDA | 67 | 5-7 | 20 | 7 | 3 | 650 | 1 | 3..0 |
| 4 MAEDA | 68 | 6-10 | 16 | 4,10 | 3 | 60 | 0 | 8..0 |
| 4 MAEDA | 71 | 6-8 | 16 | 8 | 3 | 600 | 1 | 7..0 |
| 4 MAEDA | 75 | 5-7,13-16 | 22 | 7,16 | 3 | 900 | 1 | 3..0 |
| 4 MAEDA | 77 | 8-18 | 24 | 15 | 5 | 700 | 1 | 12..-24 |
| 4 MAEDA | 86 | 5-10 | 22 | 10 | 7 | 500 | 1 | 12..0 |
| 4 MAEDA | 110 | 11-15 | 27 | 15,16 | 2 | 0 | 0 | 7..0 |
| 5 SANADA | 61 | 3-5 | 12 | 5 | 7 | 500 | 1 | 3..0 |
| 5 SANADA | 63 | 13-15 | 27 | 9,15 | 1 | 150 | 1 | 7..0 |
| 5 SANADA | 69 | 11-16 | 32 | 10,16 | 5 | 100 | 0 | 11..0 |
| 5 SANADA | 71 | 12-17,35-40 | 54 | 17,40 | 3 | 800 | 1 | 6..0 |
| 5 SANADA | 75 | 3-5 | 14 | 5 | 7 | 350 | 1 | 3..0 |
| 5 SANADA | 88 | 5-5 | 48 | 5,29,32,37 | 7 | 0 | 1 | 0..0 |
| 5 SANADA | 110 | 44-49 | 69 | 7,47,48,49 | 1 | 50 | 0 | 3..0 |
| 5 SANADA | 114 | 16-18 | 27 | - | - | - | - | - |
| 6 TOKUGAWA | 61 | 12-17 | 28 | 15,17 | 3 | 200 | 1 | 4..0 |
| 6 TOKUGAWA | 63 | 15-16 | 32 | 14,16,18 | 1 | 250 | 1 | 1..0 |
| 6 TOKUGAWA | 65 | 8-11 | 48 | 11,34 | 3 | 500 | 1 | 12..0 |
| 6 TOKUGAWA | 69 | 13-17,27-30 | 41 | 17,30 | 3 | 0 | 1 | 6..0 |
| 6 TOKUGAWA | 71 | 16-22 | 33 | 21,22 | 3 | 500 | 1 | 5..0 |
| 6 TOKUGAWA | 97 | 38-42,48-52,58-62 | 63 | - | - | - | - | - |
| 6 TOKUGAWA | 110 | 6-48 | 58 | 7,11,15,19,23,27,31,35,39,43,48 | 7 | 55 | 0 | 2..0 |
| 7 CYOSOKABE | 69 | 5-6 | 49 | 6,29 | 3 | 0 | 0 | 2..0 |
| 7 CYOSOKABE | 73 | 8-10 | 44 | 10,32 | 3 | 0 | 1 | 3..0 |
| 7 CYOSOKABE | 77 | 3-5 | 15 | 5 | 3 | 450 | 1 | 5..0 |
| 7 CYOSOKABE | 130 | 2-3 | 15 | 3 | 7 | 0 | 0 | 6..0 |
| 8 TAKEDA | 61 | 11-15 | 25 | 15 | 7 | 450 | 1 | 6..0 |
| 8 TAKEDA | 63 | 19-22 | 33 | 22 | 7 | 400 | 1 | 4..0 |
| 8 TAKEDA | 69 | 9-12,14-16 | 28 | 6,12,16 | 1 | 150 | 1 | 7..0 |
| 8 TAKEDA | 71 | 12-16 | 29 | 16 | 3 | 400 | 1 | 7..0 |
| 8 TAKEDA | 120 | 19-22 | 37 | 20,21,22 | 7 | 850 | 1 | 2..0 |
| 9 UESUGI | 61 | 7-13 | 22 | 13,14 | 7 | 500 | 1 | 7..0 |
| 9 UESUGI | 67 | 9-14 | 21 | 14,15 | 3 | 475 | 1 | 8..0 |
| 10 NEGAI | 63 | 8-9 | 23 | 9 | 7 | 650 | 1 | 1..0 |
| 10 NEGAI | 67 | 9-15 | 38 | 15 | 7 | 550 | 1 | 7..0 |
| 10 NEGAI | 69 | 12-21,37-43 | 58 | 16,17,18,19,21,41,42,43 | 7 | 80 | 0 | 5..0 |
| 10 NEGAI | 71 | 11-16 | 33 | 16 | 7 | 650 | 1 | 6..0 |
| 10 NEGAI | 87 | 8-10 | 26 | 7,10,15 | 5 | 100 | 0 | 2..0 |
| 11 KATSUKO | 61 | 5-10 | 20 | 10 | 7 | 400 | 1 | 7..0 |
| 11 KATSUKO | 65 | 8-14 | 23 | 14 | 7 | 500 | 1 | 11..0 |
| 11 KATSUKO | 69 | 9-15 | 27 | 13,14,15 | 7 | 450 | 1 | 11..0 |

### Obj_IsInWeakAttackPreHitWindowByChara 0x469270 (was `Obj_IsInMoveFrameRange6ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 68 | 3-4 | 11 | 4 | 7 | 50 | 0 | 2..0 |
| 0 KANAE | 83 | 2-8 | 23 | 8 | 7 | 50 | 0 | 14..0 |
| 1 DATE | 62 | 12-14 | 24 | 13,14 | 5 | 60 | 0 | 1..0 |
| 1 DATE | 92 | 7-9 | 19 | 8,9 | 5 | 100 | 0 | 1..0 |
| 1 DATE | 104 | 5-9 | 23 | 9,10 | 4 | 0 | 1 | 9..0 |
| 1 DATE | 105 | 5-9 | 23 | 9,10 | 4 | 0 | 1 | 9..0 |
| 2 IMAGAWA | 64 | 3-8 | 22 | 5,6,7,8 | 6 | 60 | 0 | 7..0 |
| 2 IMAGAWA | 68 | 4-6 | 15 | 5 | 3 | 20 | 0 | 1..-2 |
| 2 IMAGAWA | 76 | 4-8 | 18 | 7,8 | 5 | 85 | 0 | 4..0 |
| 2 IMAGAWA | 118 | 8-11 | 43 | 11,29,30,31,32 | 5 | 800 | 1 | 5..0 |
| 3 AMIY | 70 | 5-9 | 16 | 9 | 7 | 80 | 0 | 4..0 |
| 3 AMIY | 74 | 9-13 | 20 | 13 | 3 | 100 | 0 | 7..0 |
| 3 AMIY | 76 | 6-9 | 16 | 9 | 7 | 60 | 0 | 6..0 |
| 6 TOKUGAWA | 64 | 4-9 | 38 | 7,8,9 | 2 | 80 | 0 | 4..0 |
| 6 TOKUGAWA | 84 | 0-99 | 18 | 0,1,2,3,4,5,9,10 | 5 | 400 | 1 | all |
| 6 TOKUGAWA | 95 | 0-99 | 4 | 0,1,2 | 5 | 500 | 1 | all |
| 6 TOKUGAWA | 103 | 0-99 | 4 | 0 | 7 | 50 | 0 | all |
| 6 TOKUGAWA | 114 | 13-25 | 26 | - | - | - | - | - |
| 6 TOKUGAWA | 115 | 0-99 | 12 | 5,6 | 5 | 300 | 1 | all |
| 6 TOKUGAWA | 116 | 0-99 | 12 | 6 | 5 | 800 | 1 | all |
| 6 TOKUGAWA | 118 | 33-55 | 56 | - | - | - | - | - |
| 6 TOKUGAWA | 119 | 0-99 | 12 | 0,1,2,3,4,5,6,7,8,9,10 | 7 | 1100 | 1 | all |
| 7 CYOSOKABE | 60 | 2-3 | 8 | 3 | 3 | 80 | 0 | 2..0 |
| 7 CYOSOKABE | 62 | 5-7 | 16 | 7 | 1 | 200 | 0 | 7..0 |
| 7 CYOSOKABE | 68 | 5-7 | 14 | 7 | 1 | 100 | 0 | 3..0 |
| 7 CYOSOKABE | 70 | 4-8 | 16 | 8 | 7 | 250 | 0 | 7..0 |
| 7 CYOSOKABE | 74 | 6-8 | 20 | 8 | 7 | 120 | 0 | 5..0 |
| 8 TAKEDA | 64 | 11-13 | 21 | 13 | 7 | 75 | 0 | 2..0 |
| 9 UESUGI | 62 | 7-8 | 20 | 8 | 3 | 100 | 0 | 1..0 |
| 9 UESUGI | 76 | 13-13 | 22 | 9,10,11,12,13 | 5 | 50 | 0 | 0..0 |
| 9 UESUGI | 114 | 17-33 | 57 | 16,21,25,28,32,43 | 2 | 150 | 1 | 24..10 |
| 9 UESUGI | 119 | 0-3 | 21 | 3 | 0 | 600 | 1 | 5..0 |
| 10 NEGAI | 62 | 11-14 | 21 | 14 | 7 | 120 | 0 | 3..0 |
| 10 NEGAI | 68 | 7-12 | 21 | 10,11,12 | 7 | 120 | 0 | 5..0 |
| 10 NEGAI | 76 | 8-13 | 21 | 12,13 | 5 | 120 | 0 | 5..0 |
| 10 NEGAI | 88 | 8-26 | 40 | 7,10,15,20,26 | 5 | 80 | 0 | 2..0 |
| 10 NEGAI | 110 | 16-86 | 121 | 15,27,42,54,66,76,86,99 | 5 | 150 | 0 | all |
| 11 KATSUKO | 62 | 3-8 | 15 | 6,7,8 | 7 | 80 | 0 | 4..0 |
| 11 KATSUKO | 72 | 6-10 | 16 | 9 | 7 | 100 | 0 | 9..-2 |
| 11 KATSUKO | 76 | 3-14 | 23 | 8,11,14 | 5 | 20 | 0 | 8..0 |
| 11 KATSUKO | 92 | 9-14 | 20 | 13,14 | 6 | 600 | 1 | 6..0 |
| 11 KATSUKO | 93 | 9-14 | 21 | 13,14 | 5 | 500 | 1 | 6..0 |

### Obj_IsInPostHitRecoveryWindowByChara 0x4698F0 (was `Obj_IsInMoveFrameRange7ByChara`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 61 | 11-19,24-32 | 33 | 10,23 | 3 | 800 | 1 | 33..-24 |
| 0 KANAE | 63 | 14-22 | 23 | 13 | 3 | 600 | 1 | -2..-27 |
| 0 KANAE | 64 | 8-13 | 14 | 6,7 | 7 | 200 | 0 | -12..-28 |
| 0 KANAE | 65 | 10-19 | 20 | 8,9,10 | 3 | 500 | 1 | 0..-30 |
| 0 KANAE | 67 | 19-25 | 26 | 18 | 2 | 550 | 1 | -2..-23 |
| 0 KANAE | 69 | 11-18 | 19 | 10 | 5 | 600 | 1 | -4..-23 |
| 0 KANAE | 70 | 6-12 | 13 | 5 | 7 | 120 | 0 | -2..-27 |
| 0 KANAE | 71 | 8-16 | 33 | 7,24 | 3 | 300 | 1 | 66..27 |
| 0 KANAE | 73 | 9-16 | 25 | 8 | 7 | 800 | 1 | -3..-20 |
| 0 KANAE | 75 | 9-19 | 20 | 7,8 | 5 | 350 | 1 | -2..-48 |
| 0 KANAE | 77 | 10-18 | 19 | 9 | 5 | 550 | 1 | -2..-29 |
| 0 KANAE | 79 | 4-13 | 14 | 3 | 1 | 550 | 1 | -4..-32 |
| 0 KANAE | 83 | 9-22 | 23 | 8 | 7 | 50 | 0 | -2..-30 |
| 0 KANAE | 84 | 10-18 | 19 | 9 | 5 | 60 | 0 | -2..-40 |
| 0 KANAE | 86 | 13-27 | 28 | 9,10,11,12 | 7 | 600 | 1 | -8..-67 |
| 0 KANAE | 87 | 13-27 | 28 | 9,10,11,12 | 7 | 400 | 1 | -8..-61 |
| 0 KANAE | 103 | 9-19 | 20 | 8 | 1 | 900 | 1 | -12..-31 |
| 0 KANAE | 104 | 16-28 | 29 | 15 | 2 | 900 | 1 | -3..-60 |
| 0 KANAE | 107 | 19-29 | 30 | 7,18 | 3 | 200 | 0 | -12..-31 |
| 0 KANAE | 108 | 19-29 | 30 | 7,18 | 3 | 200 | 0 | -12..-53 |
| 0 KANAE | 110 | 11-19,47-55 | 56 | 9,10,46 | 2 | 0 | 0 | 78..-51 |
| 1 DATE | 61 | 10-20 | 21 | 9 | 7 | 400 | 1 | -2..-29 |
| 1 DATE | 62 | 15-23 | 24 | 13,14 | 5 | 60 | 0 | -7..-21 |
| 1 DATE | 63 | 13-21 | 22 | 11,12 | 2 | 350 | 1 | -1..-25 |
| 1 DATE | 65 | 5-9 | 39 | 3,4,18 | 2 | 0 | 1 | 50..28 |
| 1 DATE | 67 | 8-30 | 31 | 7 | 3 | 400 | 1 | -2..-45 |
| 1 DATE | 69 | 14-24 | 25 | 13 | 3 | 500 | 1 | -2..-49 |
| 1 DATE | 71 | 10-17,23-30 | 31 | 9,22 | 7 | 800 | 1 | 25..-24 |
| 1 DATE | 75 | 9-24 | 25 | 8 | 5 | 400 | 1 | -2..-44 |
| 1 DATE | 76 | 12-21 | 22 | 10,11 | 7 | 80 | 0 | -2..-23 |
| 1 DATE | 77 | 15-35 | 36 | 14 | 1 | 450 | 1 | -2..-55 |
| 1 DATE | 79 | 5-12 | 13 | 4 | 5 | 350 | 1 | -2..-34 |
| 1 DATE | 87 | 11-15 | 16 | - | - | - | - | - |
| 1 DATE | 88 | 5-12 | 13 | 4 | 6 | 150 | 1 | -3..-23 |
| 1 DATE | 92 | 10-18 | 19 | 8,9 | 5 | 100 | 0 | -11..-27 |
| 1 DATE | 93 | 3-13 | 14 | 2 | 7 | 100 | 0 | -3..-32 |
| 1 DATE | 97 | 18-27 | 28 | 17 | 5 | 500 | 1 | -8..-26 |
| 1 DATE | 98 | 18-27 | 28 | 17 | 5 | 500 | 1 | -8..-26 |
| 1 DATE | 102 | 22-27 | 28 | - | - | - | - | - |
| 1 DATE | 103 | 22-27 | 28 | - | - | - | - | - |
| 1 DATE | 104 | 11-22 | 23 | 9,10 | 4 | 0 | 1 | -2..-26 |
| 1 DATE | 105 | 11-22 | 23 | 9,10 | 4 | 0 | 1 | -2..-26 |
| 1 DATE | 110 | 21-32 | 33 | 10,15,20 | 1 | 200 | 1 | -3..-64 |
| 1 DATE | 118 | 15-34 | 35 | 12,13,14 | 7 | 500 | 1 | -2..-60 |
| 2 IMAGAWA | 61 | 11-18 | 19 | 10 | 7 | 400 | 1 | -2..-33 |
| 2 IMAGAWA | 62 | 6-15 | 16 | 5 | 1 | 80 | 0 | -3..-30 |
| 2 IMAGAWA | 63 | 18-26 | 50 | 9,10,11,17,40 | 7 | 0 | 0 | 42..18 |
| 2 IMAGAWA | 64 | 9-21 | 22 | 5,6,7,8 | 6 | 60 | 0 | -3..-36 |
| 2 IMAGAWA | 65 | 20-28 | 29 | 9,13,19 | 7 | 50 | 0 | -5..-53 |
| 2 IMAGAWA | 67 | 7-16 | 17 | 6 | 6 | 400 | 1 | -3..-39 |
| 2 IMAGAWA | 68 | 7-14 | 15 | 5 | 3 | 20 | 0 | -4..-24 |
| 2 IMAGAWA | 69 | 9-17 | 18 | 8 | 5 | 450 | 1 | -2..-39 |
| 2 IMAGAWA | 71 | 7-12 | 32 | 6,23 | 7 | 0 | 1 | 52..28 |
| 2 IMAGAWA | 73 | 18-25,36-43 | 44 | 13,14,15,16,17,31,32,33,34,35 | 3 | 50 | 1 | 43..-38 |
| 2 IMAGAWA | 74 | 12-19 | 0 | - | - | - | - | - |
| 2 IMAGAWA | 76 | 9-17 | 18 | 7,8 | 5 | 85 | 0 | -5..-34 |
| 2 IMAGAWA | 77 | 19-26 | 27 | 18 | 7 | 550 | 1 | -5..-25 |
| 2 IMAGAWA | 78 | 10-16,24-26 | 27 | 5 | 5 | 200 | 0 | -53..-86 |
| 2 IMAGAWA | 79 | 4-14 | 15 | 4 | 5 | 400 | 1 | 0..-37 |
| 2 IMAGAWA | 82 | 5-11 | 12 | 4 | 7 | 200 | 0 | -7..-34 |
| 2 IMAGAWA | 83 | 4-11 | 12 | 3 | 5 | 150 | 0 | -5..-27 |
| 2 IMAGAWA | 91 | 9-17 | 18 | 7,8 | 5 | 300 | 1 | -2..-40 |
| 2 IMAGAWA | 101 | 5-21 | 22 | - | - | - | - | - |
| 2 IMAGAWA | 102 | 3-21 | 22 | - | - | - | - | - |
| 2 IMAGAWA | 110 | 27-35 | 36 | 17,18,19,20,21,22,23,24 | 3 | 200 | 0 | -11..-64 |
| 2 IMAGAWA | 111 | 19-26 | 27 | 10,11,12,13,14,15,16,17,18 | 7 | 100 | 0 | -6..-47 |
| 2 IMAGAWA | 118 | 33-42 | 43 | 11,29,30,31,32 | 5 | 800 | 1 | -2..-50 |
| 3 AMIY | 61 | 11-19 | 20 | 10 | 3 | 300 | 1 | -2..-35 |
| 3 AMIY | 63 | 17-20 | 21 | 8 | 5 | 300 | 1 | -30..-35 |
| 3 AMIY | 65 | 15-25 | 26 | 14 | 1 | 300 | 1 | -2..-32 |
| 3 AMIY | 67 | 9-17 | 18 | 8 | 3 | 350 | 1 | -2..-18 |
| 3 AMIY | 69 | 11-23 | 24 | 10 | 7 | 300 | 1 | -2..-50 |
| 3 AMIY | 70 | 10-15 | 16 | 9 | 7 | 80 | 0 | -2..-17 |
| 3 AMIY | 71 | 8-17 | 31 | 7 | 7 | 0 | 1 | -3..-53 |
| 3 AMIY | 74 | 14-19 | 20 | 13 | 3 | 100 | 0 | -4..-24 |
| 3 AMIY | 76 | 10-15 | 16 | 9 | 7 | 60 | 0 | -4..-13 |
| 3 AMIY | 79 | 5-12 | 13 | 4 | 5 | 250 | 1 | -3..-22 |
| 3 AMIY | 82 | 9-15 | 16 | 8 | 5 | 150 | 0 | -2..-34 |
| 3 AMIY | 85 | 5-11 | 12 | 4 | 7 | 60 | 0 | -3..-31 |
| 3 AMIY | 89 | 10-23 | 24 | 7,9 | 1 | 150 | 0 | -5..-49 |
| 3 AMIY | 93 | 11-18 | 19 | 10 | 6 | 50 | 0 | -4..-46 |
| 3 AMIY | 94 | 11-18 | 19 | 10 | 6 | 350 | 1 | -4..-49 |
| 3 AMIY | 97 | 3-11 | 12 | 2 | 7 | 600 | 1 | -5..-40 |
| 3 AMIY | 98 | 3-9 | 10 | 2 | 7 | 500 | 1 | -3..-30 |
| 3 AMIY | 99 | 5-13 | 14 | 2,4 | 6 | 300 | 1 | -8..-27 |
| 3 AMIY | 100 | 11-14 | 15 | 3 | 5 | 500 | 1 | -57..-66 |
| 3 AMIY | 101 | 4-16 | 17 | 3 | 7 | 550 | 1 | -8..-64 |
| 3 AMIY | 102 | 4-14 | 15 | 3 | 5 | 550 | 1 | -3..-41 |
| 3 AMIY | 103 | 3-10 | 11 | 2 | 5 | 600 | 1 | -5..-25 |
| 3 AMIY | 104 | 3-14 | 15 | 2 | 5 | 1000 | 1 | -6..-55 |
| 3 AMIY | 110 | 27-36 | 37 | 8,11,14,17,26 | 5 | 50 | 0 | -1..-23 |
| 3 AMIY | 114 | 14-23 | 24 | 11,12,13 | 7 | 1200 | 1 | -3..-28 |
| 3 AMIY | 118 | 0-99 | 29 | - | - | - | - | all |
| 4 MAEDA | 61 | 5-11 | 12 | 4 | 7 | 600 | 1 | -2..-37 |
| 4 MAEDA | 63 | 14-20 | 21 | 13 | 1 | 600 | 1 | -3..-45 |
| 4 MAEDA | 65 | 6-13 | 14 | 5 | 3 | 775 | 1 | -1..-55 |
| 4 MAEDA | 67 | 8-19 | 20 | 7 | 3 | 650 | 1 | -2..-52 |
| 4 MAEDA | 68 | 11-15 | 16 | 4,10 | 3 | 60 | 0 | -2..-28 |
| 4 MAEDA | 69 | 7-15 | 16 | 6 | 1 | 500 | 1 | -2..-55 |
| 4 MAEDA | 70 | 13-19 | 20 | 7,11,12 | 1 | 140 | 0 | -4..-30 |
| 4 MAEDA | 71 | 9-15 | 16 | 8 | 3 | 600 | 1 | -2..-46 |
| 4 MAEDA | 72 | 7-14 | 15 | 5,6 | 1 | 150 | 0 | -2..-24 |
| 4 MAEDA | 73 | 8-15 | 24 | 7 | 1 | 800 | 1 | -2..-42 |
| 4 MAEDA | 74 | 8-16 | 17 | 7 | 1 | 120 | 0 | -2..-28 |
| 4 MAEDA | 75 | 8-12,17-21 | 22 | 7,16 | 3 | 900 | 1 | 39..-44 |
| 4 MAEDA | 76 | 6-14 | 15 | 5 | 5 | 200 | 0 | -6..-36 |
| 4 MAEDA | 77 | 16-23 | 24 | 15 | 5 | 700 | 1 | -3..-36 |
| 4 MAEDA | 79 | 7-14 | 15 | 3 | 5 | 800 | 1 | -14..-35 |
| 4 MAEDA | 86 | 11-21 | 22 | 10 | 7 | 500 | 1 | -3..-51 |
| 4 MAEDA | 87 | 9-15 | 16 | - | - | - | - | - |
| 4 MAEDA | 88 | 9-15 | 16 | - | - | - | - | - |
| 4 MAEDA | 110 | 17-26 | 27 | 15,16 | 2 | 0 | 0 | -6..-51 |
| 4 MAEDA | 114 | 18-30 | 31 | 16,17 | 2 | 1800 | 1 | -2..-44 |
| 4 MAEDA | 118 | 22-29 | 30 | 5,16,21 | 7 | 400 | 1 | -2..-59 |
| 5 SANADA | 61 | 6-11 | 12 | 5 | 7 | 500 | 1 | -4..-29 |
| 5 SANADA | 63 | 16-26 | 27 | 9,15 | 1 | 150 | 1 | -2..-25 |
| 5 SANADA | 65 | 15-22 | 23 | 14 | 2 | 500 | 1 | -4..-34 |
| 5 SANADA | 67 | 13-23 | 24 | 12 | 1 | 600 | 1 | -3..-32 |
| 5 SANADA | 69 | 17-22 | 32 | 10,16 | 5 | 100 | 0 | -3..-24 |
| 5 SANADA | 71 | 18-30,41-53 | 54 | 17,40 | 3 | 800 | 1 | 46..-26 |
| 5 SANADA | 73 | 8-14 | 15 | 7 | 5 | 500 | 1 | -1..-32 |
| 5 SANADA | 75 | 6-13 | 14 | 5 | 7 | 350 | 1 | -3..-36 |
| 5 SANADA | 79 | 4-14 | 15 | 3 | 1 | 600 | 1 | -2..-28 |
| 5 SANADA | 81 | 8-15 | 16 | 7 | 5 | 300 | 1 | -2..-31 |
| 5 SANADA | 82 | 8-14 | 15 | 7 | 1 | 500 | 1 | -2..-26 |
| 5 SANADA | 83 | 10-21 | 22 | 5,6,7,8,9 | 5 | 800 | 1 | -2..-31 |
| 5 SANADA | 87 | 6-18 | 48 | 5,29,32,37 | 7 | 0 | 1 | 62..27 |
| 5 SANADA | 88 | 6-18 | 48 | 5,29,32,37 | 7 | 0 | 1 | 62..27 |
| 5 SANADA | 110 | 50-68 | 69 | 7,47,48,49 | 1 | 50 | 0 | -2..-58 |
| 5 SANADA | 114 | 21-26 | 27 | - | - | - | - | - |
| 5 SANADA | 118 | 11-17 | 18 | - | - | - | - | - |
| 6 TOKUGAWA | 61 | 18-27 | 28 | 15,17 | 3 | 200 | 1 | -3..-44 |
| 6 TOKUGAWA | 63 | 19-31 | 32 | 14,16,18 | 1 | 250 | 1 | -3..-47 |
| 6 TOKUGAWA | 64 | 10-37 | 38 | 7,8,9 | 2 | 80 | 0 | -2..-82 |
| 6 TOKUGAWA | 65 | 12-24 | 48 | 11,34 | 3 | 500 | 1 | 84..28 |
| 6 TOKUGAWA | 67 | 9-18 | 19 | 8 | 5 | 450 | 1 | -1..-51 |
| 6 TOKUGAWA | 69 | 18-26,31-40 | 41 | 17,30 | 3 | 0 | 1 | 34..-36 |
| 6 TOKUGAWA | 71 | 23-32 | 33 | 21,22 | 3 | 500 | 1 | -8..-45 |
| 6 TOKUGAWA | 73 | 43-56 | 57 | 40,41,42 | 5 | 500 | 1 | -2..-36 |
| 6 TOKUGAWA | 75 | 25-36 | 37 | 23,24 | 5 | 800 | 1 | -3..-65582 |
| 6 TOKUGAWA | 76 | 6-17 | 18 | 5 | 6 | 120 | 0 | -2..-44 |
| 6 TOKUGAWA | 77 | 12-16 | 43 | 5,6,7,28 | 5 | 0 | 0 | 37..26 |
| 6 TOKUGAWA | 79 | 9-23 | 24 | 8 | 5 | 500 | 1 | -1..-39 |
| 6 TOKUGAWA | 98 | 0-99 | 13 | - | - | - | - | all |
| 6 TOKUGAWA | 110 | 49-57 | 58 | 7,11,15,19,23,27,31,35,39,43,48 | 7 | 55 | 0 | -2..-26 |
| 6 TOKUGAWA | 111 | 1-12 | 13 | 0 | 7 | 80 | 0 | -2..-82 |
| 7 CYOSOKABE | 61 | 13-21 | 22 | 12 | 1 | 500 | 1 | -2..-32 |
| 7 CYOSOKABE | 62 | 8-15 | 16 | 7 | 1 | 200 | 0 | -2..-29 |
| 7 CYOSOKABE | 63 | 18-31 | 32 | 9,10,11,12,13,14,15,17 | 7 | 300 | 1 | -2..-37 |
| 7 CYOSOKABE | 65 | 8-14 | 30 | 7,21 | 1 | 0 | 1 | 100..46 |
| 7 CYOSOKABE | 67 | 13-19 | 20 | 12 | 2 | 550 | 1 | -2..-39 |
| 7 CYOSOKABE | 68 | 8-13 | 14 | 7 | 1 | 100 | 0 | -2..-25 |
| 7 CYOSOKABE | 69 | 7-13 | 49 | 6,29 | 3 | 0 | 0 | 117..70 |
| 7 CYOSOKABE | 70 | 9-15 | 16 | 8 | 7 | 250 | 0 | -4..-19 |
| 7 CYOSOKABE | 71 | 26-42 | 43 | 24,25 | 1 | 600 | 1 | -3..-44 |
| 7 CYOSOKABE | 73 | 11-18 | 44 | 10,32 | 3 | 0 | 1 | 110..67 |
| 7 CYOSOKABE | 74 | 9-19 | 20 | 8 | 7 | 120 | 0 | -2..-26 |
| 7 CYOSOKABE | 75 | 7-13 | 14 | 7 | 1 | 300 | 1 | 0..-19 |
| 7 CYOSOKABE | 77 | 6-14 | 15 | 5 | 3 | 450 | 1 | -2..-30 |
| 7 CYOSOKABE | 79 | 13-30 | 31 | 10,11,12 | 5 | 600 | 1 | -2..-65 |
| 7 CYOSOKABE | 110 | 24-34 | 57 | 23 | 7 | 0 | 0 | -3..-36 |
| 7 CYOSOKABE | 118 | 19-34 | 35 | 18 | 5 | 1280 | 1 | -2..-37 |
| 7 CYOSOKABE | 126 | 13-44 | 45 | 10,11,12 | 5 | 1500 | 1 | -2..-78 |
| 7 CYOSOKABE | 129 | 7-14 | 15 | 3 | 7 | 0 | 0 | -12..-61 |
| 7 CYOSOKABE | 130 | 4-14 | 15 | 3 | 7 | 0 | 0 | -3..-71 |
| 8 TAKEDA | 61 | 16-24 | 25 | 15 | 7 | 450 | 1 | -2..-31 |
| 8 TAKEDA | 63 | 23-32 | 33 | 22 | 7 | 400 | 1 | -4..-24 |
| 8 TAKEDA | 64 | 14-20 | 21 | 13 | 7 | 75 | 0 | -5..-30 |
| 8 TAKEDA | 65 | 15-22 | 23 | 14 | 3 | 350 | 1 | -3..-24 |
| 8 TAKEDA | 67 | 12-21 | 22 | 11 | 5 | 366 | 1 | -2..-24 |
| 8 TAKEDA | 69 | 17-27 | 28 | 6,12,16 | 1 | 150 | 1 | -4..-34 |
| 8 TAKEDA | 71 | 17-28 | 29 | 16 | 3 | 400 | 1 | -4..-35 |
| 8 TAKEDA | 72 | 17-20 | 21 | 6,12,16 | 2 | 40 | 0 | -2..-20 |
| 8 TAKEDA | 73 | 19-31 | 32 | 18 | 5 | 400 | 1 | -2..-26 |
| 8 TAKEDA | 75 | 19-31 | 32 | 18 | 5 | 400 | 1 | -2..-26 |
| 8 TAKEDA | 77 | 11-18 | 19 | 10 | 5 | 333 | 1 | -4..-23 |
| 8 TAKEDA | 79 | 4-13 | 14 | 3 | 1 | 450 | 1 | -2..-27 |
| 8 TAKEDA | 87 | 17-25 | 26 | 10,11,12,13,14,15,16 | 5 | 100 | 0 | -2..-15 |
| 8 TAKEDA | 110 | 23-28 | 29 | 5,9,13,22 | 6 | 50 | 1 | -4..-20 |
| 8 TAKEDA | 111 | 19-24 | 25 | 5,9,18 | 1 | 60 | 0 | -4..-24 |
| 8 TAKEDA | 113 | 22-57 | 58 | 12,13,14,19,20,21 | 5 | 0 | 0 | -2..-146 |
| 8 TAKEDA | 120 | 23-36 | 37 | 20,21,22 | 7 | 850 | 1 | -2..-38 |
| 8 TAKEDA | 124 | 30-53 | 54 | 23,24,25,26,27,28,29 | 5 | 30 | 0 | -5..-71 |
| 8 TAKEDA | 125 | 0-99 | 26 | - | - | - | - | all |
| 9 UESUGI | 61 | 15-21 | 22 | 13,14 | 7 | 500 | 1 | -2..-47 |
| 9 UESUGI | 62 | 9-19 | 20 | 8 | 3 | 100 | 0 | -3..-23 |
| 9 UESUGI | 63 | 8-13 | 14 | 7 | 5 | 525 | 1 | -2..-44 |
| 9 UESUGI | 64 | 3-9 | 10 | 2 | 2 | 80 | 0 | -2..-40 |
| 9 UESUGI | 65 | 8-13 | 14 | 7 | 2 | 550 | 1 | -2..-42 |
| 9 UESUGI | 67 | 16-20 | 21 | 14,15 | 3 | 475 | 1 | -4..-26 |
| 9 UESUGI | 68 | 7-14 | 15 | 6 | 6 | 100 | 0 | -2..-29 |
| 9 UESUGI | 69 | 15-20,27-32 | 33 | 12,13,14,26 | 5 | 250 | 1 | 33..-18 |
| 9 UESUGI | 70 | 14-17 | 18 | 5,13 | 5 | 100 | 0 | -4..-17 |
| 9 UESUGI | 71 | 12-21 | 22 | 11 | 6 | 550 | 1 | -2..-41 |
| 9 UESUGI | 72 | 4-13 | 14 | 3 | 7 | 80 | 0 | -2..-47 |
| 9 UESUGI | 73 | 5-8 | 21 | 4 | 5 | 750 | 1 | -3..-22 |
| 9 UESUGI | 74 | 8-19 | 20 | 7 | 7 | 200 | 0 | -2..-39 |
| 9 UESUGI | 75 | 4-11 | 0 | - | - | - | - | - |
| 9 UESUGI | 76 | 14-21 | 22 | 9,10,11,12,13 | 5 | 50 | 0 | -6..-46 |
| 9 UESUGI | 77 | 11-16 | 17 | 10 | 5 | 450 | 1 | -2..-36 |
| 9 UESUGI | 78 | 5-9 | 10 | 2 | 5 | 200 | 0 | -7..-15 |
| 9 UESUGI | 79 | 4-12 | 13 | 3 | 5 | 500 | 1 | -3..-24 |
| 9 UESUGI | 86 | 7-14 | 15 | - | - | - | - | - |
| 9 UESUGI | 87 | 3-14 | 7 | 3,5 | 7 | 150 | 1 | 0..-8 |
| 9 UESUGI | 92 | 8-19 | 32 | - | - | - | - | - |
| 9 UESUGI | 93 | 4-4(t>=5),5-19 | 32 | - | - | - | - | - |
| 9 UESUGI | 110 | 10-18 | 19 | 7,8,9 | 1 | 700 | 1 | -2..-58 |
| 9 UESUGI | 114 | 44-56 | 57 | 16,21,25,28,32,43 | 2 | 150 | 1 | -4..-46 |
| 9 UESUGI | 118 | 14-21 | 22 | - | - | - | - | - |
| 9 UESUGI | 119 | 4-12 | 21 | 3 | 0 | 600 | 1 | -4..-42 |
| 10 NEGAI | 61 | 12-24 | 25 | 11 | 5 | 600 | 1 | -2..-32 |
| 10 NEGAI | 62 | 15-20 | 21 | 14 | 7 | 120 | 0 | -2..-26 |
| 10 NEGAI | 63 | 10-22 | 23 | 9 | 7 | 650 | 1 | -2..-47 |
| 10 NEGAI | 65 | 12-28 | 29 | 11 | 6 | 500 | 1 | -2..-52 |
| 10 NEGAI | 67 | 16-37 | 38 | 15 | 7 | 550 | 1 | -2..-48 |
| 10 NEGAI | 68 | 13-20 | 21 | 10,11,12 | 7 | 120 | 0 | -5..-26 |
| 10 NEGAI | 69 | 22-34,44-57 | 58 | 16,17,18,19,21,41,42,43 | 7 | 80 | 0 | 44..-51 |
| 10 NEGAI | 71 | 17-32 | 33 | 16 | 7 | 650 | 1 | -2..-44 |
| 10 NEGAI | 76 | 14-20 | 21 | 12,13 | 5 | 120 | 0 | -2..-13 |
| 10 NEGAI | 77 | 10-22 | 23 | 7,8,9 | 5 | 750 | 1 | -2..-35 |
| 10 NEGAI | 79 | 11-23 | 24 | 9,10 | 5 | 500 | 1 | -4..-37 |
| 10 NEGAI | 81 | 8-14 | 15 | 7 | 5 | 200 | 1 | -1..-43 |
| 10 NEGAI | 82 | 7-13 | 14 | 6 | 5 | 200 | 1 | -1..-45 |
| 10 NEGAI | 83 | 13-20 | 21 | 12 | 5 | 600 | 1 | -5..-23 |
| 10 NEGAI | 84 | 13-26 | 27 | 12 | 5 | 600 | 1 | -5..-37 |
| 10 NEGAI | 87 | 16-25 | 26 | 7,10,15 | 5 | 100 | 0 | -3..-20 |
| 10 NEGAI | 88 | 27-39 | 40 | 7,10,15,20,26 | 5 | 80 | 0 | -3..-55 |
| 10 NEGAI | 114 | 15-29 | 30 | 13,14 | 4 | 1200 | 1 | -2..-34 |
| 11 KATSUKO | 61 | 11-19 | 20 | 10 | 7 | 400 | 1 | -3..-33 |
| 11 KATSUKO | 62 | 9-14 | 15 | 6,7,8 | 7 | 80 | 0 | -3..-18 |
| 11 KATSUKO | 63 | 6-19 | 20 | 5 | 5 | 500 | 1 | -3..-28 |
| 11 KATSUKO | 64 | 9-17 | 18 | 7,8 | 6 | 80 | 0 | -2..-28 |
| 11 KATSUKO | 65 | 15-22 | 23 | 14 | 7 | 500 | 1 | -3..-47 |
| 11 KATSUKO | 67 | 10-19 | 20 | 9 | 6 | 400 | 1 | -4..-33 |
| 11 KATSUKO | 68 | 6-12 | 13 | 5 | 7 | 80 | 0 | -2..-23 |
| 11 KATSUKO | 69 | 16-26 | 27 | 13,14,15 | 7 | 450 | 1 | -3..-27 |
| 11 KATSUKO | 70 | 7-11 | 12 | 5,6 | 5 | 120 | 0 | -4..-21 |
| 11 KATSUKO | 71 | 9-15,43-51 | 52 | 7,8,42 | 5 | 0 | 0 | 76..-20 |
| 11 KATSUKO | 72 | 11-15 | 16 | 9 | 7 | 100 | 0 | -4..-23 |
| 11 KATSUKO | 75 | 17-27 | 28 | 14,15,16 | 5 | 600 | 1 | -3..-43 |
| 11 KATSUKO | 76 | 15-22 | 23 | 8,11,14 | 5 | 20 | 0 | -3..-20 |
| 11 KATSUKO | 77 | 8-19 | 20 | 6,7 | 5 | 600 | 1 | -2..-34 |
| 11 KATSUKO | 79 | 10-18 | 19 | 7,8,9 | 5 | 550 | 1 | -3..-37 |
| 11 KATSUKO | 81 | 7-16 | 17 | 6 | 6 | 120 | 0 | -2..-31 |
| 11 KATSUKO | 86 | 11-19 | 20 | - | - | - | - | - |
| 11 KATSUKO | 87 | 23-29 | 30 | 21,22 | 5 | 300 | 1 | -3..-41 |
| 11 KATSUKO | 92 | 15-19 | 20 | 13,14 | 6 | 600 | 1 | -5..-29 |
| 11 KATSUKO | 93 | 15-20 | 21 | 13,14 | 5 | 500 | 1 | -3..-28 |
| 11 KATSUKO | 100 | 16-28 | 29 | 15 | 5 | 800 | 1 | -2..-51 |
| 11 KATSUKO | 104 | 20-29 | 30 | - | - | - | - | - |
| 11 KATSUKO | 108 | 20-27 | 28 | - | - | - | - | - |
| 11 KATSUKO | 112 | 16-28 | 29 | - | - | - | - | - |

### Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Set 0x466660 (was `Obj_IsInPattern16Or19StartupWindowA`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 19 | 0-4 | 13 | 3 | 7 | 200 | 1 | 19..-2 |
| 1 DATE | 16 | 0-5 | 13 | 5 | 5 | 400 | 1 | 17..0 |
| 2 IMAGAWA | 16 | 0-3 | 10 | 3,4 | 5 | 800 | 1 | 11..0 |
| 3 AMIY | 16 | 0-3 | 11 | 3 | 5 | 400 | 1 | 13..0 |
| 4 MAEDA | 16 | 0-6 | 16 | 6 | 1 | 550 | 1 | 15..0 |
| 5 SANADA | 16 | 0-2 | 10 | 2 | 5 | 200 | 1 | 9..0 |
| 7 CYOSOKABE | 16 | 0-3 | 10 | 3 | 5 | 700 | 1 | 10..0 |
| 8 TAKEDA | 16 | 0-7 | 13 | 7 | 5 | 400 | 1 | 16..0 |
| 9 UESUGI | 19 | 0-7 | 21 | 7 | 7 | 400 | 1 | 21..0 |
| 10 NEGAI | 16 | 0-11 | 25 | 11 | 5 | 300 | 1 | 15..0 |
| 11 KATSUKO | 16 | 0-5 | 14 | 5 | 5 | 500 | 1 | 19..0 |

### Obj_IsInPat16Or19AtkStartup_ReplyGuardDir8Set 0x466770 (was `Obj_IsInPattern16Or19StartupWindowB`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 0 KANAE | 16 | 0-3 | 12 | 4 | 6 | 400 | 1 | 20..1 |
| 2 IMAGAWA | 19 | 0-6 | 14 | 5,6 | 6 | 800 | 1 | 12..0 |
| 3 AMIY | 19 | 0-8 | 19 | 8 | 6 | 400 | 1 | 17..0 |
| 4 MAEDA | 19 | 0-7 | 20 | 7 | 6 | 525 | 1 | 23..0 |
| 5 SANADA | 19 | 0-3 | 14 | 3 | 6 | 300 | 1 | 8..0 |
| 6 TOKUGAWA | 19 | 0-6 | 16 | 6 | 6 | 600 | 1 | 13..0 |
| 11 KATSUKO | 19 | 0-5 | 13 | 5 | 6 | 550 | 1 | 13..0 |

### Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Or8Set 0x466850 (was `Obj_IsInPattern16Or19StartupWindowC`)

| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |
|---|---|---|---|---|---|---|---|---|
| 6 TOKUGAWA | 16 | 0-6 | 19 | 6 | 7 | 600 | 1 | 27..0 |
| 7 CYOSOKABE | 19 | 0-8 | 14 | 8 | 7 | 750 | 1 | 18..0 |
| 10 NEGAI | 19 | 0-11 | 25 | 11 | 7 | 550 | 1 | 29..0 |
