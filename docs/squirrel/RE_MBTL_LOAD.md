# MBTL: how a character's scripts load and how moves resolve (IDA, 2026-10-15)

MBTL Steam build 25646393, IDB `C:\dev\ida\server\MBTL_25646393.exe.i64`. Everything below is traced in the exe or read from the
game's own scripts; nothing is inferred from file names. Replaces the guessed rules that were reverted in a08d0aa.

## Load order (`Chara_LoadBattleScripts` 0x607650, log "CharaStatusData_LoadFromFile_Sq")

| step | exe | what it does |
|---|---|---|
| 1 | `CharaScript_BuildScPath` 0x608C90 + `CharaDatSq_LoadBattleStatus` 0x802A20 | `<dir>/<chr>_sc_N.txt` into its own `Battle_Status` table (charadatsq_sc.cpp) |
| 2 | `CharaScript_BuildCmdPath` 0x608FB0 + `Chara_LoadCommandList` 0x6089B0 → `CBtlSkillCommandList_LoadFromScript` 0x812A80 | runs the cmd file (`Sq_RunScriptFile` 0x815380), then reads root slot `<CHR>_CommandTable` |
| 3 | `CharaScript_BuildMvPath` 0x608E10 + `Chara_LoadMoveList` 0x6085A0 | runs the mv file, then reads root slot `<CHR>_MoveTable` into `CBtlMoveList` (stored at chara +0x8FC; the table object at list +0x30) |
| 4 | | `<dir>/_finish_list.txt` (CBtlFinishList) |
| 5 | `sub_606B60` → `CharaScript_BuildComPath` 0x606EE0 | COM (CPU AI) files; `data/_combase.txt` (defines the script classes `CCom`, `CComSkill`, `CComMove`) is loaded by `sub_7F4710` together with `script/Battle_Init.txt` |

Variant suffix: 0 → `_0`, 9 → `_9` or `_N` (N > 0), 999 → no suffix.

`CCom` is NOT an engine class: it is a Squirrel class in `data/_combase.txt`. The engine-side AI object is `BCom` (Sqrat class
`CComUpdateSt`, registered in `sub_5405D0`).

## Command table (`CBtlSkillCommandList_LoadFromScript`, entry parser `sub_8105C0`)

If `<CHR>_CommandTable` is missing or null after the cmd file runs, the list stays empty: the character cannot act
(BootySama: "cmd errors don't let the character move"). Each array entry (212 bytes) reads the keys `command`, `Check`,
`CheckAlive` and `UpdateTable` { `name`, `cmdparam`, `code0`, `code1`, `sparkdisableattack`, `sparkdisablemove` }.
`UpdateTable.name` is kept as a string (entry +0x78). Keys such as `num`, `flags`, `CmdCheck`, `MvCheck` are consumed by the
script library (Battle_Std) before the engine sees the table.

## Move switch (`sub_5072E0`, native `SetNextMoveTable` 0x809C40)

`BMvTbl.SetNextMoveTable(name)` only queues the name (`sub_8058A0`). On the switch the engine calls the old move's
`LastUpdate`, copies the queued name to the current move (chara +0x744), then calls the new move's `Init`.
Calls go through `sub_4223A0` → `SquirrelObjectManagerPush` 0x422280: `MoveTable[name][func]`; when the move or the function
is missing the call is skipped (returns 0). So a name the move table does not have does not crash at the switch: the
character is in a move with no script functions.

## The move table is built by script (`Battle_Std::MakeMoveTable`, script/btl_ChrTableFunc.txt:780)

* Every `UpdateTable.name` of the command table gets an entry (dev comment: "the list of mvs that must exist or errors
  appear"), plus every move the mv file defines.
* `MakeMoveTmpl` (line 940) builds each move from its name: `Mv_Skill_<X>` uses pattern code `<X>` (`mvname.slice(9)`),
  overridable with `usepat` in the mv file's table; flags/fgp likewise; EX/SP/BC/LastArc/_Add/_JAdd/_Hit/_End are read from
  the name. Pattern numbers come from `BMvEff.GetPatternNum({datatype, pat})`.
* `GetStdMoveTable` merges the standard moves; `GetMoveTableFromTmpl` / `GetMvFromTmpl` (line 545) convert to the final
  table and wrap `Init_Before/Init_Std/Init_After`.
* The developers' own checks: `btl_debError_CharacterMake("【警告】Initがありません:"+name)` (GetMvFromTmpl) and
  `ErrorCheckMoveTable` (line 913: "Initがねぇ", "Finalizeがねぇ").

## Consequences for Hantei-chan

* Run the real pipeline (bootstrap, then sc, cmd, mv in this order, then `_combase` + COM) and read the built
  `<CHR>_MoveTable` / `<CHR>_CommandTable` from the VM instead of re-implementing the binding rules.
* Report the developers' own `btl_debError_*` messages as problems.
* Load-time natives must return real values: `GetMvStatus().CharaNo` / `GetCharaNo` (the character folder),
  `GetPatternNum` (the HA6 PTCN table; datatype 1 = effect HA6), `GetVector_FromAngle` (math), `GetBossFlag`, `IsFileAlive`.
  Return classes and fields come from the Sqrat registrations (next section, extractor).

## Sqrat classes (extraction method)

Instance-returning wrappers (`SqratWrap_instance__*`) push a per-class static (e.g. `GetMvStatus` → `SqratWrap_instance__noargs_8`
→ `dword_364B874`). The registration that fills that static stores `Sqrat::Class<BtlMvFunc::MvStatus,...>::vftable` next to
`push offset dword_364B874; call Sqrat_CreateClass`, then binds fields with `Sqrat_Class_Var*` (name, byte offset) and ends with
`Sqrat_RootTable_BindTable("MvStatus")`. Field types come from the binder's getter: `*(_DWORD*)` int, `*(__int16*)` short,
`unsigned __int8` bool (sub_455E40).

## Battle VM setup (`Sq_InitBattleVM` 0x7F4710)

`Sq_RegisterBattleObjectFunctions`, then `./script/Battle_Init.txt` ("BattleInit"), `./script/Battle_Std.txt` ("BattleScript"),
then `./data/_combase.txt` ("_com_base_"). The sc file runs through `Sq_RunScriptBuffer` 0x815680 in the same VM; the engine
reads root `Battle_Status` {HpDivValue, HpDivRatio, CP_VecMinus_Ground/Air/AirBound, CP_Time_Val, Blast_DamagePlus, HeatStatus,
MoonStatus{ValueMax}, MvActionParam, VS_AtkVal_HP[4], VS_DefVal_HP[4], VS_HealVal[2], GuardDam_CorrectLower/CorrectHeal/
StopTime, GRD{AutoFramePlus, BoundTimePlus, ShieldFramePlus, JustShieldPlus, PassiveMovePlus, PressureVal_Min/Max, MaxKeepTime,
BreakTime, OverBreakTime, JustShieldMinus, FaildGuardMinus, Liberate_AutoPlusVal, ReduceSuccessPlus, JudgeWin_*, JudgeBreak_*}}
and deletes it (`Sq_DeleteRootSlot` 0x815580 runs `if ("X" in getroottable()) { delete X; }`).

## Load-time natives (every native the vanilla character loads call; `sqtool check --chars --calls`)

The character is pushed on the engine's object stack after its sc file and before LoadCommandList; natives reading "the
current object" see nothing during the VM setup and the sc file.

| native | exe | load-time behaviour (implemented in src/squirrel/sq_api.cpp `CallTracedNative`) |
|---|---|---|
| BMvEff.GetPatternNum({pat, datatype=0}) | 0x7FDAD0 → `PatternNum_FromSqObject` 0x444450 → `CharaObj_FindPatternByCodeName` 0x41E770 | no object → -1; integer pat → itself; string pat, datatype 0 → first PTCN match in the character's HA6 stack, 1 → `g_ExtraPlayerObject` (data/effect.txt); else -1 |
| BtlMvStd.CheckHan6Data | 0x80B450 | release build: `return 0` ("0 = debug off"; the developers' debug build returns -1/-2 for missing pattern/frames) |
| BMvCore.GetCharaNo | 0x7F5360 | player data chara number; -1 without an object |
| BMvTbl.GetCharaNo | 0x80A130 | object +5; 0 without an object |
| BMvTbl.GetMvStatus().CharaNo | 0x80A430 | object +5 (the other MvStatus fields are battle state; vanilla loads only read CharaNo) |
| BMvCore.VsScreen_GetCharaNo | 0x7F5300 | `g_VsScreenCharaNo` 0xC0BE30, the chara-list index the loader set (sqtool: folder number) |
| BMvTbl.GetPlayerSide | 0x80A110 | object side byte (+1260) != 0 (sqtool `--side`) |
| MvStageStatus.IsTrainingBattle | 0x808020 | game mode: 0+sub 1 or 3 → 1, 4 → 2, 5 → 3, else 0 (sqtool `--mode`/`--training`) |
| BMvTbl.GetRapidBeatType(i) | 0x8064C0 | root chara +20 (0..3): i=0 → {0,1,2,0}, i=1 → {0,1,1,1}; 0 in modes 4/5 (sqtool `--rapidbeat`) |
| BMvEff.GetVector_FromAngle({angle, speed=100}) | 0x7FBF60 | x = sin(angle·π)·speed, y = cos(angle·π)·−speed (angle in half turns: Battle_Std passes angle/10000·2) |
| BtlMvStd.IsFileAlive | | file existence inside the game root |

## What a missing pattern does

`Battle_Std::MakeMoveTmpl` looks up a template move's pattern by code (`Mv_Skill_<X>` → `<X>`, or `usepat`). In the release
build `CheckHan6Data` is 0, so a missing code becomes `GetPatternNum` = -1 and the move's `Init_Std` calls
`BMvTbl.SetPattern(-1)` → `Sq_SetPattern` 0x80B0D0 → `Object_SetPatternFrame(obj, -1, 0)` 0x510800: pattern -1 = keep the
current pattern, frame 0. Entering such a move keeps whatever the character was showing and restarts it from frame 0; it does
not crash. The standard move builders (`GetStdMoveTable`, `makemv_*`) also probe optional per-character codes (BlastFall,
P_AtkA, ...); those misses are by design and are not tied to a command. `sqtool --chars` reports a missing code only for a
template move the character's `<CHR>_CommandTable` can start (UpdateTable.name), with the move name read from the calling
script frame (`mvname`). Vanilla MBTL: none.
