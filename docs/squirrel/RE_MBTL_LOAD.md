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
