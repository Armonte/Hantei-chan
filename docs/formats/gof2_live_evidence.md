# GOF2 live evidence (copy `C:\dev\hantei-chan\work\gof_game`, GOF2.exe, windowed)

Raw files: `docs/formats/evidence/gof2_live_*.txt|png`; scripts: `tools/gof2/live/` (Windows python3 ctypes: ReadProcessMemory sampler, int3 debugger harness with WOW64 contexts,
input injector; second run: `gof2_clash_sample.py` (8 ms sampler with worldX) and `gof2_clash_force.py` (forced pattern requests)). The IDB is `C:\dev\ida\server\GOF2.exe.i64`.

## Method
* Fighters: `g_PlayerCharaObj[2]` (0x65B928) -> Gof2Obj (0x1264 bytes). Sampled every ~8 ms: patternNo (+0x26C), frameNo (+0x270), facing, key word
  `obj->keyCmdRef[2]` (+0x10D8), clash state (+0x6C0), latches +0x11D8/+0x11DC, attackRecord (+0x620) -> guardMask (+0x7C), attackActive (+0x67C), hurt boxes (+0x6F0), chara id `*(charaContext+16155*4)`.
* Input: SendInput scancodes did not reach the game's DirectInput keyboard buffer (checked with `kbstate.py`), so P1 input was injected by a debugger breakpoint at 0x409249
  (inside `Input_BuildControllerRawWords`) that overwrites controller raw word `ctrl[i]+0x144`. Story mode vs the CPU was used (CPU = real AI).
* Sound: int3 on `Sound_RestartBufferAtVolume` 0x456FC0 (eax = slot, return address logged) and on the DirectSound `Lock` call 0x456DFA inside the buffer loader (slot, wav bytes).
  Status effects forced through `HitJudge_StartHitEffect` (0x4317D0, obj in edi, args kind,duration) called from a remote stub on the P1 fighter. Note: the debugger must treat WOW64 single-step as exception 0x4000001E to re-arm breakpoints.

## Confirmed
1. **Key bits** (`gof2_live_input_map.txt`): keyboard ctrl: H=dir bit 4, N=dir 8, B=2, M=1, Z=0x100 light, X=0x200 heavy, C=0x400 guard.
2. **Direction 8 = crouch, 4 = stand side** (`..._direction_attack_guardmask.txt`): hurt box top is -211 with dir 8 vs -241 neutral / -245 dir 4 (pattern 0 frames); down+light = pattern 66 whose attack record
   guardMask is 6 (crouch-guard only = low); up+light = pattern 74, guardMask 1 (stand-guard only = high). guardMask bit0 = stand guard blocks, bit1 = crouch guard blocks (matches doc).
3. **Patterns 15/16/17 and 18/19** (`..._pat15_16_18_19_timeline.txt`): Guard(0x400) + dir 4/5/6 -> pattern 15 (KANAE 8 frames), Guard + dir 8/9/A -> 18. 15 -> 16 -> 17 and 18 -> 19 happen when an attack button is pressed
   in the stance (keys 0x2000606 / 0x2000608), or, for 15, when an enemy attack connects during its frame-3 hold (pattern 16 started with hitstop 36, enemy `clashState`=1). Guard alone gives 49 -> 50 -> 51.
   `Obj_ProcessInputAndStateTransitions` is the requester (name corrected). Gauge check `DefStatus+48/+52` is 0 live, so it always passes.
4. **Attack records** (`..._pat16_19_guardmask.txt`): KANAE 16 attack f4 guardMask 6, KANAE 19 f3 guardMask 7, DATE 16 f5 guardMask 5 - identical to the static emulated tables.
5. **AI**: CPU emitted 0x408 from neutral and got pattern 18 (not a guard pattern) in `gof2_live_ai_guard_commands.txt`; consumer code shows set A -> 0x404, set B -> 0x408, set C -> mix.
6. **Status SE slots** (`gof2_live_sound_slots.txt`, `gof2_live_wav_size_match.txt`): burn tick 91 -> slot 134 from `ObjTintFx_TickHitFlashCycle`; burn dot 115 -> 135 (`..Effect115_SeBurnDot`);
   shock 93 -> 136; shock dot 117 -> 137; freeze 92 -> 138 (`ObjTintFx_TickHitFlashFixed`); freeze dot 116 -> 139. The 39 buffers loaded in slots 101..139 have exactly the data sizes of SYSTEMSE.FOB entries
   1..39 (null.wav entry 0 = slot 100/0), so slot 134..139 = se_burn, se_burn_dot, se_shock, se_shock_dot, se_freeze, se_freeze_dot. Kind 4 (`ObjTintFx_StartKind4NoSe`) plays no sound; kinds 3 and 5 both use shock.

7. **Clash latches `obj+0x11D8 / +0x11DC`** (second live run; raw: `gof2_live_clash_pat19_latch.txt`, `gof2_live_clash_pat16_forced.txt`, `gof2_live_clash_forced_matrix.txt` (raw), `gof2_live_clash_forced_summary.txt` (one line per trial)).
   * Natural case (Story fight, `..._pat19_latch.txt`): the CPU (side 1, chara 3) was in pattern 19 f8 with its attack active; P1 stood in guard stance pattern 18 f2. On the same tick the CPU object got `clashState`(+0x6C0)=1, **`+0x11DC`=1**, hitstop 52; P1 got hitstop 36 and went 18 -> 19 (the stance's counter). P1's own latches stayed 0. The CPU's latch stayed 1 through its hit-reaction patterns 68, 28, 31 and cleared when it returned to pattern 0 (action start, `Obj_ResetTransientStateOnActionStart`).
   * Forced trials (`tools/gof2/live/gof2_clash_force.py`, demo battle where both sides are AI; `Obj_RequestPattern` 0x430450 called from a remote stub thread on attacker and defender, defender x poked next to the attacker): 16 vs stance 15 -> `+0x11D8`=1 on the attacker, hitstop 52 vs 36/40, defender goes 15 -> 16 (`..._pat16_forced.txt` + summary, 4 trials, both sides as attacker); 19 vs stance 18 -> `+0x11DC`=1 (3 trials incl. the natural one); 19 vs stance 15 -> `+0x11DC`=1 (1 trial). The latch is always on the attacker, never on the stance holder.
   * Negative controls: 16/19 attacks that hit a defender who is not in 15/18 (hit-reaction patterns 28/29/31..., hitstops 24..117) never set a latch (about 10 trials plus 4 natural P1 16/19 episodes that hit the CPU in `cs1`); mutual trades 16 vs 16, 19 vs 19, 16 vs 19, 19 vs 16 (hitstops on both sides, about 7 trials) never set a latch; 16 vs 18 once connected with no latch (not decoded: the stance box probably did not overlap).
   * So the latches are not generic attack-vs-attack clash flags: they record "my pattern-16 (`+0x11D8`) / pattern-19 (`+0x11DC`) attack was caught by an opposing counter stance (15/18 clash box)". `Obj_ProcessInputAndStateTransitions` refuses Guard+dir4/5/6 -> pattern 15 while `+0x11D8` is set and Guard+dir8/9/A -> pattern 18 while `+0x11DC` is set (live: after the 19 clash the CPU did 18 -> 19 only again after its action reset). Fields renamed `hitDuringAction16/19` -> `stanceClashLatchAction16/19` (Gof2Obj, Gof2ObjYarareState, Gof2ObjTailC in the IDB and the generated headers).
   * The gate `HitJudge_DetectAttackClashForObj` 0x49BB70 / `HitJudge_ResolveAttackClashPass` 0x49C150 is NOT what feeds these latches: it walks attack boxes against the victim hurt boxes of roots in the block patterns 49..51 / 21..23 / 52 / 53 (pendingHitClass -> 21..23 or 52/53 reactions, `ObjHitFlags_RecordGuarded`, effect 60001), i.e. a block-contact pass, not the contact-event list. Name left as is (not re-verified live); the latches come from the contact-event type 8 in `Obj_ApplyContactEventList` (contact kind 2 -> `ObjHitFlags_RecordClash`).

## Contradicted (and renamed in the IDB)
* Sets A/B/C are not guardMask 5/6/7: A contains KANAE 19 and UESUGI 19 (mask 7) and MAEDA 16 (mask 1); C = mask 7 chars. They select which reply command the AI sends (A 0x404, B 0x408, C either).
  Renamed `..StandGuardOnlySet/CrouchGuardOnlySet/AnyGuardSet` -> `Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Set / ReplyGuardDir8Set / ReplyGuardDir4Or8Set`.
* 0x404/0x408 are not "stand guard / crouch guard": they start pattern 15 / 18 (see above). Doc text corrected.
* `Obj_ProcessTurnAround` -> `Obj_ProcessInputAndStateTransitions`. Also named: `ObjTintFx_StartShock` (43EF50), `ObjTintFx_StartKind4NoSe` (43F450), `ObjTintFx_StartFreezeDot` (43F1D0), `ObjEffect_BurnDotCallback` (4EC080), `ObjEffect_ShockDotCallback` (4EC100).

## Unconfirmed
* Whether the AI's high/low windows (A/B/C) fire in response to a real 16/19 attacker: the CPU never used 16/19 in these runs; only the consumer code and the command outputs were confirmed.
* What 15/18 do defensively (parry-like absorb on f3) was seen once; hurt-box set 1 in the early frames is not decoded.
* Hit memory (`hitHistoryByAttackerPattern`) not sampled.
