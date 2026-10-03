# GOF2 live evidence (copy `C:\dev\hantei-chan\work\gof_game`, GOF2.exe, windowed)

Raw files: `docs/formats/evidence/gof2_live_*.txt|png`; scripts: `tools/gof2/live/` (Windows python3 ctypes: ReadProcessMemory sampler, int3 debugger harness with WOW64 contexts,
input injector). The IDB is `C:\dev\ida\server\GOF2.exe.i64`.

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

## Contradicted (and renamed in the IDB)
* Sets A/B/C are not guardMask 5/6/7: A contains KANAE 19 and UESUGI 19 (mask 7) and MAEDA 16 (mask 1); C = mask 7 chars. They select which reply command the AI sends (A 0x404, B 0x408, C either).
  Renamed `..StandGuardOnlySet/CrouchGuardOnlySet/AnyGuardSet` -> `Obj_IsInPat16Or19AtkStartup_ReplyGuardDir4Set / ReplyGuardDir8Set / ReplyGuardDir4Or8Set`.
* 0x404/0x408 are not "stand guard / crouch guard": they start pattern 15 / 18 (see above). Doc text corrected.
* `Obj_ProcessTurnAround` -> `Obj_ProcessInputAndStateTransitions`. Also named: `ObjTintFx_StartShock` (43EF50), `ObjTintFx_StartKind4NoSe` (43F450), `ObjTintFx_StartFreezeDot` (43F1D0), `ObjEffect_BurnDotCallback` (4EC080), `ObjEffect_ShockDotCallback` (4EC100).

## Unconfirmed
* Latches `obj+0x11D8/0x11DC`: stayed 0 in all live 16/19 (about 20 occurrences). They need an attack-box clash (contact event type 8) while in 16/19; the single contact seen was the enemy hitting the 15 stance (not a clash).
  Needs a human-vs-human VS setup with both sides injected.
* Whether the AI's high/low windows (A/B/C) fire in response to a real 16/19 attacker: the CPU never used 16/19 in these runs; only the consumer code and the command outputs were confirmed.
* What 15/18 do defensively (parry-like absorb on f3) was seen once; hurt-box set 1 in the early frames is not decoded.
* Hit memory (`hitHistoryByAttackerPattern`) not sampled.
