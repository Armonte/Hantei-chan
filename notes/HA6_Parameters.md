# HA6 Parameter Reference

Complete reference for HA6 file format parameters with UNI/Dengeki and MBAACC compatibility.

## Legend

- ✅ = Supported
- ❌ = Not supported
- ⚠️ = Partial support or notes

---

## Animation Header Parameters

| Internal Name | Value Type | Description | Additional Notes | UNI/Dengeki | MBAACC |
|---------------|------------|-------------|------------------|-------------|---------|
| **PTCN** | String (max 255?) | Animation name for code reference using SetPattern() function | | ✅ | ✅ |
| **PTT2** | String (max 255?) | Internal name just for reference in HA6 file | | ✅ | ✅ |
| **PSTS** | | | | | ✅ |
| **PLVL** | 1 int32 | Skill level. Set if is weak, medium or strong. Also has separated classification for lever (elevation) moves | `0`: For weak attacks. A cancel attack is possible with the same animation when hit.<br>`10`: For medium attack. Even if it hits, the same animation cannot be produced.<br>`20`: For heavy attacks. Almost the same as 10.<br>`25`: Lever moves | | ✅ |
| **PTIT** | | | Never used on any of the three games. Maybe useless | ✅ | ✅ |
| **PUPS** | 1 int32 | Set palette used in current animation | `00` is default. `01` is for _p1.pal extra palette, `02` is _p2.pal, and so on | ✅ | ❌ |
| **PFLG** | 1 int32 | Info for the editor about the type of move. No use in game | Preset values, and only used for 01 and 02 in melty. Value: 0 | ✅ | ✅ |
| **PDST** | | | | | ✅ |
| **PDS2** | 8 int32 | Animation general info header. Always the last parameter in Animation header | List of values:<br>1. Always 20 hex value as int32<br>2. Total number of frames (FSTR)<br>3. Total number of HRNM + HRAT<br>4. Total number of EF<br>5. Total number of IF<br>6. Total of ATST<br>7. Always 00<br>8. Total of ASST<br>9. Total number of frames again | ✅ | ✅ |

---

## Frame Parameters (AF)

| Internal Name | Value Type | Description | Additional Notes | UNI/Dengeki | MBAACC |
|---------------|------------|-------------|------------------|-------------|---------|
| **AFID** | 1 int32 | Add a number (called ID) that can be used in code | | ✅ | ❌ |
| **AFD[X] / AFDL** | AFD from 0 to 9 as char<br>AFDL is 1 int32 | Set time to current frame | | ✅ | ✅ |
| **AFF[X] / AFFL** | AFF1 or 2 as char<br>AFFL 1 int32 | AFF1 to continue to next frame.<br>AFF2 for loop.<br>AFFL value 3 for animation end | | ✅ | ✅ |
| **AFFE** | 1 uint32 | Binary bitwise that control AFJP, AFJC and AFF2 | Add bitwise list later | ✅ | ✅ |
| **AFPR** | 1 int32 | Set a image priority. The values are fixed to a preset. If you exceed the preset max value, the parameter does nothing | Add preset list later | ✅ | ✅ |
| **AFHK** | boolean as int32 | Activate interpolation with value 1. Deactivate with 0 | | ✅ | ✅ |
| **AFCT** | 1 uint32 | Loop counter for when you use finite loops | | ✅ | ✅ |
| **AFJP** | 1 int32 | When the frame ends, jump to the specified animation or frame. Which one depends on AFFE. AFJP controls the id only | Depends on AFFE, this value can be positive or negative. Negative is for relative jump from current frame position | ✅ | ✅ |
| **AFJC** | 1 int32 | Jump to another frame or animation when landing to ground during an in-Air animation | AFFE decides if is animation or frame id | ✅ | ✅ |
| **AFPA** | 4 bytes | Set a value that can be used in character script. Much like AFID, but support from 1 to 255 value per each byte, so you can do 4 different references as much in the same frame | | ✅ | ❌ |
| **AFLP** | boolean as int32 | Jump param when loop count matches afct and affe flags are set | | ✅ | ✅ |
| **AFJH** | boolean as int32 | ????? | | ✅ | ❌ |
| **AFGP** | 1 boolean as int32<br>1 int32 | First int32 is to set if is a sprite (value 0) or effect (PAT file, value 1). Second is to load the sprite/effect id | This is like an old version of AFGX in layer, mostly for melty. It can use most of the parameters in layer sheet | | ✅ |

---

## Layer Parameters (AFGX-related)

| Internal Name | Value Type | Description | Additional Notes | UNI/Dengeki | MBAACC |
|---------------|------------|-------------|------------------|-------------|---------|
| **AFGX** | 1 boolean as int32<br>2 int32 | First int32 is to set the layer of current sprite/effect.<br>Second int32 is to set if is a sprite (value 0) or effect (value 1).<br>Third int32 is to load the sprite/effect id | Each AFGX can use the parameters below until another AFGX or common property is found | ✅ | ❌ |
| **AFOF** | 2 int32 | Set x and y axis of sprite/effect position. This doesn't affect the object position itself in-game | The unit used is pixel | ✅ | ✅ |
| **AFZM** | 2 float | Resize width and height respectively of the current sprite/effect. Use 1.0 float value for default size (same for other float props) | According to maso notes, this maybe have different behaviour on melty. It can only reduce the size, not enlarge | ✅ | ✅ |
| **AFAL** | 1 int32<br>1 byte as int32 | The first int32 is to set transparency filter mode.<br>The second int32 is for transparency value (from 0 to 255) | 0 is full transparency and 255 is full opaque.<br>Add mode preset values later | ✅ | ✅ |
| **AFGR** | 3 int32 | Set an overlay color with R G and B channel numbers respectively | | ✅ | ✅ |
| **AFAX** | 1 float | Do rotation with X axis on current sprite/effect | | ✅ | ✅ |
| **AFAY** | 1 float | Do rotation with Y axis on current sprite/effect | | ✅ | ✅ |
| **AFAZ** | 1 float | Do rotation with Z axis on current sprite/effect. It's like rotate from the center of the image | | ✅ | ✅ |
| **AFTN** | 2 boolean as int32 | If first boolean is 1, sprite/effect rotate 180º on X axis. Second boolean is the same for Y | | ✅ | ✅ |
| **AFAN** | 1 float | Same as AFAX??? | | ✅ | ✅ |
| **AFPL** | 1 int32 | Set a layer priority. Need to check if this for when you have several layers on same frame. It goes before AFOF prop | List of preset values:<br>`00` = None<br>`01` = Always first<br>`02` = Always last<br>`03` = None? (test with three layers)<br>`04` = None? (test with three layers)<br>`05` and far make the game crash | ✅ | ❌ |
| **AFRT** | 1 boolean(?) as int32 | According to maso, is display???? | | ✅ | ✅ |

---

## State Parameters (AS)

| Internal Name | Value Type | Description | Additional Notes | UNI/Dengeki | MBAACC |
|---------------|------------|-------------|------------------|-------------|---------|
| **ASAA** | 1 int32 | Set number of hits of the attack | Once you set it, this value is retained for the entire animation, so you can set it in non-attack frames before an attack happens | ✅ | ✅ |
| **ASVX** | 1 int32 | Stop vector movement | | ✅ | ✅ |
| **ASV0** | 5 int32 | Initialize vector movement | 1. Determines vector setting performance so as to avoid overwriting current vec values or to set _Vector_Keep<br>2. X speed movement<br>3. Y speed movement<br>4. X acceleration<br>5. Y acceleration | ✅ | ✅ |
| **ASCT** | 1 int32 | Set counter hit timing. Use preset values | `0` = No set (設定なし)<br>`1` = HI check (HI発生)<br>`2` = LO check (LO発生)<br>`3` = Erase (消去) | ✅ | ✅ |
| **ASCN** | 1 boolean as int32 | Cancel animation with basic attacks (6A, 6B, 6C...) | `0` = None<br>`1` = When hit (guarding or not)<br>`2` = Always<br>`3` = Only when damage enemy (HIT) | ✅ | ✅ |
| **ASS[X]** | X is 1 or 2 as char | Set frame state. No using this parameter treats it like standing. 1 treats it as in-air frame. 2 treats it as crouching frame | | ✅ | ✅ |
| **ASMX** | 1 int32 | Increase acceleration on X movement. Or set X max acceleration/speed, according to maso | MaxX typically used to limit a negative addx to prevent reaching an x value below 0 | ✅ | ✅ |
| **ASCS** | 1 int32 | Cancel animation with special or super attacks (236A, 63214BC, 2AB, ...) Use preset values (if like ha4) | `0` = None<br>`1` = When hit (guarding or not)<br>`2` = Always<br>`3` = Only when damage enemy (HIT) | ✅ | ✅ |
| **ASMV** | 1 boolean as int32 | Player can Act (move and attack in general) | | ✅ | ✅ |
| **AST0** | 5 int32 | Initialize vector movement. Pretty sure this is the cursed preset vector anim version so bottom line: never use anything but asv0 | 1. Always 10 or 11 hex<br>2. X speed movement<br>3. Y speed movement<br>4. X acceleration<br>5. Y acceleration<br>6. ????<br>7. ???? | ✅ | ✅ |
| **ASV1** | 1 int32?? | Unused | | | ✅ |
| **ASVA** | 1 int32?? | Unused | | | ✅ |
| **ASVC** | | Unused | | | ✅ |
| **ASYS** | 1 int32 | Player timer-esque invincibility on the ha6 side | This is the only way to add throw invincibility on the ha6 side but also allows you to set a damage invincibility flag as well | | ✅ |
| **ASCF** | 1 int32 as boolean | Shana 236A uses this. Always along asct and when use projectiles | | ✅ | ❌ |
| **ASDF** | 1 int32? | Unused | | | ✅ |
| **ASCL** | 2 int32? | Unused | | | ✅ |
| **ASSS** | | Unused | | | ✅ |
| **ASKV** | | Unused | | | ✅ |
| **ASF[X]** | 0, 1, 2, or 3 as char and 1 int32 | AsStatusFlags | `1` = EX<br>`8` = ChainShift | ✅ | ✅ |
| **ASSE** | 1 int32? | Unused | | | ✅ |
| **ASSM** | 1 int32 | Reuse ASST of previous frames in current frame | | ✅ | ✅ |

---

## Attack Parameters (AT)

| Internal Name | Value Type | Description | Additional Notes | UNI/Dengeki | MBAACC |
|---------------|------------|-------------|------------------|-------------|---------|
| **ATGD** | 1 int32 as bitwise | Set flags for guard properties | 1. Blockable in Standing<br>2. Blockable in-Air<br>3. Blockable in Crouching<br>4. ?????<br>5. ?????<br>6. ?????<br>7. ?????<br>8. ?????<br>9. Can't hit if enemy is Standing<br>10. Can't hit if enemy is in-Air<br>11. Can't hit if enemy is Crouching<br>12. Can't hit if enemy is in Bound<br>13. Can't hit if enemy is in Block Stun<br>14. ?????<br>15. Can hit only in Bound<br>16. Can't hit Playable Character | ✅ | ✅ |
| **ATV2** | 14 int32 | Set vector ids for the different states when hit the enemy, guarding or not | 1. Always 03<br>2. Always 02<br>3. Standing HIT vector id<br>4. If 1, vector don't move on X. If 2, reverse the vector on X<br>5. Standing GUARD vector id<br>6. same as 4 property<br>7. Air HIT vector id<br>8. same as 4 prop<br>9. Air GUARD vector id<br>10. same as 4 prop<br>11. Crouching HIT vector id<br>12. same as 4 prop<br>13. Crouching GUARD vector id<br>14. same as 4 prop | ✅ | ❌ |
| **ATHE** | 2 int32 | Set common effect used when hit. Second int32 is always 00? | HitMarkList defined in vectortable, hitmark mv created in charatblfunc | ✅ | ✅ |
| **ATHH** | 1 int32 | Damage proration. It works like a percentage, so 95 reduces base damage during combo by 5%. No proration is 100, and greater than that increases base damage | Its percentage value modifies Hosei value in code? | ✅ | ❌ |
| **ATHS** | 1 int32 | Damage correction value if combo starter | | ✅ | ⚠️ (at least it appears on exe) |
| **ATAT** | 1 int32 | Base damage of the move | | ✅ | ✅ |
| **ATSU** | 1 int32 | Set time that enemy can't do Ukemi (can't recover) | | ✅ | ✅ |
| **ATSP** | 1 int32 | Preset values. Set the hit-stun stop time when hit the enemy, guarding or not | `0`. Weak<br>`1`. Medium<br>`2`. Strong<br>`3`. None<br>`4`. Long<br>`5`. Very long<br>`6`. Very weak<br>HitStopList is defined in vectortable | ✅ | ✅ |
| **ATF1** | 1 int32 as bitwise | Set flags for attack various props | 1. Damage enemy in guard status<br>2. Can't KO enemy<br>3. Can't hit enemy again during Stun<br>4-5. <br>6. Don't increase combo counter<br>7. Screen shaking effect during Stun<br>8-9. <br>10. Child objects can damage Playable Char<br>11. Hit-Stun don't stop Playable Char<br>12-25. <br>26. No wait for Stun end on multihit<br>27-29. <br>30. Block enemy blast during Stun<br>31-32. | ✅ | ✅ |
| **ATCA** | 1 int32 | Add super meter on HIT | | ✅ | ✅ |
| **ATSA** | 1 int32 | Add player stun time. Time that the player is stunned when hit | | ✅ | ❌ |
| **ATGN** | 1 int32 | Used for general stun time on enemy if ATSH doesn't appear. If there is ATSH, this is to set stun time on GUARD status only | | ✅ | ✅ |
| **ATSH** | 1 int32 | Set stun time to enemy on HIT | | ✅ | ❌ |
| **ATC0** | 3 int32 | Hit stun time proration. Is like percentage, and higher values do higher reductions. Max value is 100 | 1. Hit stun time reduction<br>2. Combopoint set if combo starter<br>3. Combopoint SMP modifier (up to 255, higher is more extreme effect) | ✅ | ❌ |
| **ATAM** | 1 int32 | Attack minimum damage percentage | | ✅ | ❌ |
| **ATNG** | 1 int32 as bitwise | Grab properties as flags | 1. Enabled<br>2. Target Collision<br>3. Target Origin<br>4. Beat Player Timer Invulnerability | ✅ | ✅ |
| **ATUH** | | | | | ✅ |
| **ATS[X]** | | | | | ✅ |
| **ATF2** | 1 int32 as bitwise? | Looking at the name, I'm pretty sure is another set of flags like ATF1, but it's not confirmed. Tried to use and flags do nothing??? | | ✅ | ✅ |
| **ATBT** | 1 int32 | Break time? Guard break? | | ❌ | ✅ |
| **ATKK** | 1 int32 | Grant effect? | | | ✅ |
| **ATSN** | | | | | ✅ |
| **ATGS** | | | | | ✅ |
| **ATHV** | 4 int32 | HIT vector for standing, air and crouching state. First int32 is always 03 | Old version of ATV2 | | ✅ |
| **ATHT** | | | | | ✅ |
| **ATGV** | 4 int32 | GUARD vector for standing, air and crouching state. First int32 is always 03 | Old version of ATV2 | | ✅ |
| **ATVV** | 4 short | First is stunning, second is attack power, third for guard reduction, fourth is super meter gain | | | ✅ |
| **ATKZ** | | | | | ✅ |
| **ATBG** | 1 int32 | The attack can do guard break (nullify advanced guard pushblock) | Only used with yusa emi in dengeki. Value 02 | ✅ | ✅ |
| **ATBC** | | | | ❌ | |
| **ATAB** | | | | | ✅ |
| **ATGE** | 2 int32 | Load a common sound effect? Second int32 is always 00? | | | ✅ |
| **ATRF** | | | | ❌ | |

---

## Notes

### MBAACC-Specific Parameters
Parameters that only work in MBAACC and not UNI/Dengeki are primarily related to:
- Palette switching (PUPS)
- Advanced layer system (AFGX, AFPL)
- Enhanced damage/stun system (ATV2, ATHH, ATSH, ATC0, ATAM, ATSA)
- Guard break mechanics (ATBT)

### UNI/Dengeki-Specific Parameters
Parameters that only work in UNI/Dengeki:
- Frame IDs (AFID, AFPA)
- Advanced layer priority (AFPL)
- Jump helper (AFJH)
- Special counter flag (ASCF)

### Deprecated/Old Parameters
Some parameters are old versions superseded by newer ones:
- **AFGP** (old version of AFGX) - MBAACC only
- **AST0** (cursed preset vector, use ASV0 instead)
- **ATHV/ATGV** (old version of ATV2) - MBAACC only

### Unknown/Unused Parameters
Many parameters are marked as unused or their functionality is unknown. These include:
- ASV1, ASVA, ASVC, ASDF, ASCL, ASSS, ASKV, ASSE
- ATUH, ATS[X], ATSN, ATGS, ATHT, ATKZ, ATAB

---

## Effect Parameters (EF)

Effects control spawning, visual effects, audio, damage, and various gameplay mechanics.

### EF Type 1 - Spawn Actor

Spawns a pattern/actor at a position relative to the parent.

| Parameter | Description |
|-----------|-------------|
| **no** | Pattern to spawn |
| **param1** | X offset |
| **param2** | Y offset |
| **param3** | Flagset 1 (bitwise) |
| **param4** | Flagset 2 (bitwise) |
| **param8** | Angle (clockwise, 10000 = 360°) |
| **param9** | Amount to decrease projectile variable by on despawn |

**Flagset 1 bits:**
- `bit 0 [1]` = Remove when parent gets hit (includes shield, throw, throw tech; doesn't include clash, armor). Condition 27 still runs
- `bit 1 [2]` = Always face right (maybe other effects) / Often used with LA backgrounds
- `bit 2 [4]` = Follow parent
- `bit 3 [8]` = Affected by hitstop of parent
- `bit 4 [16]` = Coordinates relative to camera / Use absolute coordinates
- `bit 5 [32]` = Remove when parent changes pattern
- `bit 6 [64]` = Can go below floor level
- `bit 7 [128]` = Always on floor
- `bit 8 [256]` = Inherit parent's rotation / Synced random rotation (used mostly by effect.ha6)
- `bit 9 [512]` = Position relative to screen border in the back
- `bit 10 [1024]` = ?
- `bit 11 [2048]` = Flip facing
- `bit 12 [4096]` = Unknown or no effect (Akiha's 217 only)

**Flagset 2 bits:**
- `bit 0 [1]` = Child has shadow
- `bit 1 [2]` = ?? Advance child with parent (also activates Flagset 1 bit 5)
- `bit 2 [4]` = Remove when parent is thrown
- `bit 3 [8]` = Parent is affected by hitstop
- `bit 4 [16]` = ??
- `bit 5 [32]` = ??
- `bit 6 [64]` = Unaffected by parent's Type 2 superflash if spawned during it / Doesn't freeze on super flash
- `bit 7 [128]` = Doesn't move with camera
- `bit 8 [256]` = Position is relative to opponent
- `bit 9 [512]` = Position is relative to (-32768, 0)
- `bit 10 [1024]` = Top Z Priority (gets overridden if projectile sets own priority)
- `bit 11 [2048]` = Face according to player slot
- `bit 12 [4096]` = Unaffected by any superflash

### EF Type 2 - Various Effects

| Number | Effect | Parameters |
|--------|--------|------------|
| **4** | Random spawning sparkle/speed line | p1: X1, p2: Y1, p3: X2, p4: Y2, p5: Stretch? |
| **26** | (Blood) Heat | p1: X, p2: Y |
| **50** | Superflash | p1: X, p2: Y, p3: 0 freeze self, 1 don't freeze self (freezes own projectiles), 2 don't freeze self or opponent (or projectiles), 3 no flash, p4: Freeze/blackout effect duration (0 defaults to 30f), p5: 0 EX portrait, 1 AD portrait, 255 no portrait, p9: Don't spend meter (non-zero values don't spend meter but still set the meter spend value), p10: Meter gain multiplier value (requires param9 != 0; default is 110; 256 = 1.0x), p11: Meter gain multiplier value time (requires param9 != 0; default is 420f) |
| **210** | Unknown (ciel only) | |
| **260** | Unknown (arc aoko only) | |
| **261** | Trailing effect | p1: X offset, p2: Y offset, p3: pattern to spawn as trail, p4: width (x) of trail sprite |
| **1260** | Fading circle effect (Boss aoko only) | p3: color (from an index?), p4: No idea but has to be different from 6, p5: Sprite scale. Always spawns at origin |

### EF Type 3 - Spawn Preset Effect at Position

Spawns preset procedural effects (not patterns). All types use param1 for X and param2 for Y position.

| Number | Effect | Additional Parameters |
|--------|--------|----------------------|
| **1** | Jump looking effect | p3: Duration, p4: Size, p5: Growth rate |
| **2** | David star (lags the game, unused) | |
| **3** | Red hitspark | p3: Intensity of glow, p4: Intensity of sparks |
| **4** | Force field (unused) | |
| **6** | Fire (unused) | |
| **7** | Snow (unused) | |
| **8** | Blue flash (unused) | |
| **9** | Blue hitspark | p3: Intensity of sparks |
| **10** | Superflash background (has unused parameters) | |
| **15** | Blue horizontal wave (unused) | |
| **16** | Red vertical wave (unused) | |
| **19** | Foggy rays (unused) | |
| **20** | 3D rotating waves (unused) | |
| **23** | Blinding effect | p3: Fade in duration?, p4: color (0 white, 1 black, 2 red), p5: If 1, don't obscure characters, p6: Fade out duration? |
| **24** | Blinding effect 2 (Same as 23 but with different curve) | |
| **27** | Dust cloud | p3: X speed, p4: Y speed, p5: Duration of particles, p6: Color (0 brown, 1 black, 2 purple, 3 white), p7: If odd, particles go only toward one side (probably a flag), p8: Amount of particles |
| **28** | Dust (large, same as 27 but bigger particles) | |
| **29** | Dust (rotating, same as 28 but particles rotate faster and there's no purple) | |
| **30** | Massive dust cloud (unused) | |

### EF Type 4 - Set Opponent State on Grab (Don't Reset Bounces)

| Parameter | Description |
|-----------|-------------|
| **no** | Pattern to set the opponent in (usually only 23, 24, 26, 30, 350, 354) |
| **param1** | X pos of opponent |
| **param2** | Y pos of opponent |
| **param3** | Unknown. Found in old airthrows. Probably a leftover from older versions |
| **param4** | Flags (bitwise) |
| **param5** | Vector id. Works only if animation plays |
| **param6** | Untech time. 0 = infinite. Only works for airstun vectors |
| **param7** | Unknown. Most of the time it's 0 |
| **param8** | Opponent's frame in the pattern |

**Param4 flags:**
- `bit 0 [1]` = Play animation
- `bit 1 [2]` = Reverse vector
- `bit 2 [4]` = Can't otg
- `bit 3 [8]` = ??
- `bit 4 [16]` = ??
- `bit 5 [32]` = Hard knockdown

### EF Type 5 - Damage

| Number | Effect | Parameters |
|--------|--------|------------|
| **0** | Set added effect | p1: Type (1 burn, 2 freeze/ice, 3 shock/electrify, 4 confuse, 100 black sprite) |
| **1** | Damage opponent | p1: damage, p2: Add to hit count? (0-2), p3: Hitstop, p4: Hit sound effect (same as AT data), p5: Hit scaling? (0,1), p6: VS damage |
| **4** | Unknown (obsolete?) | p1: Unknown, always set to -10. Sometimes used just after effect 5 no 1 |

### EF Type 6 - Various Effects

Buttload of effects controlling gameplay mechanics. Known effects:

| Number | Effect | Parameters |
|--------|--------|------------|
| **0** | After-image (one every 7 frames; lasts until new pattern) | p1: Blending mode (0-11, see blending modes below) |
| **1** | Screen effects | p1: screen shake duration, p2: screen effect (0=dim, 1=black bg, 2=blue bg, 3=red bg, 4=white bg, 5=red bg with silhouette chars, 31=ex flash facing right, 32=ex flash facing left), p3: param2 duration, p4: slowdown duration |
| **2** | Invulnerability | p1: strike invulnerability duration, p2: throw invulnerability duration |
| **3** | Trailing images | p1: Blending mode (0-11), p2: Number of after-images, p3: How many frames each after-image is behind the previous one (location only, all show the current frame; max 32) |
| **4** | Gauges / Regenerate HP (red life only?) | p1: Health change (may not exceed Red Health), p2: Meter change, p3: MAX/HEAT/BLOOD HEAT time change (only works if effect owner is in MAX/HEAT/BLOOD HEAT regardless of player state), p4: Red Health change |
| **5** | Turnaround behavior / Unknown (used in throws) | p1: 0=Turnaround regardless of input, 1=Turn to face opponent, 2=Turnaround on 1/4/7 input, 3=Turnaround on 3/6/9 input, 4=Always face right, 5=Always face left, 6=Random / Also value 6 randomly makes actors rotate in the opposite direction |
| **6** | Set movement vector | p1: minimum speed, p2: maximum speed (actual equation is param1 + rand()%(param2 - param1)), p3: minimum acceleration, p4: maximum acceleration (actual equation is param3 + rand()%(param4 - param3)), p5: 0 = X, 1 = Y, p12: 1 = param4 = angle |
| **9** | Set No Input flag | p1: value |
| **10** | Various effects | p1: 0=armor, 1=confusion; p2: duration |
| **11** | Super flash | p1: duration of global flash (only if param2 != 0), p2: set player flash to 0 if 0 |
| **12** | Various teleports / Move parent to pos | p5: 1=Opponent position, 2=Child position. See details below |
| **14** | Gauges of character within special box | p1: 0=Enemies only, 1=Allies only, 2=Both; p2: Health change, p3: Meter change, p4: MAX/HEAT/BLOOD HEAT time change |
| **15** | Start/Stop Music | p1: 0 = stop, 1 = start |
| **16** | Directional movement / Go to camera relative position | p12: 0=Directional Movement (p1: base angle degrees clockwise, p2: random angle range, p3: base speed, p4: random speed range), 1=Go to camera relative position (p1: X, p2: Y, p3: velocity divisor, if 1 or 0 travels entire distance in 1 frame) |
| **19** | Prorate (used with effect 5) | p1: proration value, p2: type (0 absolute, 1 multiplicative, 2 subtractive) |
| **22** | Something with scale and rotation | p6: 0=something with x scale, 1=something with y scale, 2=something with x and y scale, 10=something with rotation |
| **24** | Guard Quality Change (pause guard healing; 10000 = 1.000; unlisted values are 0) | p1: See guard quality values below |
| **100** | Increase Projectile Variable | p1: 1s place = Amount to increase by, 10s place = Variable ID |
| **101** | Decrease Projectile Variable | p1: 1s place = Amount to decrease by, 10s place = Variable ID |
| **102** | Increase Dash variable | p1: Amount to increase by |
| **103** | Decrease dash variable | p1: Amount to decrease by |
| **105** | Change variable | p1: Variable ID (0-9, can overflow into other data with <0 and >9), p2: Value, p3: 0 set, 1 add |
| **106** | Make projectile no longer despawn on hit (deactivates EFTP1 P3 bit0) | |
| **107** | Change frames into pattern | p1: Value, p2: 0 set, 1 add |
| **110** | Count normal as used in string | p1: Pattern of normal - 1 (0-8) |
| **111** | Store/Load Movement | p1: 0 save, 1 load; p2: clear current movement if not 0; p3: set Stored Y Acceleration to value if not 0 and if current Y Velocity and Acceleration are < 1 |
| **112** | Change proration | p1: proration, p2: proration type (0=override, 1=multiply, 2=subtract) |
| **113** | Rebeat Penalty (22.5%) | |
| **114** | Circuit Break | |
| **150** | Command partner | p1: Partner pattern, p2: 0=always, 1=if assist is grounded, 2=if assist is airborne, 3=if assist is standing, 4=if assist is crouching |
| **252** | Set tag flag | p1: value |
| **253** | Hide chars and delay hit effects | |
| **254** | Intro check | |
| **255** | Char + 0x1b1 \| 0x10 (idk what this does) | |

**Blending modes (for after-images and trailing images):**
- `0`: Blue (Normal - 30 30 255 150)
- `1`: Red (Normal - 255 30 30 150)
- `2`: Green (Normal - 30 255 30 150)
- `3`: Yellow (Normal - 255 255 30 150)
- `4`: Normal (Normal - 255 255 255 150)
- `5`: Black (Normal - 0 0 0 150)
- `6`: Blue (Additive - 30 30 255 150)
- `7`: Red (Additive - 255 30 30 150)
- `8`: Green (Additive - 30 255 30 150)
- `9`: Yellow (Additive - 255 255 30 150)
- `10`: Normal (Additive - 255 255 255 150)
- `11`: Normal (Normal - 255 255 255 255)

**Type 12 (Various teleports) details:**
- **param12: 0** - Move self or parent relative to itself or relative to camera
  - param1: x offset
  - param2: y offset
  - param3: 0 position relative to param4, 1 position relative to camera's edge (disregards zoom) and floor
  - param4: 0 target parent, 1 target self
  - param5: 1 Move parent relative to opponent
    - param1: x offset
    - param2: y offset
  - param5: 2 Move parent to self
- **param12: 1** - Reset camera and move to absolute position
  - param1: x position
  - param2: y position
  - param3: 0 x is absolute, 1 x is relative to facing
- **param12: 2** - Move relative to parent or opponent if has been grabbed
  - param1: x offset
  - param2: y offset
- **param12: 3** - Keep in bounds

**Guard Quality values (Type 24):**
- `0`: -50000 (Guard Reset)
- `1`: 0 (Super jump)
- `5`: 10000 (Stand shield)
- `6`: 10000 (Crouch shield)
- `7`: 10000 (Air shield)
- `10`: 4000 (Ground dodge)
- `11`: 7500 (Air dodge)
- `15`: 0 (Backdash)
- `20`: -50000 (Heat/Blood Heat Activation)
- `21`: -500 (Meter Charge, unused)
- `22`: -50000 (Ground burst)
- `23`: -50000 (Air burst)
- `25`: -5000 (Throw escape, unused)
- `30`: -15000 (Successful stand shield)
- `31`: -15000 (Successful crouch shield)
- `32`: -5000 (Successful air shield)
- `35`: -50000 (Throw escape/Guard Crush)
- `42`: -500 (Meter charge 2F)
- `43`: -333 (Meter charge 3F)
- `44`: -250 (Meter charge 4F)

### EF Type 8 - Spawn Actor from effect.ha6

Same parameters as Effect 1. Spawns patterns/actors from effect.ha6 instead of character file.

| Parameter | Description |
|-----------|-------------|
| **no** | Pattern to spawn from effect.ha6 |
| **param1-12** | Same as Effect Type 1 |

Known actors: `5` = clash, `26` = charge [X]

### EF Type 9 - Play Audio

| Parameter | Description |
|-----------|-------------|
| **no** | Bank (0 = universal sounds, 1 = char specific) |
| **param1** | Sound id |
| **param2** | Probability (0 = 100%) |
| **param3** | Number of different sounds to play. 1 and 0 are equivalent. Ids adjacent |
| **param4** | Unknown, sometimes 1 |
| **param6** | Unknown. Used with multiple instances of effect 9 |
| **param7** | Unknown, either 2 or 0. Probably related to parameter 6 |
| **param12** | Unknown. Sometimes 1. Used in time up, intro and win animations |

### EF Type 11 - Spawn Random Pattern at Random Location

| Parameter | Description |
|-----------|-------------|
| **no** | Minimum pattern |
| **param1** | X |
| **param2** | Y |
| **param3** | W (Width) |
| **param4** | H (Height) |
| **param5** | Range of patterns |
| **param6** | Amount of pattern spawns (randomly selects from within range N times) |
| **param7** | Flagset 1 (Same as effect 1) |
| **param8** | Flagset 2 (Same as effect 1) |

### EF Type 14 - Set Opponent State on Grab (Reset Bounces)

| Parameter | Description |
|-----------|-------------|
| **no** | Pattern of opponent |
| **param1** | X (Ignored if it's not the first effect 14 in the frame) |
| **param2** | Y (Ignored if it's not the first effect 14 in the frame) |
| **param3** | Rotation. Not sure about the unit but large values like 1000 and 4000 are used |
| **param4** | Flags (bitwise) |
| **param5** | Vector id / Which part to grab (0=Origin, 1-3=Special box 10-12). Works only if animation plays |
| **param6** | Untech time. Used with vectors |
| **param7** | Character location to use: 0 = self, 1 = opponent |

**Param4 flags:**
- `bit 0 [1]` = Play animation / If not 0 it plays the animation
- `bit 1 [2]` = Reverse vector
- `bit 2 [4]` = Can't otg
- `bit 3 [8]` = Make enemy unhittable
- `bit 4 [16]` = Use last position?
- `bit 5 [32]` = Hard knockdown

### EF Type 101 - Spawn Relative Pattern

Same as Effect 1 but `no:` is an offset from current pattern (not absolute pattern ID).

### EF Type 111 - Spawn Random Relative Pattern

Same as Effect 11 but `no:` is an offset from current pattern (not absolute pattern ID). Used in fvaki pits.

| Parameter | Description |
|-----------|-------------|
| **no** | Almost always 1 |
| **param1** | X offset |
| **param2** | Y offset |
| **param3** | A flag? |
| **up to 12** | Various parameters |

### EF Type 257

Only used once in arc's pattern 124. No effect? Probably a typo.

### EF Type 1000 - Spawns and Follow

Dust of osiris uses it for the pool effect and sion. Most probably interacts with pat data.

### EF Type 10002

Related to effect 2... or just a stupid way to comment out an effect.

---

## Condition Parameters (IF)

Conditions control branching, loops, and state checks within animation patterns.

### IF Type 1 - Jump on Directional Input

| Parameter | Description |
|-----------|-------------|
| **param1** | Lever direction. Numpad notation, with 0 as neutral. Value 10 is both downforward and forward |
| **param2** | Pattern to jump to. Add 10000 for frame jump |
| **param3** | If 1, logically negate the condition |

### IF Type 2 - Effect Despawn Conditions

| Parameter | Description |
|-----------|-------------|
| **param1** | 1=Despawn when touching stage walls, 2=Despawn when touching stage borders, 3=Despawn when outside the screen |
| **param2** | 0-1 Unknown |
| **param3** | 1 = Despawn at animation end |
| **param4** | Values: 1, 11, 21, 31, 41, 51, 61, 62, 71, 79, 81, 91. Appears only if param3 is 1. Unknown. Related to param9 of effect1? |

### IF Type 3 - Branch on Hit

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to. Add 10000 for pattern jump |
| **param2** | 0=only on hit, 1=on block too, 2=?, 3=?, 5=Only on block, 6=?, 7=?, 100=only on shield |
| **param3** | 0=any, 1=only on grounded opponent, 2=only on airborne opponent |

### IF Type 4 - Vector Check

### IF Type 5 - KO Flag Check

### IF Type 6 - Lever & Trigger Check 1

| Parameter | Description |
|-----------|-------------|
| **param1** | ?? often 255 |
| **param2** | Button(s) that needs to be pressed. It's a flag. Values can be OR'd (1=a, 2=b, 4=c, 8=d) |
| **param3** | Frame to jump to |
| **param4** | Flags (1=Negate the condition?, 2=Must be held) |

### IF Type 7 - Lever & Trigger Check 2

### IF Type 8 - Random Check (runs every subframe)

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to |
| **param2** | Chance (max 512) |

### IF Type 9 - Loop Counter Settings (Unused?)

### IF Type 10 - Loop Counter Check (Unused?)

### IF Type 11 - Additional Command Input Check

### IF Type 12 - Opponent Distance Check

### IF Type 13 - Screen Corner Check

### IF Type 14 - Box Collision Check

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to |
| **param2** | No idea |
| **param3** | Probably blue box index? |

### IF Type 15 - Box Collision Check (Capture)

### IF Type 16 - Be Affected by Scrolling

### IF Type 17 - Branch According to Number of Hits Within Animation

### IF Type 18 - Check Main/Trunk Animation and Jump

### IF Type 19 - Projectile Box Col Check (Reflection)

### IF Type 20 - Box Collision Check 2

### IF Type 21 - Opponent's Character Number Check

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to |
| **param2** | Character id (probably the list in charaselect.txt). Examples: 0=Sion, 8=Miyako |

### IF Type 22 - BG Number Check (Unused?)

### IF Type 23 - BG Type Check (Unused?)

### IF Type 24 - Projectile Flag Check

### IF Type 25 - Variable Comparison

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to |
| **param2** | Variable id |
| **param3** | Value to compare |
| **param4** | Type of check (0=Greater than, 1=Less than, 2=Equal to) |

### IF Type 26 - Check Lever and Change Vector

### IF Type 27 - Branch When Parent Gets Hurt

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame/Pattern to jump to |
| **param2** | 0 Pattern, 1 Frame |
| **param3** | Flags. Only when getting thrown if 0 (bit 0=When getting thrown, bit 1=When getting hurt, bit 2=When blocking) |

### IF Type 28 - Jump if Knocked Out

### IF Type 29 - Check X Pos in Screen and Jump (Unused?)

### IF Type 30 - Facing Direction Check

### IF Type 31 - Change Variable on Command Input

### IF Type 32 - Jump if on the CPU Side of a CPU Battle

Possibly an arcade mode thing. Only used by Hime's intro animation.

| Parameter | Description |
|-----------|-------------|
| **param1** | ? |

### IF Type 33 - If Sound Effect is Playing

### IF Type 34 - Homing

### IF Type 35 - Custom Cancel Command Check (Multiple)

Can input moves with bit5 on.

| Parameter | Description |
|-----------|-------------|
| **param1** | 0=always, 1=on hit/block, 2=only on hit, 3=only on block (unused) |
| **param2-5** | Can input move id (first parameter) defined in c.txt |
| **param8** | ??? |

### IF Type 36 - Meter Bar Mode Check

### IF Type 37 - Jump According to Color Selected

A leftover from Re-Act? The color selected doesn't affect anything.

| Parameter | Description |
|-----------|-------------|
| **param1** | Pattern to jump to |

### IF Type 50 - Effect Reflection Box (Unused?)

### IF Type 52 - Throw Check

| Parameter | Description |
|-----------|-------------|
| **param1** | Frame to jump to if the throw is successful |
| **param2** | If 1, it's an air throw |
| **param3** | Frame to jump to if the airthrow is part of a combo |

### IF Types 38-42, 51, 53-55, 60, 70, 100

Unknown functionality. Documented as "?" in source.

---

## Structure Information

From source code analysis (sosfiro_hantei fork):

### Layer Structure
```cpp
struct Layer {
    int spriteId = -1;
    bool usePat;              // Use PAT file (effect)
    int offset_y;
    int offset_x;
    int blend_mode;
    float rgba[4]{1,1,1,1};
    float rotation[3]{};      // XYZ rotation
    float scale[2]{1,1};      // XY scale
    int priority;
};
```

### Frame_AF Structure (Animation Frame)
```cpp
struct Frame_AF {
    std::vector<Layer> layers;
    int duration;
    int aniType;              // 0: End, 1: Next, 2: Jump to frame, 3: Go to start of seq??
    unsigned int aniFlag;
    int jump;
    int landJump;             // Jumps to this frame if landing
    int interpolationType;    // 1-5: Linear, Fast end, Slow end, Fast middle, Slow middle
    int priority;             // Default is 0. Used in throws and dodge
    int loopCount;            // Times to loop
    int loopEnd;              // The frame number is not part of the loop
    bool AFRT;                // Makes rotation respect EF scale
    bool afjh;                // New, from UNI
    uint8_t param[4];         // New, from UNI (AFPA)
    int frameId;              // New, from UNI (AFID)
};
```

### Frame_AS Structure (Animation State)
```cpp
struct Frame_AS {
    unsigned int movementFlags;
    int speed[2];             // XY speed
    int accel[2];             // XY acceleration
    int maxSpeedX;
    bool canMove;
    int stanceState;
    int cancelNormal;
    int cancelSpecial;
    int counterType;
    int hitsNumber;
    int invincibility;
    unsigned int statusFlags[2];

    // Sinewave parameters
    unsigned int sineFlags;   // Similar to ASV0
    int sineParameters[4];    // Distance XY, Frames per cycle XY
    float sinePhases[2];      // Phases XY. Use (0.75, 0) for CCW circles

    int ascf;                 // UNI: related to counterhits or cancel?
};
```

### Frame_AT Structure (Attack)
```cpp
struct Frame_AT {
    unsigned int guard_flags;
    unsigned int otherFlags;
    int hitStun;
    int correction_type;      // 0: default (set only if lower), 1: multiplicative, 2: subtractive
    int damage;
    int meter_gain;
    int guardVector[3];       // Stand, Air, Crouch
    int hitVector[3];         // Stand, Air, Crouch
    int gVFlags[3];
    int hVFlags[3];
    int hitEffect;
    int soundEffect;
    int addedEffect;          // Lasting visual effect after being hit
    unsigned int hitgrab;     // Bitfield in UNI
    float extraGravity;       // Affects untech time and launch vector, can be negative
    int breakTime;
    int untechTime;
    int hitStopTime;          // Default value zero. Overrides common values
    int hitStop;              // From common value list
    int blockStopTime;        // Needs flag 16 (0 indexed) to be set

    // UNI-specific
    int addHitStun;
    int starterCorrection;
    int hitStunDecay[3];
    int correction2;          // During combo?
    int minDamage;
};
```

### Frame_EF Structure (Effect)
```cpp
struct Frame_EF {
    int type;
    int number;
    int parameters[12];
};
```

### Frame_IF Structure (Condition)
```cpp
struct Frame_IF {
    int type;
    int parameters[9];       // Max used value is 9
};
```

---

## TODO: Additional Documentation Needed

1. **AFFE bitwise flags** - Complete list of AFFE bitwise values and their effects on AFJP, AFJC, and AFF2
2. **AFPR preset values** - List of image priority preset values
3. **AFAL mode preset values** - Transparency filter mode preset values
4. **ATF1 complete flags** - Full documentation of all 32 bits in ATF1
5. **ATF2 flags** - If functionality exists, document the bitwise flags
6. **IF Types 38-42, 51, 53-55, 60, 70, 100** - Unknown functionality
7. **Sinewave parameters** - Complete documentation of sinewave movement system
8. **Interpolation types** - Full details on interpolation types 1-5

---

*Document extracted from H6 param.pdf and sosfiro_hantei fork*
*Last updated: 2025-10-06*
