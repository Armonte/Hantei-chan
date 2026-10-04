# Yamayuri Rendan / Lilian Fourhand (French-Bread, Dec 2005): archives, text grammars, BG chips/maps, replays, runtime defs

STATUS: WORK IN PROGRESS (parked). Everything marked **T** was traced in `LilianFourhand.exe` (MSVC, 32-bit, base 0x400000, IDB `C:\dev\ida\server\LilianFourhand.exe.i64`)
and checked against the shipped files by `tools/fb/lilian_verify.py` (0 unexplained). **I** = inferred from code shape/data, **U** = unproven / not traced.
Types: `docs/formats/ida/lilian_types.h` (25 structs, 9 enums; generator-checked offsets; applied in the IDB). Files: `files\data\00dt.p 00bg.p 00bgt.p 00dm.p 00b.p 00e.p`.

## 1. Archives (.p) - `LilPak_Open` 0x437BA0, `LilPak_ReadEntry` 0x437E80

Identical to the Melty Blood / Re-ACT `.p` (docs/formats/mbr.md section 2), key 0xE3DF59AC, 68-byte entries:
`u32 cipherMode` (0 name-keyed, 1 plain, 2 XOR 0xCA), `u32 count ^ KEY`, `count x { char name[60]; u32 offset; u32 size ^ KEY }`
(name bytes 0..58 `^= (3*j*i + 61) & 0xFF`), payloads contiguous. Name-keyed payload: first `min(size, 8563)` bytes `^= (k + upper(name)[k % len] + 3) & 0xFF` (CP932 aware upper-case).
Measured: 00dt, 00bgt, 00dm, 00e = mode 0 (v0); 00bg, 00b = mode 1 (plain, `fbarctool` calls it v1). Lookup is by basename, `_strcmpi` (`LilPak_FindEntryIndex`).
Slot tables: data slots `g_PakArchives` 0x59B378 (10 x 284 bytes; 00dt.p 00bg.p 00bgt.p 00dm.p opened by `LilPak_OpenAllData` 0x439130, searched in order by
`LilPak_LoadEntryAlloc` 0x439210; a loose file in the game dir is the fallback, `LilPak_OpenEntryHandle` 0x4391B0), audio slots `%02db.p` / `%02de.p` (00b.p = BGM MP3, 00e.p = `se\se%04d.wav`;
`LilAudio_OpenPaks` 0x43BEC0, `LilAudio_FindMp3Entry` 0x43C060). Game paths like `.\bg\bg01.txt` or `.\data\chara\_00_0_xxx.txt` resolve to the entry `BG01.TXT` / `_00_0_xxx.TXT`.
Archive contents (verified): 00dt 533 (390 EX3 + 143 TXT), 00bg 70 (35 BGC + 35 MAP), 00bgt 134 TXT, 00dm 3 REP, 00b 15 (14 MP3 + `00`), 00e 75 WAV.

## 2. Text reader (every .TXT) - `LilIniFile` 0x134 bytes, `LilIni_*` 0x438460..0x438BD0, line cursor `LilText_*` 0x441540..0x441670

The game never tokenises. A whole file is held in memory (CP932, CRLF) and looked up by substring/line scanning:
* Section index (`LilIni_IndexSections`): every line whose first char (after skipping CR LF TAB SPACE and `//` comment lines) is `[`; name = text up to the first `]`, 1..31 chars. Text after `]` is ignored (shipped: `[Enemy_160]/////////`).
  First section with a given name wins; fallback `strstr("[name]")` anywhere.
* Key lookup (`LilIni_FindKey`): starting at the section line, advance line by line; skip blank lines, lines of spaces/tabs and **only whole-line `//` comments**; stop at the next `[` line.
  A line matches when its first char equals the key's first char and the token up to the first of `SPACE TAB =` equals the key (exact, case sensitive). Value = text after the first `=` with leading SPACE/TAB skipped,
  running to end of line (CR). Trailing `// comments` are NOT stripped: ints use `atoi`, floats `atof`, strings run to CR (so a trailing comment would become part of a path - never present in shipped data).
  Missing key/section -> default. Duplicate key: first wins. Unknown keys are ignored (shipped typo `aram_01` in BG03ENEMY*.TXT is dead).
* Writers must therefore preserve: CRLF, key spacing (`Key = value` is the dominant style: 25999 of 26043 key lines have spaces around `=`), indentation, trailing `//` comments. Verifier models a file as lossless
  `(indent, key, ws, '=', ws, value-with-comment)` lines; round trip is exact for all 147 non-AniEdit text files (one has bare LF: DEFAULTENEMY.TXT line ending, 4 BG06PROC*.TXT have no final CRLF).

## 3. AniEdit animation .TXT (130 files in 00dt, `[Header]`/`Name=AniEdit Data`) - `LilAniTable_LoadFromIni` 0x4010E0, `LilAniFrame_ReadFromIni` 0x401830

Layout the shipped files use (all 130 round-trip byte-exact through a numeric model and writer in the verifier). CRLF, no spaces around `=`, file ends with CRLF:
```
[Header]  Name=AniEdit Data  TextureNum=N
[Texture00] Name=xxx.bmp  Size=512          (NN 00..; game loop stops at the first missing Size, max 200)
[Pattern000] FrameNum=k  Name=<CP932 label>  (NNN ascending, only present patterns; game reads 0..254 and treats FrameNum 0/missing as absent)
[Pattern000-000]  ShiftX= ShiftY= U= V= W= H= tpage= AttrFlag= AlphaFlag= AlphaDepth= Angle= ZoomX= ZoomY= AniFlag= Jump= Delay= EtcFlag0=
                  then only the NON-ZERO Hantei00..Hantei15 (ascending) then non-zero Effect00..Effect09 (ascending)
[HanteiRect]  Hantei_0000=x1,y1,x2,y2 ...   (contiguous from 0000; game stops at the first missing key)
[EffectParam] Param_0000=16 ints, each followed by one space ("0 0 ... 0 " with trailing space)   (contiguous from 0000)
```
Number formats (byte-exact in all 6733 frames): ints `%d`; `Angle=%f` (`0.000000`); `ZoomX/ZoomY=%7.3f` (`  0.000`, `  0.600`). `Delay`/`Jump`/`tpage` etc `%d`.
All 17 frame keys are always present and in the order above. A zero Hantei/Effect value is never written (absent = 0 = no box / no effect), index 0 of `[HanteiRect]` is a real row but slot value 0 means "none".
Frame record `LilAniFrame` 92 bytes (types header). Game-read keys: all of them (T). **Editor-only: none found**; the `Hantei05` name of the task is simply slot 5.
* `AniFlag` (T, `LilEntity_StepAnimation` 0x43D600; after `Delay` ticks): 0 = next frame (wraps to 0), 1 = stop on this frame and set finished flag, 2 = jump to frame `Jump` (wraps to 0 when out of range), 3 = set finished flag to -1 (hold).
* `AttrFlag` bit0 = flip H, bit1 = flip V (T, `LilAniAsset_LoadFrameUV` 0x43EA20; values seen 0,1,2,3). `AlphaFlag` 0/1/2 and `AlphaDepth` 0..255: draw blend (I). `EtcFlag0` 0/1 (U meaning).
* `tpage` indexes `[TextureNN]`; in 5 files (`_00_1`, `_01_1`, `_02_1`, `_03_0`, `_03_1`) 7 frames each have tpage == TextureNum (one past the array; U what the game does).
* Texture `Name=foo.bmp` is stored as archive entry `FOO.EX3` plus optional alpha mask `FOO_M.EX3`. Every texture of every AniEdit file has an EX3; 35 EX3 are UI images loaded by path from the exe
  (`c_sel_*`, `sys_*`, `ed%02d`, `loading`, `logo00`, `dbgfnt`) and 6 are unreferenced leftovers (C03_04M, C04_04M, EN01_02, EN01_03, EN03_00, ENEMY_BARA00_02). EX3 = `LLIF` + 60-byte name (`foo.bmp`) + LLIF blocks (round-trip supported elsewhere in the worktree; the verifier only checks the header).
* Hantei slots (rect indices into `[HanteiRect]`, world rect = entity pos + offset + rect, `LilEntity_GetWorldRect` 0x43E020 / `LilEntity_CacheWorldRects` 0x43DEC0): **0..4 = attack/contact boxes** (damage the other side; the player's stomp test uses the enemy's 0..4, `LilPlayer_StompTest` 0x40B1B0),
  **5 = vulnerable (hurt) box of enemies**, hit by the player's slot 0..4 boxes (`LilPlayer_ShotHitsEnemyTest` 0x40C940) (I), **14 = player body/foot box** (also used as ground reference, T for the touch test `sub 0x40DE00`), **15 = anchor point**: x*facing + pos is the muzzle/aim point (T, 0x4094A0). Slots 6, 10, 12, 13 occur in data (90 / 13 / 1 / 1 frames) and 10 is read (0x41814F): meaning U. Point boxes are written as `x,y,x,y`.
* Effect slots (0..9, index into `[EffectParam]`, run by `LilEntity_RunFrameEffects` 0x43E860 when the frame is entered): row `v[0]` = effect id looked up in `g_FrameEffectTable` 0x475BF8: 1, 5, 10, 15, 100 (the 10/15/100/1 handlers are virtual calls on the entity, +20/+8/+12/+16; id 5: `v[1]==10` plays wave `se%04d` number `v[2]`, `v[1]==0` calls the screen-shake/hit-stop setter with `v[2], v[3]`). Remaining args per effect: U.

## 4. [DATA]-style game text files

All read through section 2. Keys the game reads (T) and the shipped values; the verifier asserts that no other key occurs in any file.
* **Character** `C00..C03.TXT`, `C00SUB..C03SUB.TXT` (8 files, `LilPlayer_LoadCharaFile` 0x410190): `[DATA] Path=.\data\chara\  Ani=<file>` (Ani read, the file is a normal AniEdit entry), `[SUBDATA] Data=` (not read), `[STATUS] Gravity Jump Run Dash Fumituke (floats) JumpNum DashNum DashTime (ints) JumpShotStartAngle JumpShotAngleAdd FumitukePower (floats)`,
  `[SHOT_00..04] BulletNum(int) Rapid PowRapid(float) Time BulletCnt(int) StartAngle IncAngle(float)`, main characters only `[LOCKSHOT] Type ChargeTime DelayWait AttackDelayWait AttackEndWait Max LockOnFront`.
  The game ignores the file names inside: the 8 files are chosen from the fixed string table `.\data\c00.txt, c00sub, c01, c01sub, c02, c02sub, c03, c03sub` by index (`LilPlayer_LoadSelectedChars` 0x410340).
* **Stage** `BGnn.TXT` / `BGnn_k.TXT` (k = course/difficulty variant 0..2 for BG01..07, else plain; `LilBg_LoadStageDataFile` 0x403250): `[DATA] Name=` (editor label) `Path=<dir>` `Layer00..Layer04 = <stem>` (`Layer10..Layer14` = alternate set 1, only 5 files) `Hantei = <stem>` (collision layer);
  `[VALUE] BosstimeBest BosstimeDelay ShotDownWorst ShotDownDelay` (read by `LilStageValues_Load` 0x43FF80 from `bg%02d_%d.txt`, fallback `bg%02d.txt`). Layer file = `<Path><stem>.BGC` and `.MAP` (the 00bg.p entries `BG01_00.BGC` ...).
  Stage files present: BG01..BG07 (x3 variants each, BG02PROC_ stray), BG10..BG24, TITLEBG; BG16 and BG22 share a name label.
* **SystemData.TXT**: `[DATA] ItemRate=` (T, 0x43FEC0). **SystemEffect.TXT**: `[Effect_NNN] Type AniTablePath AniTableName Time StartPat IsScroll` (T, `LilEffectDefs_Load` 0x441710; 84-byte `LilEffectDef` x 1000).
* **CharaBullet.TXT** (`[Bullet_NNN]` 0..99: `Speed Time AniTablePath AniTableName StartPattern HitPattern GroundHit HitVectorRoll Power PowPower Detonation`, `LilBulletDefs_Load` 0x404D70, 108-byte records). `_CHARABULLET.TXT`/`_CBULLET*.TXT`... in 00dt are AniEdit files; `CHARABULLET.TXT` and `_CHARABULLET.TXT` start with a `////` banner and are the bullet-def text (the game loads `.\data\CharaBullet.txt`).
* **ENDSTAFF.TXT** (`LilEndStaff_Init` 0x4435E0): raw line cursor (`//` comment lines skipped, `sscanf %d` ignores trailing comments): line 1 = name-hold wait (280), line 2 = fade/next wait (120), then `role,nameGraphic` pairs (8-byte records) until `-1,-1`. 11 pairs shipped. This is the only credits/"story" text.
  **There are no dialogue/story script files**: ending screens are images (`ed%02d.bmp`) and the `_エンド_*.TXT` AniEdit files; `BG10` (ENDLE...) and `BG07` (ENDING) are BG data files, not text scripts. (U: no other text resource was searched for in the .rsrc.)

## 5. Enemy definition files (`BGnnENEMY[_k].TXT`, `DEFAULTENEMY.TXT`, `STANDARDENEMY.TXT`) - `LilEnemyDefs_Load` 0x436400

Load order per stage: stage file, DEFAULTENEMY, STANDARDENEMY. For each id 0..999, `LilEnemyDef_LoadOne` 0x4362D0 uses the first source that has `[Enemy_NNN]`; `_SameParam_ = M` copies `[Enemy_MMM]` first;
every key read goes through `LilEnemyDef_ReadKeys` 0x435FF0 which reads the stage file first and falls back to DEFAULTENEMY when the stage value equals the sentinel. Keys (T): `Type` (default NNN), `Position` (0 front, 1 back), `IsScroll`, `GroundHit`, `PlayerBulletHit`, `MaxHp` (float), `Attack` (float), `Score`, `IsBullet`, `StartPat`, `DestroyPat` (default -1), `CenterPos`, `RadarType` (-1), `NoShotDownPlus`, `Shadow`, `ShadowX/Y` (float->int), `ShadowW`,
`ItemType` (-1), `Param_00..Param_09`, `AniTablePath`, `AniTableName` (asset `<path><name>` is loaded through `LilAniAsset_Get`, a missing file pops a message box). The first lines of BGnnENEMY.TXT document the keys in Japanese. 192-byte `LilEnemyDef`, `g_EnemyDefs` 0x56B308 (id 1000 = generic effect object).

## 6. Stage proc scripts (`BGnnPROC[_k].TXT`, `TITLEBGPROC.TXT`) - the interpreter

`LilBgScript_RunFrame` 0x404370 runs once per frame over the raw text (`g_BgScriptCursor` 0x480524): find the command (`LilBgScript_FindCommandId` 0x404330: token before `SPACE TAB =` compared exactly against the 39-row table `g_BgScriptCommands` 0x471210 of `{char name[32]; u32 id; handler}`),
call the handler with the text after `=` (leading blanks skipped, value runs to CR), continue to the next line while the handler returns 1; `Wait` returns `g_BgFrameCounter >= N` so the cursor sleeps until the counter reaches N; blank and `//` lines are skipped.
Grammar: `Command = args` per line, args comma separated, parsed with sscanf (`" %d,%d,%d"`), trailing `//` comments harmless. Layers are addressed by number n: layer = n % 10 (0..4), set = n / 10 (0/1); `-1` = collision layer.

| command | args | effect (T) |
|---|---|---|
| Wait | N | sleep until frame counter >= N |
| SetScrollX / SetScrollY | f | layer velocity = f * ratio (all layers) |
| SetScrollAddX / AddY | f | acceleration = f * ratio |
| Flip | set[,speed] | activate layer set 0/1 (-1 toggles) with cross-fade, default speed 0.01 |
| SetScrollRatio | layer,fx,fy | per-layer parallax ratio |
| SetScrollLoop | layer,enable,width | wrap (alias row also present); SetLoopEndCount layer: sets frame counter at loop end |
| SetMoveMode | n | camera/player move mode (2 = follow) |
| SetScrollFree | layer,flag | free-scroll mode of the collision layer |
| SetBgPos | layer,x,y | layer position |
| SetBgPrio | layer,p | draw priority 0->0, 1->230, 2->400 |
| SetBgColorDepth / InitializeBgColorDepth | n | background tint level (immediate / reset) |
| StopBgCount | - | freeze the frame counter |
| HanteiView / SpriteHanteiView / MutekiMode | 0/1 | debug overlays, invincibility |
| SetCharaPos x,y / SetCharaPosGround x / SetCharaControl / SetCharaClip / SetCharaKey / SetPauseAble | ints | player placement and input locks |
| SetNextItem | n | item dropped by the next enemy |
| SetEnemy | x,y,type[,depthBias] | spawn `[Enemy_type]` at (x,y); x/y 0xFFFF = random; depth = bias + 200 (front) / 250 (back) |
| SetEnemy_AreaBoss | x,y,type,bias | same, area-boss flag |
| SetEnemyPamam / ResetEnemyPamam (sic) | 9 ints / - | default parameter block for the next spawns |
| LoadBGM / PlayBGM / StopBGM / FadeBGM | `.\bgm\xx.mp3` / loop / - / frames | music (names resolve to 00b.p entries) |
| PlayEffect | n | wave `se%04d` |
| StageClear | f | end of stage |

Every key token in every proc file belongs to this table (checked by the verifier). Variants: `_0/_1/_2` = course/difficulty chosen by `g_CourseVariant`.

## 7. BG chips `.BGC` ("FCHIP SYS") and maps `.MAP` ("FMAP SYS") - `LilChipset_ReadBgc` 0x4026C0, `LilMap_ReadMap` 0x402AD0, 35 + 35 files

```
BGC  +0x00 char magic[9] "FCHIP SYS" (no NUL)   +0x09 u32 version = 0   +0x0D u32 mode (0 = 8-bit indexed, 1 = 24-bit RGB)
     +0x11 u32 flag (1 in all files)  +0x15 u32 zero  +0x19 u32 gridCols  +0x1D u32 gridRows  (all four are kept by the loader; gridCols*gridRows == chipCount in all 35 files)
 mode 1 (27 files, picture layers):  +0x21 u32 0, +0x25 u32 chipCount, +0x29 u32 w(32), +0x2D u32 h(32);  +0x31 colour plane  chipCount*w*h*3 bytes (chip-major, rows top-down, 3 bytes/pixel),
                                      then alpha plane of the same size (3 equal bytes per pixel = grey alpha; ENDING_BG00.BGC has 95 non-grey pixels).   file = 0x31 + 2*chipCount*w*h*3
 mode 0 (8 *_CHECK.BGC, collision):  +0x21 u32 0, +0x25 chipCount, +0x29 w, +0x2D h, +0x31 n1, +0x35 n2 (both 256);  +0x39 plane A chipCount*w*h bytes, plane B (identical), chipCount u32 palette-select (all 0),
                                      n1 u32 palette A (ARGB), n2 u32 palette B.
 chip size 32x32 everywhere.
MAP  +0x00 "FMAP SYS"  +0x08 u32 version = 0  +0x0C u32 width  +0x10 u32 height  +0x14 width*height u16 chip indices (row major)   (every index < chipCount of the same-stem BGC)
```
Maps are paired by stem (`BG01_00.BGC` + `BG01_00.MAP`); pictures are `BGnn_00..02` (layers), collision is `BGnn_CHECK`. Collision lookup (`LilBgLayer_GetAttrAtWorld` 0x4046C0): world x,y -> chip via map -> pixel of plane A = `LilChipAttr`:
0 air, 1 and 4 floor-type (`LilPlayer_MoveAgainstChipAttrs` 0x40AE00, I), 2 and 3 solid/wall (2 pushes up, 3 blocks sideways, I). Attribute value range 0..4 verified. Layer runtime struct `LilBgLayer` 152 bytes (position, ratio, velocity, accel, loop, priority).

## 8. REP replays (DEMO00..02.REP in 00dm.p; attract demos; `LilReplay_LoadFile` 0x40C120 / `LilReplay_SaveFile` 0x40C040)

`u32 rngSeed` (becomes `g_RngState` 0x59C8A8) | `u32 frameCount` (written on save, ignored on load) | `u32 charaMain` (`g_SelectedCharaMain`, index 0/2/4 into the c0N / c0Nsub string table) | `u32 charaSub` (1/3/5) | `u32 reserved = 0` | `u32 courseVariant` (`g_CourseVariant`, 1 in all) |
`u32 stageNo` (`g_StageNo`, 1..3) | `u32 inputSideMode` (`g_InputSideMode`, 0) | `u32 runCount` | `runCount x { u32 extraFrames; LilPadState pad (64 bytes) }`: the pad state is held for extraFrames+1 frames (runs are maximal; sum(extra+1) == frameCount, verified).
`LilPadState` (from `LilPad_Poll` 0x4399B0): dword0 direction in numpad notation (4,6,7,8,9 seen), dword1 direction edge, dword4 direction copy, bytes 32..63 = 32 button bytes (bit0 held, bit1 pressed this frame, bit2 released). `g_ReplayMode`: 0 off, 1 playback, 2 record (`LilPlayer_RecordOrPlayPad` 0x40C300). Names of the three header words after `charaSub` are I (roles inferred from callers, values consistent across the 3 demos). 

## 9. Entry `00` of 00b.p (6,163,022 bytes)

Not Melty Blood: it is a **GOF1-family character .DAT** (docs/formats/gof1.md): stage-2 cipher (keys `Memory...`, `Me...`, `hi...`), header signature `備前長船`, version 18, patternAreaEnd 46306, partsSize 6100332, cgSize 0, plus the 0x4000 pattern-name tail (names `立ち`, `ジャブ&アッパー`...: a punching fighter).
**Role: none in Lilian.** No code path requests an entry named `00` (audio lookups use `bgm\*.mp3` basenames; no string `00`/`%02d` is used for it); it is dead data carried by the archive builder. Decipher/encipher round trip verified. (U: a dynamically built name was not exhaustively excluded.)

## 10. Misc

BGM: 14 MP3 (BOSS1-3, CHARASEL, CLEAR, ENDING, GAMEOVER, OPENING, ST01-ST06) played from memory; SE: 75 WAV `se%04d.wav` (RIFF/WAVE verified). `.\_Rank.sav`, `.\_Opt.sav`, `.\App.ini` are written by the game (not in the archives).

## 11. Runtime notes (partial)

`LilEntity` (0x278 bytes, enemy/bullet/effect; the player is a bigger class): +4 isScroll, +0x160/+0x164 pos, +0x174/+0x178 offset, +0x18C draw priority, +0x1AC finished flag (byte), +0x1B4 pattern, +0x1B8 frame, +0x1BC timer, +0x1C0 LilAniAsset*, +0x1C8/+0x1CC list links, +0x1D8 child list, +0x1DC state-init flag, +0x1E0 state, +0x1E4 state timer, +0x1F8 HP, +0x204 position(plane), +0x214 alive, +0x218 areaBoss, +0x248 LilEnemyDef*, +0x260..+0x26C shadow params. Only the first fields are typed in the header.

## 12. Resume points (not done)

1. Hantei slots 6, 10, 12, 13 and the EffectParam argument meanings of effects 1/10/15/100 (read the virtual method implementations of the enemy/bullet classes).
2. Full `LilEntity` and player class layouts; type `g_Player*` objects (dword_59BEC0 / dword_59AA50) and apply to all game-state functions (dword_59AA48 state switch: 2 title?, 4 battle, 5 ?).
3. `LilPlayer` input record header semantics (names I), `LilPadState` unused dwords, the BG `Flip`/`SetMoveMode`/`SetScrollFree` exact behaviour.
4. EX3 and PAC repack are not re-encoded by the verifier (index rebuild only); EX3 re-encode lives in tools/fb/ex3_*.py.
5. Texture loader (`LilAniAsset_LoadFiles` 0x43EB10): confirm the `_m` mask blend and `tpage == textureCount` behaviour.
