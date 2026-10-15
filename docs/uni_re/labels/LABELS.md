# UNI2 / MBTL EF and IF labels

Tables shown by Hantei-chan for UNI2, MBTL (and, unverified, UNIB/UNIST) files. Data file:
`src/game_labels_fb.inc` (branch `feat/uni-labels`); lookups `src/game_labels.h`; UI `src/frame_disp/game_label_ui.h`.
MBAACC files keep the MBAACC tables and smart UI.

## Sources and verification

- **Primary source:** Mizuumi "Modding French Bread Games" pages by Pixloen, saved in `sources/mizuumi/`
  (index: `sources/INDEX.md`). They document only EF 1, 4, 6 (No. 1, 2, 5, 6, 8, 12, 111, 255), 8, 9, 14 and IF 56.
- **Handlers (IDA, renamed and saved in both IDBs):**

| | UNI2 `uni2_142_4ebed985.exe.i64` | MBTL `MBTL_25646393.exe.i64` |
|---|---|---|
| EF dispatch (`Han6_EF_Dispatch`) | 0x5B8620 | 0x582420 |
| EF6 sub-number dispatch (`Han6_EF6_Dispatch`) | 0x5B5610 | 0x57F480 |
| IF dispatch (`Han6_IF_Dispatch`) | 0x5C6220 | 0x590260 |
| per-frame IF loop (`Han6_IF_RunFrameConditions`) | 0x5C8580 | 0x5925C0 |
| per-frame EF loops | 0x5B8DB0, 0x5B94E0, 0x565990 | 0x537110, 0x5833F0 |
| IF pass table (`g_IF_TypePassTable`) | 0x9571A0 | 0x9D9680 |

  Decompilations: `ida/uni2/*.c`, `ida/mbtl/*.c`. The two games have **identical case sets** in all three
  switches, and a normalised diff shows only struct-offset changes (MBTL drops the EF6 1 p11 reset). One table
  serves both.
- **Status column:** `wiki+IDA` = the wiki row matches the handler (verified); `IDA` = no source covers it,
  read from the handler; `IDA-corrected` = the wiki or the MBAACC label is wrong for these games.
- **Counts:** 10 verified (wiki+IDA), 8 corrected, 46 IDA-only, 0 source-only.
  Rows: EF 10, EF6 34, IF 20.

## Corrections against the wiki and MBAACC

- **EF 4** (wiki "Set opponent visual state (on grab)"): no handler in either game (the wiki already says EF 14
  succeeds it). Shown as `EF 4 (unknown in UNI2)`.
- **EF 2, 3, 7, 30, 1000, 10002**: no handler (MBAACC has them). EF 3 appears in UNI2 data but is ignored.
- **EF6 5** (wiki "Round Recover 1"): turn-around modes (p1 0-7), as in MBAACC.
- **EF6 6** (wiki "locks horizontal movement"): set movement vector (random ranges, p12 mode).
- **EF6 8** (wiki "locks vertical movement"): push toward the target along an angle.
- **EF6 21**: restores the saved camera (MBAACC: camera zoom preset).
- **EF6 106**: set/clear object flags by mask (MBAACC: projectile keeps on hit).
- **EF6 115/116/117, 300-302, 310/311**: not in MBAACC. 310/311 set/clear ObjFlags with mask `p3 | p4<<16`
  (`Obj_SetObjFlags_Masked` uni2 0x5B7C60, effect objects only).
- **IF 5**: releases on KO (calls the EF5-0 routine) rather than jumping. **IF 29** clamps X to a screen
  position. **IF 70** pushes the object back inside the stage borders (p1-p4 enable each check, <0 = off).
- **IF types with no handler:** everything except 2, 3, 5, 6, 7, 8, 11, 14, 15, 17, 19, 20, 26, 29, 31, 34, 35, 38, 56, 70.

## EF types

| EF | Name | Status | Parameters (p1 = first) |
|---|---|---|---|
| 1 | Spawn pattern (character HA6) | wiki+IDA | p1 Relative X; p2 Relative Y; p3 Flags 1 (1 erase on parent hit, 4 follow parent, 8 parent hitstop, 16 camera centre, 32 erase on parent pattern change, 64 below floor, 128 on floor); p4 Flags 2 (2 follow parent, 4 erase when parent thrown, 64 ignore super flash, 128 ignore camera, 256 relative to opponent, 512 stage centre, 1024 stage centre facing right); p8 Angle (as SetAngle) |
| 5 | Held victim command | IDA | p1 No 0: victim state / No 1: damage / No 3: signed value; p3 No 1: hit effect; p4 No 1: sound id (normal_se); p5 No 1: apply combo damage calc |
| 6 | Various effects 2 (see number) | IDA | - |
| 8 | Spawn system effect (effect.HA6) | wiki+IDA | p1 Relative X; p2 Relative Y; p3 Flags 1 (as EF 1); p4 Flags 2 (as EF 1); p8 Angle (as SetAngle) |
| 9 | Play sound | wiki+IDA | p1 Sound id; p2 Probability (0 = always); p3 Number of adjacent ids to pick from |
| 11 | Spawn random pattern | IDA | - |
| 14 | Set opponent visual state (on grab) | wiki+IDA | p1 Relative X (once per pattern); p2 Relative Y (once per pattern); p3 Rotation; p4 Vector id; p5 Anchor (0 origin, 1-3 special box 10-12); p6 Untech time; p7 Hold position (1 player to opponent, 2 opponent to player) |
| 101 | Spawn pattern (relative, as EF 1) | IDA | p1 Relative X; p2 Relative Y; p3 Flags 1 (as EF 1); p4 Flags 2 (as EF 1); p8 Angle (as SetAngle) |
| 108 | Spawn system effect (as EF 8) | IDA | p1 Relative X; p2 Relative Y; p3 Flags 1 (as EF 1); p4 Flags 2 (as EF 1); p8 Angle (as SetAngle) |
| 111 | Spawn random pattern (as EF 11) | IDA | - |

## EF6 numbers (EF type 6, "Number" field)

| No. | Name | Status | Parameters (p1 = first) |
|---|---|---|---|
| 1 | Screen shake | wiki+IDA | p1 Duration; p4 Rumble id; p5 Shake param; p6 Shake param; p11 Bit 1: reset shake state |
| 2 | Invincibility timers (INVINCIBILITY ticker) | wiki+IDA | p1 Timer A (100+ shows the ticker); p2 Timer B |
| 4 | Add to gauge | IDA | p1 Amount (clamped to max); p2 Extra flag |
| 5 | Turn around (see p1) | IDA-corrected | p1 Mode: 0 flip, 1 face opponent (p3 dead zone), 2-3 flip by state, 4-5 set facing, 6 random, 7 copy; p2 Do not flip the held victim; p3 Dead zone (px) |
| 6 | Set movement vector | IDA-corrected | p1 Value min; p2 Value max (random); p3 Accel min; p4 Accel max (random); p5 0 = X, 1 = Y; p12 Mode: 0 set, 1 angle launch, 2 arc to opponent |
| 8 | Push toward target along angle | IDA-corrected | p2 Speed; p3 Re-run frame change |
| 9 | Set no-input flag | IDA | p1 0 = set, other = clear |
| 11 | Super flash | IDA | p1 Flash id; p2 Enable |
| 12 | Teleport / set position | wiki+IDA | p1 X; p2 Y; p3 Relative to camera; p4 1 = self, other = owner; p5 Flags (1 opponent, 2 to parent, 4 keep X, 8 keep Y, 16 force facing); p11 Mirror for 2P; p12 Mode: 0 relative, 1 camera, 2 parent, 3 stage edge |
| 14 | Add to gauge of box target | IDA | p1 Box; p2 Amount; p3 Extra flag |
| 15 | Start / stop music | IDA | p1 0 = stop |
| 16 | Directional movement | IDA | p1 Angle / X; p2 Random range / Y; p3 Frames / speed; p5 Random speed range; p12 Mode: 0 angle, 1 move to camera point |
| 19 | Proration (opponent side) | IDA | p1 Value; p2 Mode: 0 min, 1 multiply %, 2 subtract |
| 21 | Restore saved camera | IDA-corrected | - |
| 22 | Scale / rotation | IDA | p1 Value (1/10000); p2 Random range; p6 Mode: 0 scale X, 1 scale Y, 2 both, 10 rotation |
| 102 | Increase dash variable | IDA | p1 Amount |
| 103 | Decrease dash variable | IDA | p1 Amount |
| 105 | Change variable | IDA | p1 Variable; p2 Value; p3 Tens: 0 owner, 1 self; ones: 0 set, 1 add |
| 106 | Set / clear object flags | IDA-corrected | p1 Flag mask; p2 0 clear, 1 set, 2 clear if set |
| 111 | Store / restore movement (freeze) | wiki+IDA | p1 0 store, 1 restore; p2 Zero the movement after storing; p3 Y speed after the freeze |
| 112 | Proration (own side) | IDA | p1 Value; p2 Mode: 0 min, 1 multiply %, 2 subtract |
| 115 | Store movement and brake | IDA | p1 Divisor |
| 116 | Launch along rotation | IDA | p1 Speed (1/128 px) |
| 117 | Home in on own object's box | IDA | p1 Box (+9); p2 Frames; p5 Flags (1 keep X, 2 keep Y, 4 face it) |
| 153 | Draw in front (Z priority) | IDA | - |
| 252 | Set state byte | IDA | p1 Value |
| 254 | Round flag (intro done) | IDA | - |
| 255 | Round end: win animation done | wiki+IDA | - |
| 300 | Set / add draw offset | IDA | p1 X; p2 Y; p11 Bits: 1 X, 2 Y, 4 no mirror; p12 0 set, 1 add |
| 301 | Screen effect at position | IDA | p1 X; p2 Y; p3 Effect id; p4 Param; p5 Param; p6 Scale (1/10000); p7 Use default position |
| 302 | Set global flag | IDA | p1 Value |
| 310 | Set object flags | IDA | p3 Flags (low 16); p4 Flags (high 16) |
| 311 | Clear object flags | IDA | p3 Flags (low 16); p4 Flags (high 16) |
| 1100 | Reverse velocity | IDA | p1 0 = X, 1 = Y |

## IF types

| IF | Name | Status | Parameters (p1 = first) |
|---|---|---|---|
| 2 | Despawn conditions | IDA | p1 Bits: 1 off-screen X, 2 off-screen Y; p2 On landing; p4 Decrement variable (tens slot, ones amount) |
| 3 | Branch on hit / guard | IDA | p1 Target frame; p2 Contact type (%100); p3 0 any, 1 hit, 2 guard; p4 Decrement variable; p6 Combo count; p7 Hit count |
| 5 | On KO: release | IDA-corrected | - |
| 6 | Lever and button check (frame) | IDA | - |
| 7 | Lever and button check (pattern) | IDA | p8 Restart pattern |
| 8 | Random branch | IDA | p1 Target; p2 Chance (of 512); p3 Random target; p4 Count |
| 11 | Additional command check | IDA | p3 Value; p4 Command |
| 14 | Box collision check | IDA | - |
| 15 | Box collision check (capture) | IDA | p2 Box; p3 Object (%1000, /10000 side) |
| 17 | Branch on number of hits | IDA | p2 Hits at least; p3 Decrement variable |
| 19 | Projectile box contact | IDA | - |
| 20 | Box collision check 2 | IDA | p3 Object; p4 Hit stop |
| 26 | Lever direction check | IDA | - |
| 29 | Clamp X to screen position | IDA-corrected | p2 Screen X (-30000 = variable 0); p3 0 left limit, 1 right limit |
| 31 | Change variable on command | IDA | p2 Variable; p3 Value |
| 34 | Homing | IDA | - |
| 35 | Multi-command check | IDA | - |
| 38 | Change variable on hit | IDA | p1 Value; p2 Contact type (%100); p3 0 any, 1 hit, 2 guard; p4 Variable; p5 Ones: 0 set, 1 add; tens: target |
| 56 | Vacuum (pull opponents) | wiki+IDA | p1 Speed; p2 Range (px); p3 Weaken with distance |
| 70 | Stage border collision | IDA-corrected | p1 Border check 2 (<0 off); p2 Border check 3 (<0 off); p3 Border check 0 (<0 off); p4 Border check 1 (<0 off) |

## Not covered

- AT field labels: no source documents UNI/MBTL AT differences and the AT reader was not traced here; the AT
  panel keeps its current labels (UNI-only fields were already split out by D14).
- UNIB / UNIST: no executable checked; they use the UNI2 rows. UNIB data also uses EF 7 and IF 43, which show as unknown.
- Japanese: the sources give none; the label names stay English (UI strings around them are translated).
- EF 11/111 and IF 6/7/11/14/19/26/34/35 parameters are not broken down; they show raw unless non-zero.
