# GOF2 Obj part A: bytes 0x000..0x61F (decimal 0..1567)

Files: `gof2obj_partA_fields.py` (FIELDS, 74 top-level rows, contiguous 0..1567), `gof2obj_partA_types.h` (24 structs/unions, 22 enums, 8 callback typedefs; parsed into GOF2.exe.i64 with idc.parse_decls, sizes and member offsets machine-checked against the header comments and FIELDS; flag enums set to bitmask).
All new type names use the prefix `Gof2ObjA` / `G2OBJA_`. Embedded anime at +616 uses the existing `Gof2HanteiAnime` (68 bytes, +616..+683). Obj pointers are typed `void *` in nested structs (the integrator owns `struct Gof2Obj`).

## Method
Hex-Rays ctree scan of all 5000 functions for `*(var + k)` / `var[i]` accesses (pointer scaling handled), Obj-pointer variables identified by >=2-3 hits on Obj-only offsets (vtables, pool links, +688/+700/+1484/+4592...), then every field read in context (Obj_InitDefaults 0x431000, Obj_ConstructPoolSlot 0x435850, Obj_ResetOnDetach 0x4390D0, Obj_ExecStateCmdList 0x438660, Obj_SetActionAndRunScript 0x431B90, Obj_ScriptTick 0x434AD0, Obj_TickTimersRecursive 0x49E120, Obj_UpdateScreenBoundContacts 0x49D780, HitJudge_*). Raw `[reg+disp]` instruction scans over all functions were used for the unused claims. RBO RboActor docs (actor_part1..4) were used to recognise analogs (RampFx/ZoomFx/RgbFx/RotFx, ParentLink, ScreenBound, CameraFocus, BufferedJump, ColorFlash, overlay quad lists, event heads).

## Layout (decimal)
| Range | Field | Evidence |
|---|---|---|
| 0..99 | `transitionEvents` (count, 5 nodes, 6 category list heads, spare, single timed slot) | Obj_CollectTransitionRuleEvents gets obj+0; TransitionEventList_Link; list heads run by Obj_ScriptTick (+68), sub_4CDDB0 (+44/+60) |
| 100..295 | `frameEnterEvents` (count, 20 nodes, 4 heads at +264..295) | Obj_RunFrameScriptLists passes obj+100; FrameEnterEventList_Link |
| 296 | `motionHistRing` | ScriptOp_ObjMotionHistCmds, ObjMotionHist_InitFromOwner |
| 300..311 | overlay rect blend index + two overlay rect node lists | Obj_ExecOverlayRectOps (sub_438D60), DrawHelper_DrawOverlayRects, Obj_ReleaseSubNodes |
| 312..351 | `scriptWork[10]` | g_ScriptVm_ArgObj = obj+312; round-start copy of 10 dwords |
| 352..431 | `parentLink` (20 dwords) | setup op list Obj_ExecStateCmdList; Obj_RunParentLinkRulesOnChildren/Action |
| 432, 436 | `setupFlags`, `turnAroundMode` | many readers, sub_433D00 |
| 440..503 | screen bound state, camera focus, bound rect | Obj_UpdateScreenBoundContacts, Obj_ResolveWallContactLatches, Camera_ComputeTargetFromFighters |
| 504..540 | destroy flag, per-tick script, cooldowns, depth fields, afterimage trail | Obj_ScriptTick, Obj_ApplyFrameDrawPriority, Obj_ComputeTotalDepth |
| 544..591 | pool/owner list, sibling/parent/child, spawner, linked-target list, link context/node | ObjPool_*, Obj_ResetOnDetach, Obj_AttachSlot24CNode |
| 592..615 | `bufferedJump`, `superFreezeSerial`, `prevPatternNo` | ObjEvent_InputRuleBufferJump, SuperFreeze_Begin, Obj_SetActionAndRunScript |
| 616..683 | `anime` (Gof2HanteiAnime) | existing type |
| 684..711 | landed latch, facing, fixed facing/ptr, pending pattern/frame/facing | Obj_ApplyLandingFrameJump, sub_430490 |
| 712..1035 | three 104-byte movers (main, knockback, brake) + flag/counter dwords at 816/820/928 | Obj_ResetMoveTracks, Obj_ApplyLandingFrameJump, Obj_ApplyFrameBrake |
| 1036..1479 | colour flash, alpha/zoom/modulate/additive/flicker/rotX/Y/Z/zoomAdd effect blocks | Obj_TickEffectBlocks 0x430BE0, Obj_InitDefaults, DrawHelper_Draw |
| 1480..1527 | draw-helper follow mode, side, owner list index/sub slot, scriptTag, classFlags, blend/draw style, invulnerability windows | listed in FIELDS |
| 1528..1555 | `hookFlags[7]` (ScriptVm kind 3 ids 7..13) | Obj_ExecStateCmdList case 29, Obj_CallHookScript |
| 1556..1567 | first three dwords of the embedded CAppHanteiKougeki (vtable, primary/secondary counts) | Obj_ConstructPoolSlot; rest belongs to part B |

## Counts (leaf bytes, anime counted as one traced leaf)
traced 1392 B, inferred 0 B, unused 176 B = 1568 bytes (byte-exact, from `tools/ida/count_gof2obj_evidence.py`; run it for the current figures, `--list` names every unused leaf).

Unused proofs (no code reads them; all `[reg+disp]` operands in Obj-pointer functions scanned, other hits belong to non-Obj structs): `transitionEvents.clearedSpare` (+92, zeroed only), `flickerGate` (+1312..1327, zeroed only by Obj_InitDefaults), each rotation block's +12..+31 (RBO accum/angVel fields do not exist in GOF2; update callback is always the no-op), mover header bytes +0..+11 (only address-of uses of obj+712/824/932), `RgbFx.unused_04`.
Resolved 2026-10-03 (all former [inferred] fields): never-consumed or write-only fields are now `unused_<off>`: `transitionEvents.unused_4C` (was forcedJumps: linked, never run), `transitionEvents.unused_60` (was singleSlotTimedRule: category 6 absent from the table), `frameEnterEvents.unused_AC` / `unused_B4` (were onHitAttackerMods / chanceActions), `bufferedJump.unused_04/08/0C` (write-only), RotFx `unused_08` (was baseAngle, write-only). Proven and renamed: `transitionEvents.endRules` -> `facingKindRules`, `clashCooldownSpecial/Normal` (were effectCooldownA/B); `MoverScript payload.skipSetup` is traced (reader MoverScript_Step).

## Notes / boundaries
* Part B/C boundaries: part B owns 0x620.. (record pointer +0x620, rest of CAppHanteiKougeki, Kasanari +1744, Yarare +1776). Part C's 2x256 table at 0x7B8..0xFB7 and part B's Yarare hit history lie wholly above this range; no overlap.
* Pointer-scaling pitfall: in Hex-Rays text, `*(v + N)` on a `_DWORD *` variable means byte offset 4*N; all evidence here was checked on scaled ctree offsets.
* The three motion tracks differ from RBO: 5 callbacks (step/copyOutVelocity/loadVelocity/getVelocityY/flipHorizontal) and a 68-byte payload union (kinds 1,2,4,5).
