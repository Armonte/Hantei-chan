# Native Squirrel API (generated from the IDB, do not edit)

Source: `mbtl_raw.json`. Counts: const 386, func 615, table 61, var 118.

Types are inferred from the Sqrat wrapper (`sq_get*`/`sq_push*` calls); `value` = pushes a non-primitive, `string` may also accept an int via Sqrat::Object.

## (root/unbound:sub_4155C0) (2)

- `value SQ_LoadString(object)` func native `sub_415EC0` wrapper `sub_415EC0`
- `int __dofile__(object)` func native `sub_416010` wrapper `sub_416010`

## SeFunc (2)

- `int GetParam(int)` func native `sub_4BAA90` wrapper `SqratWrap_int__int`
- `void SetParam(int, int)` func native `sub_4BAAA0` wrapper `SqratWrap_void__int_int`

## BCom (7)

- `int Move_Jump(string)` func native `sub_541480` wrapper `SqratWrap_int__string`
- `int Move_Next()` func native `sub_541460` wrapper `SqratWrap_int__noargs`
- `int Move_End(string)` func native `sub_541360` wrapper `SqratWrap_int__string`
- `string GetMoveTag()` func native `sub_541320` wrapper `SqratWrap_string__noargs`
- `int SetReversalID(int)` func native `sub_5412E0` wrapper `SqratWrap_int__int`
- `int Tutorial_GetSkillPos()` func native `sub_5412D0` wrapper `SqratWrap_int__noargs`
- `instance GetStatus()` func native `sub_541280` wrapper `SqratWrap_instance__noargs`

## UpdateSt (18)

- `int GetLevel()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetSkill()` func native `sub_428960` wrapper `SqratWrap_int__noargs_2`
- `int GetMove()` func native `sub_428970` wrapper `SqratWrap_int__noargs_2`
- `int GetSkillTime()` func native `sub_428980` wrapper `SqratWrap_int__noargs_2`
- `int GetMoveTime()` func native `sub_428990` wrapper `SqratWrap_int__noargs_2`
- `int GetELenX()` func native `sub_4289A0` wrapper `SqratWrap_int__noargs_3`
- `int GetELenY()` func native `sub_4289B0` wrapper `SqratWrap_int__noargs_3`
- `int GetHPP()` func native `sub_4289C0` wrapper `SqratWrap_int__noargs_2`
- `int GetEXS()` func native `sub_4289D0` wrapper `SqratWrap_int__noargs_2`
- `int IsPMoveable()` func native `sub_428160` wrapper `SqratWrap_int__noargs_2`
- `int IsPBound()` func native `sub_4289E0` wrapper `SqratWrap_int__noargs_2`
- `int CheckPosState(int)` func native `sub_428170` wrapper `SqratWrap_int__int_2`
- `int CheckPosState_Enemy(int)` func native `sub_428190` wrapper `SqratWrap_int__int_2`
- `int CheckPState(int)` func native `sub_4281B0` wrapper `SqratWrap_int__int_2`
- `int CheckEState(int)` func native `sub_4281D0` wrapper `SqratWrap_int__int_2`
- `int GetTobiTime()` func native `sub_428A10` wrapper `SqratWrap_int__noargs_2`
- `int GetComJumpLen()` func native `sub_428A20` wrapper `SqratWrap_int__noargs_3`
- `int GetPlayerMuteki(int)` func native `sub_428A30` wrapper `SqratWrap_int__int_2`

## BMvCoreConst (77)

- const `_eComMoveFlag_MoveEnd = 1`
- const `_eComMoveFlag_ThroughEnd = 2`
- const `_eComMoveFlag_HitNext = 4`
- const `_eComMoveFlag_DamageNext = 8`
- const `_eComMoveFlag_HitObjNext = 16`
- const `_eComMoveFlag_DamageObjNext = 32`
- const `_eComMoveFlag_HitEnd = 64`
- const `_eComMoveFlag_DamageEnd = 128`
- const `_eComMoveFlag_AirNext = 256`
- const `_eComMoveFlag_GroundNext = 512`
- const `_eComMoveFlag_MoveableNext = 1024`
- const `_eComMoveFlag_MovedisableNext = 2048`
- const `_eComMoveFlag_AirEnd = 4096`
- const `_eComMoveFlag_GroundEnd = 0x2000`
- const `_eComMoveFlag_MoveableEnd = 0x4000`
- const `_eComMoveFlag_MovedisableEnd = 0x8000`
- const `_eComMoveFlag_NmCancelEnd = 0x10000`
- const `_eComMoveFlag_SpCancelEnd = 0x20000`
- const `_eComMoveFlag_CancelEnd = 196608`
- const `_eComMoveFlag_NmCancelNext = 0x40000`
- const `_eComMoveFlag_SpCancelNext = 0x80000`
- const `_eComMoveFlag_CancelNext = 786432`
- const `_eComMoveFlag_EnemyBoundNext = 0x100000`
- const `_eComMoveFlag_GuardNext = 0x200000`
- const `_eComMoveFlag_GuardEnd = 0x400000`
- const `_eComMoveFlag_EnemyAirNext = &loc_7FFFFD + 3`
- const `_eComMoveFlag_EnemyGroundNext = &unk_1000000`
- const `_eComMoveFlag_EnemyAirEnd = &unk_2000000`
- const `_eComMoveFlag_EnemyGroundEnd = 0x4000000`
- const `_eComMoveFlag_NoHitstopNext = 0x8000000`
- const `_eComMoveFlag_StickHold = 0x40000000`
- const `_eComMoveFlag_ButtonHold = 0x80000000`
- const `_eCMF_MoveEnd = 1`
- const `_eCMF_ThroughEnd = 2`
- const `_eCMF_HitNext = 4`
- const `_eCMF_DamageNext = 8`
- const `_eCMF_HitObjNext = 16`
- const `_eCMF_DamageObjNext = 32`
- const `_eCMF_HitEnd = 64`
- const `_eCMF_DamageEnd = 128`
- const `_eCMF_AirNext = 256`
- const `_eCMF_GroundNext = 512`
- const `_eCMF_MoveableNext = 1024`
- const `_eCMF_MovedisableNext = 2048`
- const `_eCMF_AirEnd = 4096`
- const `_eCMF_GroundEnd = 0x2000`
- const `_eCMF_MoveableEnd = 0x4000`
- const `_eCMF_MovedisableEnd = 0x8000`
- const `_eCMF_NmCancelEnd = 0x10000`
- const `_eCMF_SpCancelEnd = 0x20000`
- const `_eCMF_CancelEnd = 196608`
- const `_eCMF_NmCancelNext = 0x40000`
- const `_eCMF_SpCancelNext = 0x80000`
- const `_eCMF_CancelNext = 786432`
- const `_eCMF_EnemyBoundNext = 0x100000`
- const `_eCMF_GuardNext = 0x200000`
- const `_eCMF_GuardEnd = 0x400000`
- const `_eCMF_EnemyAirNext = &loc_7FFFFD + 3`
- const `_eCMF_EnemyGroundNext = &unk_1000000`
- const `_eCMF_EnemyAirEnd = &unk_2000000`
- const `_eCMF_EnemyGroundEnd = 0x4000000`
- const `_eCMF_NoHitstopNext = 0x8000000`
- const `_eCMF_StickHold = 0x40000000`
- const `_eCMF_ButtonHold = 0x80000000`
- const `_eComSkillType_Interrupt = 1`
- const `_eComSkillType_NoInterrupt = 2`
- const `_eComButton_A = 1`
- const `_eComButton_B = 2`
- const `_eComButton_C = 4`
- const `_eComButton_D = 8`
- const `_eComState_MoveAble = 1`
- const `_eComState_Guard = 2`
- const `_eComState_Damage = 4`
- const `_eComState_Grapped = 8`
- const `_eComState_Bound = 6`
- const `_eComState_Chance = 128`
- const `_eComState_Attack = 256`

## tSceneOpenSecChar (1)

- `instance GetStatus()` func native `sub_76CDF0` wrapper `SqratWrap_instance__noargs_2`

## sqfunc (1)

- `string GetExStringText(object)` func native `sub_7A8580` wrapper `SqratWrap_string__object`

## tSceneVS (1)

- `instance GetStatus()` func native `sub_7BDB20` wrapper `SqratWrap_instance__noargs_3`

## CStatus (27)

- var `WinnerNum` @48 (Sqrat_Class_VarA)
- var `Player` @12 (Sqrat_Class_VarA)
- var `Stage` @4 (Sqrat_Class_VarA)
- var `NextStage` @8 (Sqrat_Class_VarA)
- var `WinnerNumType` @52 (Sqrat_Class_VarA)
- var `WinnerNumTypeSub` @56 (Sqrat_Class_VarA)
- var `WinnerSwitchFrame` @60 (Sqrat_Class_VarA)
- var `WinnerNumTypeSub0` @56 (Sqrat_Class_VarA)
- var `WinnerSwitchFrame0` @60 (Sqrat_Class_VarA)
- var `WinnerNumTypeSub1` @64 (Sqrat_Class_VarA)
- var `WinnerSwitchFrame1` @68 (Sqrat_Class_VarA)
- var `WinnerNumTypeSub2` @72 (Sqrat_Class_VarA)
- var `WinnerSwitchFrame2` @76 (Sqrat_Class_VarA)
- var `WinStreak` @20 (Sqrat_Class_VarA)
- var `IsStageBG` @84 (Sqrat_Class_VarE)
- var `ResultType` @88 (Sqrat_Class_VarA)
- var `Level` @92 (Sqrat_Class_VarA)
- var `Rank` @96 (Sqrat_Class_VarA)
- var `NewRecord` @100 (Sqrat_Class_VarE)
- var `TotalWins` @104 (Sqrat_Class_VarA)
- var `Clear` @108 (Sqrat_Class_VarE)
- var `NecoVoiceType` @112 (Sqrat_Class_VarA)
- var `Rankchange` @120 (Sqrat_Class_VarA)
- var `RankBefore` @124 (Sqrat_Class_VarA)
- var `RankAfter` @128 (Sqrat_Class_VarA)
- var `RankStageBefore` @132 (Sqrat_Class_VarA)
- var `RankStageAfter` @136 (Sqrat_Class_VarA)

## tSceneWinDemo (1)

- `instance GetStatus()` func native `sub_7C5C20` wrapper `SqratWrap_instance__noargs_4`

## tWinMessageTableCSVCallback (1)

- `instance GetWinMesTableLotCallbackInfo()` func native `sub_7D37C0` wrapper `SqratWrap_instance__noargs_5`

## WinMesTableLotCallbackInfo (14)

- var `ModeType` @0 (Sqrat_Class_VarB)
- var `Stage` @4 (Sqrat_Class_VarB)
- var `WinCharaNo` @8 (Sqrat_Class_VarB)
- var `LoseCharaNo` @12 (Sqrat_Class_VarB)
- var `WinType` @16 (Sqrat_Class_VarB)
- var `Perfect` @20 (Sqrat_Class_VarB)
- var `StartPlayerLose` @24 (Sqrat_Class_VarB)
- var `SpecialFinish` @28 (Sqrat_Class_VarB)
- var `BaseMessageNo` @32 (Sqrat_Class_VarB)
- var `MessageNum` @36 (Sqrat_Class_VarB)
- var `MessageNo` @40 (Sqrat_Class_VarB)
- var `Average` @44 (Sqrat_Class_VarB)
- var `NecoVoiceType` @48 (Sqrat_Class_VarB)
- var `BgNo` @56 (Sqrat_Class_VarB)

## WinMesTableConst (16)

- const `_eMessageNo_Default = -1`
- const `_MODE_TYPE__ETC = 0`
- const `_MODE_TYPE__ARCADE = 1`
- const `_MODE_TYPE__BOSS_RUSH = 2`
- const `_MODE_TYPE__BOSS_RUSH_BATTLE = 3`
- const `_MODE_TYPE__VERSUS = 4`
- const `_eWinType_None = 0`
- const `_eWinType_NormalKo = 1`
- const `_eWinType_Timeup = 2`
- const `_eWinType_DoubleKo = 3`
- const `_eWinType_MissionEnd = 4`
- const `_defBtlFinish_None = 0`
- const `_defBtlFinish_Normal = 1`
- const `_defBtlFinish_EX = 10`
- const `_defBtlFinish_ArcDrive = 20`
- const `_defBtlFinish_LastArc = 30`

## BMvCore (23)

- `instance GetMyCharaData()` func native `sub_7F57B0` wrapper `SqratWrap_instance__noargs_6`
- `instance GetCaptureCharaData()` func native `sub_7F5750` wrapper `SqratWrap_instance__noargs_6`
- `instance GetParentCharaData()` func native `sub_7F5710` wrapper `SqratWrap_instance__noargs_6`
- `instance GetPlayerCharaData()` func native `sub_7F56D0` wrapper `SqratWrap_instance__noargs_6`
- `instance GetNearEnemyCharaData()` func native `sub_7F5680` wrapper `SqratWrap_instance__noargs_6`
- `value GetEnemyCharaData(string)` func native `sub_7F55F0` wrapper `SqratWrap_value__string`
- `instance GetLastDamageCharaData(int)` func native `sub_7F5550` wrapper `SqratWrap_instance__int`
- `void SetLastDamageCharaData(instance)` func native `sub_7F55A0` wrapper `SqratWrap_void__instance`
- `instance GetLastHitCharaData(int)` func native `sub_7F5500` wrapper `SqratWrap_instance__int`
- `instance GetLastCatchCharaData(int)` func native `sub_7F54B0` wrapper `SqratWrap_instance__int`
- `int CallEntryBCCachePreTransfer(string)` func native `GetObjectPatternInfo` wrapper `SqratWrap_int__string`
- `int Hiskoha_CallSwitch()` func native `sub_7F53E0` wrapper `SqratWrap_int__noargs`
- `int GetTagStatus()` func native `sub_7F5390` wrapper `SqratWrap_int__noargs`
- `int GetCharaNo()` func native `Sq_GetCharaNo` wrapper `SqratWrap_int__noargs`
- `int GetBossFlag()` func native `GetObjectCharacterData` wrapper `SqratWrap_int__noargs`
- `int GetBossRushFlag()` func native `GetObjectManagerData` wrapper `SqratWrap_int__noargs`
- `int VsScreen_GetCharaNo()` func native `sub_7F5300` wrapper `SqratWrap_int__noargs`
- `instance GetActiveCharaData()` func native `GetObjectParentPlayerPtr` wrapper `SqratWrap_instance__noargs_6`
- `instance GetPertnerCharaData()` func native `GetObjectParentPlayer` wrapper `SqratWrap_instance__noargs_6`
- `int PushCharaData(instance)` func native `SetObjectManagerPointer` wrapper `SqratWrap_int__instance`
- `int PopCharaData()` func native `GetObjectManagerBase2` wrapper `SqratWrap_int__noargs`
- `instance GetParamCharaData(int)` func native `sub_7F51C0` wrapper `SqratWrap_instance__int`
- `int IsHan6Squirrel()` func native `sub_7F50E0` wrapper `SqratWrap_int__noargs`

## CCharaData (4)

- `int push()` func native `sub_7F5120` wrapper `SqratWrap_int__noargs_3`
- `int pop()` func native `GetObjectManagerBase` wrapper `SqratWrap_int__noargs_3`
- `int isdone()` func native `sub_443190` wrapper `SqratWrap_int__noargs_3`
- `int isPlayer()` func native `sub_7F50F0` wrapper `SqratWrap_int__noargs_3`

## CFindObject (2)

- `void SetFirst()` func native `sub_4431A0` wrapper `SqratWrap_void__noargs`
- `instance Get(int)` func native `sub_4431C0` wrapper `SqratWrap_instance__int_2`

## tDDC (70)

- `int Kirifuda_Add(int)` func native `sub_7F6B30` wrapper `SqratWrap_int__int`
- `int Kirifuda_GetValue()` func native `sub_7F6A80` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_GetMax()` func native `sub_7F6A70` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_TimeAdd(int, int)` func native `sub_7F6B10` wrapper `SqratWrap_int__int_int`
- `int Kirifuda_CheckTime()` func native `sub_7F6AE0` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_CheckStatus()` func native `sub_7F6AD0` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_SetTimeValue(int, int)` func native `nullsub_53` wrapper `SqratWrap_void__int_int`
- `int Kirifuda_GetTime()` func native `sub_7F6AB0` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_SetInstallCount(int)` func native `nullsub_52` wrapper `SqratWrap_void__int`
- `void Kirifuda_TimeDamageMinus(int)` func native `nullsub_51` wrapper `SqratWrap_void__int`
- `int Kirifuda_FinalizeProc()` func native `sub_7F6B00` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_TimeClear()` func native `nullsub_54` wrapper `SqratWrap_void__noargs_2`
- `int Kirifuda_SubMax()` func native `sub_7F6B20` wrapper `SqratWrap_int__noargs`
- `void KirifudaWatch_SetEndPalette(int, int)` func native `nullsub_50` wrapper `SqratWrap_void__int_int`
- `void KirifudaWatch_SetParam(int, int)` func native `nullsub_49` wrapper `SqratWrap_void__int_int`
- `int KirifudaWatch_GetParam(int)` func native `sub_7F6A40` wrapper `SqratWrap_int__int`
- `int Blast_AddPoint(int)` func native `sub_7F6A30` wrapper `SqratWrap_int__int`
- `int Blast_IsUse()` func native `sub_7F6A20` wrapper `SqratWrap_int__noargs`
- `int Blast_Use(int)` func native `sub_7F6A10` wrapper `SqratWrap_int__int`
- `int Blast_SetParam(string)` func native `sub_7F69C0` wrapper `SqratWrap_int__string`
- `int Blast_SetFailed(int)` func native `sub_7F69B0` wrapper `SqratWrap_int__int`
- `int Blast_GetUseMode()` func native `sub_7F6A00` wrapper `SqratWrap_int__noargs`
- `int Blast_CheckClimaxMode()` func native `sub_7F69F0` wrapper `SqratWrap_int__noargs`
- `int Blast_EndClimaxMode()` func native `sub_7F69D0` wrapper `SqratWrap_int__noargs`
- `int Blast_SetClimaxTime(int, int)` func native `sub_7F69E0` wrapper `SqratWrap_int__int_int`
- `int Heat_CheckMode()` func native `sub_7F68E0` wrapper `SqratWrap_int__noargs`
- `int Heat_GetTimeCost()` func native `sub_7F68A0` wrapper `SqratWrap_int__noargs`
- `int Heat_UseGauge()` func native `sub_7F68A0` wrapper `SqratWrap_int__noargs`
- `int Moon_GetType()` func native `sub_7F6810` wrapper `SqratWrap_int__noargs`
- `int Moon_SetType(int)` func native `sub_7F67F0` wrapper `SqratWrap_int__int`
- `int Moon_GetValue()` func native `sub_7F67E0` wrapper `SqratWrap_int__noargs`
- `int Moon_SetValue(int)` func native `sub_7F67C0` wrapper `SqratWrap_int__int`
- `int Moon_AddValue(int)` func native `sub_7F6740` wrapper `SqratWrap_int__int`
- `int Moon_ChangeMoonDriveTimeSpeed(int)` func native `sub_7F67A0` wrapper `SqratWrap_int__int`
- `int Moon_MoonDriveStart(string)` func native `sub_7F66A0` wrapper `SqratWrap_int__string`
- `int Moon_MoonDriveEnd()` func native `sub_7F6680` wrapper `SqratWrap_int__noargs`
- `int Moon_MoonDriveIsInfinite()` func native `sub_7F6640` wrapper `SqratWrap_int__noargs`
- `int Moon_MoonDriveIsInfinitePlayer(int)` func native `sub_7F65B0` wrapper `SqratWrap_int__int`
- `int ReverseCorrect_Set(string)` func native `sub_7F6510` wrapper `SqratWrap_int__string`
- `int Support_IsUse()` func native `sub_7F6500` wrapper `SqratWrap_int__noargs`
- `int Support_Use()` func native `sub_7F64F0` wrapper `SqratWrap_int__noargs`
- `int Support_SetFailed(int)` func native `sub_7F64E0` wrapper `SqratWrap_int__int`
- `int Support_StartRecast(int)` func native `sub_7F64D0` wrapper `SqratWrap_int__int`
- `void Support_SetPowerupRecastMax(int)` func native `nullsub_48` wrapper `SqratWrap_void__int`
- `int Support_GetCallCount()` func native `sub_7F64B0` wrapper `SqratWrap_int__noargs`
- `int Support_SetRecastCost(int)` func native `sub_7F64A0` wrapper `SqratWrap_int__int`
- `int Support_GetRecastCost()` func native `sub_7F6490` wrapper `SqratWrap_int__noargs`
- `int Support_GetType()` func native `sub_7F6460` wrapper `SqratWrap_int__noargs`
- `int Support_GetCharaNo()` func native `sub_7F6470` wrapper `SqratWrap_int__noargs`
- `int Support_GetSelectColor()` func native `sub_7F6480` wrapper `SqratWrap_int__noargs`
- `int Support_SetMessage(object, int, int)` func native `sub_7F6450` wrapper `SqratWrap_int__object_int_int`
- `void ExJem_Attack(int, int)` func native `nullsub_47` wrapper `SqratWrap_void__int_int`
- `void Jem_AddCount(int)` func native `nullsub_46` wrapper `SqratWrap_void__int`
- `int Jem_GetRunFlag(int)` func native `sub_7F6420` wrapper `SqratWrap_int__int`
- `int Jem_IsWorking(int)` func native `sub_7F6410` wrapper `SqratWrap_int__int`
- `int Jem_IsWorkingEff(int)` func native `sub_7F6400` wrapper `SqratWrap_int__int`
- `int Jem_SetMessage(object, int)` func native `sub_7F63E0` wrapper `SqratWrap_int__object_int`
- `int Jem_SetMessageEx(int, int, object, int)` func native `sub_7F63F0` wrapper `SqratWrap_int__int_int_object_int`
- `int HP_Add(string)` func native `sub_7F6350` wrapper `SqratWrap_int__string`
- `int HP_SourceDamMode(string)` func native `sub_7F62C0` wrapper `SqratWrap_int__string`
- `void SkillCountPlus(int)` func native `nullsub_45` wrapper `SqratWrap_void__int`
- `void AchievementSousai()` func native `nullsub_44` wrapper `SqratWrap_void__noargs_2`
- `int PowerUpSys_GetCount(int)` func native `sub_7F6260` wrapper `SqratWrap_int__int`
- `int PowerUpSys_GetLast()` func native `sub_7F6280` wrapper `SqratWrap_int__noargs`
- `int PowerUpSys_IsDouble(int)` func native `sub_7F6270` wrapper `SqratWrap_int__int`
- `int SubSp_GetChara()` func native `sub_7F6250` wrapper `SqratWrap_int__noargs`
- `int SubSp_GetType()` func native `sub_7F6240` wrapper `SqratWrap_int__noargs`
- `int IsSingleMode()` func native `sub_7F6200` wrapper `SqratWrap_int__noargs`
- `int IsNotPlayMode()` func native `sub_7F61D0` wrapper `SqratWrap_int__noargs`
- `int IsNotEnemySide()` func native `sub_7F6190` wrapper `SqratWrap_int__noargs`

## BMvEff (135)

- `int Random()` func native `Sq_Random_GlobalCRandCalc` wrapper `SqratWrap_int__noargs`
- `float Random_F()` func native `Random_F` wrapper `SqratWrap_float__noargs`
- `int Random_Limit(int)` func native `Random_Limit` wrapper `SqratWrap_int__int`
- `float Random_FAbs(int)` func native `Random_FAbs` wrapper `SqratWrap_float__int`
- `value Random_PointRect(string)` func native `__std_fs_get_stats` wrapper `SqratWrap_value__string_2`
- `value Random_PointRad(string)` func native `Random_PointRad` wrapper `SqratWrap_value__string_2`
- `int StdCall(string)` func native `StdCall` wrapper `SqratWrap_int__string`
- `int ThrowRelease(string)` func native `ThrowRelease` wrapper `SqratWrap_int__string`
- `int ThrowParam(string)` func native `ThrowParam` wrapper `SqratWrap_int__string`
- `int ThrowChara_SetJoint(int)` func native `ThrowChara_SetJoint` wrapper `SqratWrap_int__int`
- `int ThrowChara_SetCamera(int)` func native `ThrowChara_SetCamera` wrapper `SqratWrap_int__int`
- `int ThrowChara_Transfer(string)` func native `ThrowChara_Transfer` wrapper `SqratWrap_int__string`
- `int CapturePlayer(string)` func native `CapturePlayer` wrapper `SqratWrap_int__string`
- `int SetHitLimitCancel(int)` func native `SetHitLimitCancel` wrapper `SqratWrap_int__int`
- `int GetFrameIDNum(int)` func native `GetFrameIDNum` wrapper `SqratWrap_int__int`
- `int GetPatternNum(string)` func native `GetPatternNum` wrapper `SqratWrap_int__string`
- `value CreateObject(string)` func native `CreateObject` wrapper `SqratWrap_value__string`
- `int SetObjectFlags(string)` func native `SetObjectFlags` wrapper `SqratWrap_int__string`
- `int CheckObjectFlags(string)` func native `CheckObjectFlags` wrapper `SqratWrap_int__string`
- `void SetObjectRender(string)` func native `SetObjectRender` wrapper `SqratWrap_void__string`
- `int EraseObjectFlags(string)` func native `EraseObjectFlags` wrapper `SqratWrap_int__string`
- `int ObjProcFlags_Set(string)` func native `ObjProcFlags_Set` wrapper `SqratWrap_int__string`
- `int ObjProcFlags_Erase(string)` func native `ObjProcFlags_Erase` wrapper `SqratWrap_int__string`
- `int ObjType_Set(string)` func native `ObjType_Set` wrapper `SqratWrap_int__string`
- `int SetCamera_Quake(string)` func native `SetCamera_Quake` wrapper `SqratWrap_int__string`
- `int SetCamera_Focus(string)` func native `SetCamera_Focus` wrapper `SqratWrap_int__string`
- `void SetCamera_FocusKeep(int)` func native `SetCamera_FocusKeep` wrapper `SqratWrap_void__int`
- `void SetCamera_Clipping(int)` func native `SetCamera_Clipping` wrapper `SqratWrap_void__int`
- `int GetCamera_Clipping()` func native `GetCamera_Clipping` wrapper `SqratWrap_int__noargs`
- `void SetCamera_UpdateFlag(int, int)` func native `SetCamera_UpdateFlag` wrapper `SqratWrap_void__int_int`
- `void ResetCamera()` func native `ResetCamera` wrapper `SqratWrap_void__noargs_2`
- `void UpdateCamera()` func native `UpdateCamera` wrapper `SqratWrap_void__noargs_2`
- `void ResetViewCamera()` func native `ResetViewCamera` wrapper `SqratWrap_void__noargs_2`
- `void CameraShift_Set(string)` func native `CameraShift_Set` wrapper `SqratWrap_void__string`
- `void CameraShift_Clear()` func native `CameraShift_Clear` wrapper `SqratWrap_void__noargs_2`
- `int SetStopTime(string)` func native `SetStopTime` wrapper `SqratWrap_int__string`
- `int SetStopTimeAll(string)` func native `SetStopTimeAll` wrapper `SqratWrap_int__string`
- `int IsMyStopTime(int)` func native `IsMyStopTime` wrapper `SqratWrap_int__int`
- `void ForceKO()` func native `ForceKO` wrapper `SqratWrap_void__noargs_2`
- `int GetKOMode()` func native `GetKOMode` wrapper `SqratWrap_int__noargs`
- `int GetKOFadeOutTime()` func native `GetKOFadeOutTime` wrapper `SqratWrap_int__noargs`
- `int IsKOSkip()` func native `IsKOSkip` wrapper `SqratWrap_int__noargs`
- `int IsKoFinishType()` func native `IsKoFinishType` wrapper `SqratWrap_int__noargs`
- `void SetNoDamageFlag(int)` func native `SetNoDamageFlag` wrapper `SqratWrap_void__int`
- `int SetHpGauge(string)` func native `SetHpGauge` wrapper `SqratWrap_int__string`
- `int SetSpGauge(string)` func native `SetSpGauge` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimit(string)` func native `SpGauge_SetLimit` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimitCombo(string)` func native `SpGauge_SetLimitCombo` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimitBound(string)` func native `SpGauge_SetLimitBound` wrapper `SqratWrap_int__string`
- `int SpGauge_SetZeroTime(string)` func native `SpGauge_SetZeroTime` wrapper `SqratWrap_int__string`
- `int SpGauge_SetAwakens(int)` func native `SpGauge_SetAwakens` wrapper `SqratWrap_int__int`
- `int SpGauge_GetAwakens()` func native `SpGauge_GetAwakens` wrapper `SqratWrap_int__noargs`
- `int SpGauge_GetPlusCorrect_ComboTimePos()` func native `SpGauge_GetPlusCorrect_ComboTimePos` wrapper `SqratWrap_int__noargs`
- `void SetExist(string)` func native `SetExist` wrapper `SqratWrap_void__string`
- `void SetPaletteSlot(int, int)` func native `SetPaletteSlot` wrapper `SqratWrap_void__int_int`
- `int GetPaletteSlot()` func native `GetPaletteSlot` wrapper `SqratWrap_int__noargs`
- `void SetOtherPalette(int, int)` func native `SetOtherPalette` wrapper `SqratWrap_void__int_int`
- `instance GetPointStatus(string)` func native `__std_fs_get_stats` wrapper `SqratWrap_instance__string`
- `instance GetVector_FromAngle(string)` func native `GetVector_FromAngle` wrapper `SqratWrap_instance__string_2`
- `instance GetAngle_FromVector(string)` func native `GetAngle_FromVector` wrapper `SqratWrap_instance__string`
- `int SetBoundSt(string)` func native `SetBoundSt` wrapper `SqratWrap_int__string`
- `int SetUkemiTime(string)` func native `SetUkemiTime` wrapper `SqratWrap_int__string`
- `int VecHitFlag_Set(int, int)` func native `VecHitFlag_Set` wrapper `SqratWrap_int__int_int`
- `value CheckHantei(string)` func native `CheckHantei` wrapper `SqratWrap_value__string`
- `value CheckHantei2(string)` func native `CheckHantei2` wrapper `SqratWrap_value__string`
- `instance GetHanteiRect(string)` func native `__std_fs_get_stats` wrapper `SqratWrap_instance__string_3`
- `int IsAttackRect()` func native `IsAttackRect` wrapper `SqratWrap_int__noargs`
- `int IsKuraiRect()` func native `IsKuraiRect` wrapper `SqratWrap_int__noargs`
- `value GetCameraPosition(string)` func native `GetCameraPosition` wrapper `SqratWrap_value__string_2`
- `instance GetCameraRect(string)` func native `GetCameraRect` wrapper `SqratWrap_instance__string_3`
- `int SetPlayerTimer(string)` func native `SetPlayerTimer` wrapper `SqratWrap_int__string`
- `int GetPlayerMuteki(int)` func native `GetPlayerMuteki` wrapper `SqratWrap_int__int`
- `float GetBoundVectorAngle(int)` func native `GetBoundVectorAngle` wrapper `SqratWrap_float__int`
- `int AddAttackHitNum(int)` func native `AddAttackHitNum` wrapper `SqratWrap_int__int`
- `void ClearAttackHitNum()` func native `ClearAttackHitNum` wrapper `SqratWrap_void__noargs_2`
- `instance GetAttackHitPos()` func native `GetAttackHitPos` wrapper `SqratWrap_instance__noargs_7`
- `int Liberate_OK()` func native `Liberate_OK` wrapper `SqratWrap_int__noargs`
- `void Liberate_Set()` func native `Liberate_Set` wrapper `SqratWrap_void__noargs_2`
- `void Liberate_SetMinusValue(int)` func native `Liberate_SetMinusValue` wrapper `SqratWrap_void__int`
- `int Liberate_Get()` func native `Liberate_Get` wrapper `SqratWrap_int__noargs`
- `int Liberate_Calc(string)` func native `Liberate_Calc` wrapper `SqratWrap_int__string`
- `void Liberate_End()` func native `Liberate_End` wrapper `SqratWrap_void__noargs_2`
- `void Liberate_SetType(int)` func native `Liberate_SetType` wrapper `SqratWrap_void__int`
- `int Liberate_GetType()` func native `Liberate_GetType` wrapper `SqratWrap_int__noargs`
- `int Liberate_GetTimeValue()` func native `Liberate_GetTimeValue` wrapper `SqratWrap_int__noargs`
- `int GuardSP_Set(string)` func native `GuardSP_Set` wrapper `SqratWrap_int__string`
- `int GuardSP_Get()` func native `GuardSP_Get` wrapper `SqratWrap_int__noargs`
- `int GuardSP_Success()` func native `GuardSP_Success` wrapper `SqratWrap_int__noargs`
- `int ClearGuardSP_Success()` func native `ClearGuardSP_Success` wrapper `SqratWrap_int__noargs`
- `void AttackInfoString_Set(string)` func native `AttackInfoString_Set` wrapper `SqratWrap_void__string`
- `void FadeProc_SetRenderFlag(int)` func native `FadeProc_SetRenderFlag` wrapper `SqratWrap_void__int`
- `int FadeProc_Set(string)` func native `FadeProc_Set` wrapper `SqratWrap_int__string`
- `int FadeProc_Erase(string)` func native `FadeProc_Erase` wrapper `SqratWrap_int__string`
- `int CutInProc_Set(string)` func native `CutInProc_Set` wrapper `SqratWrap_int__string`
- `int CutInProc_Erase(string)` func native `CutInProc_Erase` wrapper `SqratWrap_int__string`
- `int SetCharaFlash(string)` func native `SetCharaFlash` wrapper `SqratWrap_int__string`
- `int SetCharaColor(string)` func native `SetCharaColor` wrapper `SqratWrap_int__string`
- `int SetCharaDrawType(string)` func native `SetCharaDrawType` wrapper `SqratWrap_int__string`
- `int PcAfterImage_Set(string)` func native `PcAfterImage_Set` wrapper `SqratWrap_int__string`
- `int PcAfterImage_Clear()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int PcAfterImage_ClearEx(int)` func native `PcAfterImage_ClearEx` wrapper `SqratWrap_int__int`
- `int PcAuraEffect_Set(string)` func native `PcAuraEffect_Set` wrapper `SqratWrap_int__string`
- `int PcAuraEffect_Clear()` func native `PcAuraEffect_Clear` wrapper `SqratWrap_int__noargs`
- `int Slowmotion_Set(string)` func native `Slowmotion_Set` wrapper `SqratWrap_int__string`
- `int LinePrim_Set(string)` func native `LinePrim_Set` wrapper `SqratWrap_int__string`
- `int LinePrim_SetTypeParam(string)` func native `__std_fs_get_stats` wrapper `SqratWrap_int__string`
- `int Cockpit_SetView(string)` func native `Cockpit_SetView` wrapper `SqratWrap_int__string`
- `int Cockpit_SetPrioU(int)` func native `Cockpit_SetPrioU` wrapper `SqratWrap_int__int`
- `void ComboView_Clear()` func native `ComboView_Clear` wrapper `SqratWrap_void__noargs_2`
- `int ComboView_Set(string)` func native `ComboView_Set` wrapper `SqratWrap_int__string`
- `void ChangeStartCorrectTiming()` func native `ChangeStartCorrectTiming` wrapper `SqratWrap_void__noargs_2`
- `void ChangeStartCorrectTimingPlus()` func native `ChangeStartCorrectTimingPlus` wrapper `SqratWrap_void__noargs_2`
- `void BGM_Set(string)` func native `BGM_Set` wrapper `SqratWrap_void__string`
- `int BG_GetNum()` func native `BG_GetNum` wrapper `SqratWrap_int__noargs`
- `int AddSkillCount(int)` func native `AddSkillCount` wrapper `SqratWrap_int__int`
- `int GetEntranceCallCount()` func native `GetEntranceCallCount` wrapper `SqratWrap_int__noargs`
- `int IsReplayEntrance()` func native `IsReplayEntrance` wrapper `SqratWrap_int__noargs`
- `void SetBgPrioEx(string)` func native `SetBgPrioEx` wrapper `SqratWrap_void__string`
- `int GetMyObjectCount()` func native `GetMyObjectCount` wrapper `SqratWrap_int__noargs`
- `void SetGuardPlusFlag(int)` func native `SetGuardPlusFlag` wrapper `SqratWrap_void__int`
- `int Chara_SetMessage(int, int, object, int, int)` func native `Chara_SetMessage` wrapper `SqratWrap_int__int_int_object_int_int`
- `int Chara_SetMessageBattle(int, int, object, int, int)` func native `Chara_SetMessageBattle` wrapper `SqratWrap_int__int_int_object_int_int`
- `int Chara_SetMessagePC(int, int, object, int, int)` func native `Chara_SetMessage` wrapper `SqratWrap_int__int_int_object_int_int`
- `void Chara_SetMessageFontSize(int)` func native `Chara_SetMessageFontSize` wrapper `SqratWrap_void__int`
- `void Chara_SetMessageDelay(int)` func native `Chara_SetMessageDelay` wrapper `SqratWrap_void__int`
- `int Chara_JoinMessage(int)` func native `Chara_JoinMessage` wrapper `SqratWrap_int__int`
- `void Chara_ClearMessageAll()` func native `Chara_ClearMessageAll` wrapper `SqratWrap_void__noargs_2`
- `void SetAllKeyCheck(int)` func native `SetAllKeyCheck` wrapper `SqratWrap_void__int`
- `void SetDefaultExtendedEffectDrawing(int)` func native `SetDefaultExtendedEffectDrawing` wrapper `SqratWrap_void__int`
- `void Score_Add(int)` func native `Score_Add` wrapper `SqratWrap_void__int`
- `void SetBgColor(string)` func native `SetBgColor` wrapper `SqratWrap_void__string`
- `void SetBgSpeculer(string)` func native `SetBgSpeculer` wrapper `SqratWrap_void__string`
- `int GetSeType()` func native `GetSeType` wrapper `SqratWrap_int__noargs`
- `int TM_GetCharaGauge(int)` func native `TrainingMenu_GetCharaGaugeValue` wrapper `SqratWrap_int__int`
- `int TM_IsEnemyAllShield()` func native `TrainingMenu_IsEnemyAllShield` wrapper `SqratWrap_int__noargs`

## CPointStatus (5)

- var `distance` @0 (Sqrat_Class_VarD)
- var `distance_x` @4 (Sqrat_Class_VarD)
- var `distance_y` @8 (Sqrat_Class_VarD)
- var `pos_x` @12 (Sqrat_Class_VarD)
- var `pos_y` @16 (Sqrat_Class_VarD)

## BMvConst (264)

- const `_ObjFlags_ParentMove = 4`
- const `_ObjFlags_EraseParentDamage = 1`
- const `_ObjFlags_NoRender = 0x20000000`
- const `_ObjFlags_NoRenderBlackOut = 0x200000`
- const `_ObjFlags_NoRenderOrder = 0x100000`
- const `_ObjFlags_NoRenderOrderPlus = 0x40000000`
- const `_ObjFlags_EraseParentPatChange = 32`
- const `_ObjFlags_PatChangeNoLanding = 0x4000`
- const `_ObjFlags_MoveTimeStopAll = 0x10000000`
- const `_ObjFlags_ParentMuki = 4096`
- const `_ObjFlags_MukiXPosMove = 0x2000`
- const `_ObjFlags_RenderShadow = 0x10000`
- const `_ObjFlags_FromParentStop = 8`
- const `_ObjFlags_ToParentStop = 0x80000`
- const `_ObjFlags_ToParentHitBack = 0x8000`
- const `_ObjFlags_ToParentHitStatus = 0x40000`
- const `_ObjFlags_NoCamera = &loc_7FFFFD + 3`
- const `_ObjFlags_NoGround = 64`
- const `_ObjFlags_MoveTimeStop = 0x400000`
- const `_ObjProcFlags_EraseParentNull = 1`
- const `_ObjProcFlags_EraseChangeParentMv = 2`
- const `_ObjProcFlag_UnEraseDamageEx = 4`
- const `_ObjProcFlag_EraseParentDelete = 8`
- const `_ObjType_FireBall = 2`
- const `_ObjType_Blade = 3`
- const `_Exist_NoCamera = 1`
- const `_Exist_NoWall = 2`
- const `_Exist_NoHantei = 3840`
- const `_Exist_NoKasanariHantei = 256`
- const `_Exist_NoKuraiHantei = 512`
- const `_Exist_NoAttackHantei = 1024`
- const `_Exist_NoEtcHantei = 2048`
- const `_ExistMode_Set = 0`
- const `_ExistMode_Add = 1`
- const `_ExistMode_Erase = 2`
- const `_ExistMode_Reverse = 3`
- const `_Hantei_Kasanari = 0`
- const `_Hantei_Kurai = 1`
- const `_Hantei_Etc = 2`
- const `_Hantei_Attack = 3`
- const `_HC_EnemyPc = 1`
- const `_HC_EnemyObj = 2`
- const `_HC_FavourPc = 4`
- const `_HC_FavourObj = 8`
- const `_HC_WithoutNoHanteiFlagObj = 16`
- const `_CamUpdateFlag_NoX = 1`
- const `_CamUpdateFlag_NoY = 2`
- const `_Hantei_Error = -7654321`
- const `_HanteiFlag_Tool = 1`
- const `_HanteiFlag_Offset = 2`
- const `_HanteiFlag_NoMukiChange = 4`
- const `_SpGauge_UseAll = -1`
- const `_SpGauge_UseBuffer = -2`
- const `_GetPos_TrueCamera = 1`
- const `_GetPos_ViewCamera = 2`
- const `_GetPos_DispCamera = 4`
- const `_GetPos_Offset = 256`
- const `_GetPos_TypeWall = 512`
- const `_GetPos_TypeScreen = 1024`
- const `_GetPos_NoMuki = 2048`
- const `_GetPos_ToolOffset = 4096`
- const `_SpGaugeMode_Normal = 0`
- const `_SpGaugeMode_Liberate = 1`
- const `_SpGaugeMode_OverLiberate = 2`
- const `_ThrowRelease_NoGroundRecover = 16`
- const `_ThrowRelease_NoWallRecover = 32`
- const `_ThrowRelease_NoAttackHit = 64`
- const `_ThrowRelease_ReverseVec = 2`
- const `_ThrowRelease_NoVec = 1`
- const `_ThrowRelease_NoVecTimeHosei = 0x10000`
- const `_ThrowRelease_NoBurst = 0x4000`
- const `_SkillCount_Assult = 0`
- const `_SkillCount_ChainShift = 1`
- const `_SkillCount_VeilOff = 2`
- const `_SkillCount_Sp = 3`
- const `_SkillCount_SpEx = 4`
- const `_SkillCount_SpIFW = 5`
- const `_SkillCount_SpIFWX = 6`
- const `_SkillCount_Throw = 7`
- const `_SkillCount_ThrowRecover = 8`
- const `_GuardFlag_GuardStand = 1`
- const `_GuardFlag_GuardAir = 2`
- const `_GuardFlag_GuardCrouch = 4`
- const `_GuardFlag_ShieldStand = 16`
- const `_GuardFlag_ShieldAir = 32`
- const `_GuardFlag_ShieldCrouch = 64`
- const `_GuardFlag_ShieldXStand = 0x80000`
- const `_GuardFlag_ShieldXAir = 0x100000`
- const `_GuardFlag_ShieldXCrouch = 0x200000`
- const `_GuardFlag_ThroughStand = 256`
- const `_GuardFlag_ThroughAir = 512`
- const `_GuardFlag_ThroughCrouch = 1024`
- const `_GuardFlag_ThroughBound = 2048`
- const `_GuardFlag_ThroughGuardBound = 4096`
- const `_GuardFlag_ThroughDownBound = 0x2000`
- const `_GuardFlag_ThroughExceptBound = 0x4000`
- const `_GuardFlag_ThroughExceptDownBound = 0x8000`
- const `_GuardFlag_ThroughRemoveBound = 0x10000`
- const `_GuardFlag_ThroughRemoveDamage = 0x20000`
- const `_GuardFlag_ThroughRemoveDamagePlus = 0x40000`
- const `_DataType_Me = 0`
- const `_DataType_Player = 1`
- const `_DataType_Parent = 2`
- const `_KOMode_ToEndWait = 208`
- const `_KOMode_ToEndWait_Fade = 209`
- const `_LiberateType_None = 0`
- const `_LiberateType_Heat = 1`
- const `_LiberateType_BloodHeat = 1`
- const `_LiberateType_Combo = 3`
- const `_isDebugModeExe = 0`
- const `_PlatformType = 0`
- const `_PlatformAPM = 0`
- const `_DevWindows = 1`
- const `_DevPS3 = 0`
- const `_DevVITA = 0`
- const `_DevPS4 = 0`
- const `_PosState_Stand = 1`
- const `_PosState_Air = 2`
- const `_PosState_Crouch = 4`
- const `_PosState_Ground = 5`
- const `_PosState_Always = 7`
- const `_PrioType_ = 7`
- const `_HitType_Hit = 7`
- const `_HitType_Damage = 2`
- const `_HitType_Guard = 1`
- const `_HitType_Sousai = 4`
- const `_HitType_FlagSousai = 16`
- const `_HitType_SuperArmor = 8`
- const `_HitType_Player = 32`
- const `_HitType_Partner = 64`
- const `_Direction_Auto = -1`
- const `_Direction_Left = 0`
- const `_Direction_Right = 1`
- const `_Direction_Reverse = 10`
- const `_Position_CaptureChara = 1`
- const `_Position_CaptureShift = 2`
- const `_Position_ToolShift = 2`
- const `_Position_ChangeMuki = 4`
- const `_Position_Add = 8`
- const `_Position_NoMoveChild = 16`
- const `_Angle_AngleAdd = 1`
- const `_Angle_PositionAdd = 2`
- const `_VecFlag_Add = 1`
- const `_VecFlag_NoMuki = 2`
- const `_Vector_Normal = 0x80000000`
- const `_Vector_Bound = 0x40000000`
- const `_Vector_Div = 0x20000000`
- const `_Vector_Keep = 0x10000000`
- const `_Vector_DivKeep = 0x8000000`
- const `_ValSet = 0`
- const `_ValGet = 1`
- const `_ValAdd = 2`
- const `_ValClear = 3`
- const `_ValErase = 4`
- const `_ValCheck = 5`
- const `_InterruptType_Ko_Atk = 0`
- const `_InterruptType_Ko_Def = 1`
- const `_InterruptType_Judge = 2`
- const `_InterruptType_DrawMotion = 3`
- const `_InterruptType_WinMotion = 4`
- const `_InterruptType_LoseMotion = 5`
- const `_InterruptType_SupportCharge = 6`
- const `_InterruptType_Jem = 7`
- const `_InterruptType_BlastCharge = 8`
- const `_InterruptType_TM_MoonDriveReset = 9`
- const `_ImpactFlag_IsBound = 1`
- const `_ImpactFlag_IsGuard = 2`
- const `_ImpactFlag_IsCounter = 128`
- const `_ImpactFlag_IsDangerHP = 64`
- const `_ImpactFlag_IsFirstAttack = 32`
- const `_ImpactFlag_IsCapture = 16`
- const `_ImpactFlag_IsSuperArmor = 512`
- const `_ImpactFlag_IsBound_Old = 256`
- const `_CancelFlag_None = 0`
- const `_CancelFlag_Hit = 1`
- const `_CancelFlag_Always = 2`
- const `_CancelFlag_Damage = 3`
- const `_CancelFlag_Invalid = 255`
- const `_ClearFlag_ChangeFrame = 2`
- const `_ClearFlag_ChangePattern = 4`
- const `_ClearFlag_ChangeMv = 1`
- const `_ClearFlag_ComboEnd = 64`
- const `_AsFlag_ExCancel = 1`
- const `_AsFlag_ChainShift = 2`
- const `_AsFlag_GuardRev = 4`
- const `_AsFlag_Guard_Stand = 16`
- const `_AsFlag_Guard_Air = 32`
- const `_AsFlag_Guard_Crouch = 64`
- const `_AsFlag_DamageEx = 256`
- const `_CharaPrio_None = 0`
- const `_CharaPrio_Near = 1`
- const `_CharaPrio_Far = 2`
- const `_CharaPrio_Near_Layer_0 = 3`
- const `_CharaPrio_Near_Layer_1 = 4`
- const `_CharaPrio_Near_Layer_2 = 5`
- const `_CharaPrio_Near_Layer_3 = 6`
- const `_CharaPrio_Near_Layer_4 = 7`
- const `_CharaPrio_Near_Layer_5 = 8`
- const `_CharaPrio_Near_Layer_6 = 9`
- const `_CharaPrio_Near_Layer_7 = 10`
- const `_CharaPrio_Near_Layer_8 = 11`
- const `_CharaPrio_Near_Layer_9 = 12`
- const `_CharaPrio_Far_Layer_0 = 13`
- const `_CharaPrio_Far_Layer_1 = 14`
- const `_CharaPrio_Far_Layer_2 = 15`
- const `_CharaPrio_Far_Layer_3 = 16`
- const `_CharaPrio_Far_Layer_4 = 17`
- const `_CharaPrio_Far_Layer_5 = 18`
- const `_CharaPrio_Far_Layer_6 = 19`
- const `_CharaPrio_Far_Layer_7 = 20`
- const `_CharaPrio_Far_Layer_8 = 21`
- const `_CharaPrio_Far_Layer_9 = 22`
- const `_CharaPrio_Parent_P1 = 23`
- const `_CharaPrio_Parent_M1 = 24`
- const `_CharaPrio_Parent_BG = 25`
- const `_CharaPrio_Near_P1 = 26`
- const `_CharaPrio_GaugeCombo_P1 = 27`
- const `_CharaPrio_GaugeHP_P1 = 28`
- const `_CatchFlag_AtkStandGuard = 1`
- const `_CatchFlag_AtkAirGuard = 2`
- const `_CatchFlag_AtkCrouchGuard = 4`
- const `_CatchFlag_AtkGroundGuard = 5`
- const `_CatchFlag_AtkNoGuard = 8`
- const `_CatchFlag_AtkNoGuardThrow = 16`
- const `_CatchFlag_StateStand = 32`
- const `_CatchFlag_StateAir = 64`
- const `_CatchFlag_StateCrouch = 128`
- const `_CatchFlag_AtkAllGuard = 7`
- const `_CatchFlag_AtkStandShield = 256`
- const `_CatchFlag_AtkAirShield = 512`
- const `_CatchFlag_AtkCrouchShield = 1024`
- const `_CatchFlag_AtkGroundShield = 1280`
- const `_CatchFlag_Invalid_Through_ExceptBound = 0x10000`
- const `_CatchSuccess_HitSub = 1`
- const `_CatchSuccess_HitSub_Enemy = 2`
- const `_CatchSuccess_HitSub_None = 4`
- const `_CatchSuccess_FlagHit_Enemy = 8`
- const `_CatchSuccess_NoMuteki = 0x10000`
- const `_CharaMoveMode_Enable = 0`
- const `_CharaMoveMode_Limit = 1`
- const `_CharaMoveMode_Disable = 2`
- const `_PCGaugeType_Eltnum = 2`
- const `_MvStFlag_CounterAtk = 1`
- const `_MvStFlag_CounterDef = 2`
- const `_MvStFlag_DangerHPAtk = 8`
- const `_MvStFlag_DangerHPDef = 4`
- const `_HitCheckFlag_Head = 1`
- const `_HitCheckFlag_Body = 2`
- const `_HitCheckFlag_Legs = 4`
- const `_HitCheckFlag_FireBall = 8`
- const `_HitCheckFlag_Throw = 16`
- const `_CaptureHitFlag_Parent = 1`
- const `_PartnerFlag_ToParentHitStatus = 1`
- const `_PartnerFlag_ToParentHitStop = 2`
- const `_NetworkVsMode_VsRankMatch = 0`
- const `_NetworkVsMode_VsPlayerMatch = 1`
- const `_NetworkVsMode_RankMatch = 2`
- const `_NetworkVsMode_PlayerMatch = 3`
- const `_NetworkVsMode_None = -1`
- const `_NetworkVsControlType_Controller = 0`
- const `_NetworkVsControlType_NotController = 1`
- const `_NetworkVsControlType_Spectator = 2`
- const `_NetworkVsControlType_None = -1`
- const `_MoveCode0_NoMoveBasicAction = &unk_1000000`

## CCheckRect (7)

- `void Reset()` func native `SqratWrap_void__noargs` wrapper `SqratWrap_void__noargs`
- `void SetPlayer()` func native `SqratWrap_void__noargs` wrapper `SqratWrap_void__noargs`
- `void SetSrc(string)` func native `sub_445470` wrapper `sub_445470`
- `void SetDst(string)` func native `sub_445470` wrapper `sub_445470`
- `void SetFlags(int)` func native `SqratWrap_void__int_2` wrapper `SqratWrap_void__int_2`
- `int Check()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `instance GetCharaData()` func native `sub_4453B0` wrapper `sub_4453B0`

## ProcTimer (8)

- `void End()` func native `sub_446194` wrapper `SqratWrap_void__noargs`
- `void ReStart()` func native `sub_44618F` wrapper `SqratWrap_void__noargs`
- `void SetName(object)` func native `sub_445B20` wrapper `SqratWrap_void__object`
- `float GetTime()` func native `sub_445B90` wrapper `SqratWrap_float__noargs_2`
- `void PrintTime_Int()` func native `nullsub_55` wrapper `SqratWrap_void__noargs`
- `float GetNowTime()` func native `sub_446199` wrapper `SqratWrap_float__noargs_2`
- `void PrintNowTime_Int()` func native `nullsub_56` wrapper `SqratWrap_void__noargs`
- `void EndPrint_Off()` func native `sub_445CC0` wrapper `SqratWrap_void__noargs`

## BSound (9)

- `void SE_Play(string)` func native `sub_800E70` wrapper `SqratWrap_void__string`
- `void SE_Stop(string)` func native `sub_800CE0` wrapper `SqratWrap_void__string`
- `int SE_GetUserParam(string)` func native `sub_800B10` wrapper `SqratWrap_int__string`
- `int SE_GetBufferSize(string)` func native `sub_800AD0` wrapper `SqratWrap_int__string`
- `void BGM_Mute(int)` func native `nullsub_57` wrapper `SqratWrap_void__int`
- `void BGM_SetNum(int)` func native `sub_800AB0` wrapper `SqratWrap_void__int`
- `void BGM_SetFade(string)` func native `sub_800A70` wrapper `SqratWrap_void__string`
- `void SE_SetFade(string)` func native `sub_800A30` wrapper `SqratWrap_void__string`
- `void Voice_SetFade(string)` func native `sub_8009F0` wrapper `SqratWrap_void__string`

## BMvSoundConst (3)

- const `_SeType_All = -2`
- const `_SeType_Normal = -1`
- const `_SeType_Player = 0`

## CBtlInfo (8)

- `int CheckCharaAlive(int)` func native `SqratWrap_int__int_3` wrapper `SqratWrap_int__int_3`
- `int CheckCharaAlive_str(object)` func native `SqratWrap_int__object` wrapper `SqratWrap_int__object`
- `int IsSelectBgm()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int GetSelectBgm()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int IsBgmAuto()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int IsBgmMute()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int GetFixStageBGM()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void SetBattleBgm(int)` func native `SqratWrap_void__int_2` wrapper `SqratWrap_void__int_2`

## BMvTbl (177)

- `int GetPlayerSide()` func native `sub_80A110` wrapper `SqratWrap_int__noargs`
- `int GetPlayerNo()` func native `sub_80A0F0` wrapper `SqratWrap_int__noargs`
- `int IsActivePlayer(int)` func native `Sq_IsActivePlayer` wrapper `SqratWrap_int__int`
- `int SetPattern(string)` func native `Sq_SetPattern` wrapper `SqratWrap_int__string`
- `int SetPattern_Reserve(string)` func native `Sq_SetPatternReserve` wrapper `SqratWrap_int__string`
- `void SetMuki(int)` func native `sub_809F90` wrapper `SqratWrap_void__int`
- `int GetMuki()` func native `Sq_GetMuki` wrapper `SqratWrap_int__noargs`
- `int CheckFurimuki()` func native `sub_809F30` wrapper `SqratWrap_int__noargs`
- `int VGuard_CheckKeep()` func native `sub_809F10` wrapper `SqratWrap_int__noargs`
- `int VGuard_Time(int)` func native `sub_809ED0` wrapper `SqratWrap_int__int`
- `int CheckStickHold(int)` func native `sub_809E60` wrapper `SqratWrap_int__int`
- `int CheckButtonHold(int)` func native `sub_809D40` wrapper `SqratWrap_int__int`
- `int GetStickHold()` func native `sub_809E10` wrapper `SqratWrap_int__noargs`
- `int GetButtonHold()` func native `sub_809DC0` wrapper `SqratWrap_int__noargs`
- `int IsStartTrigger()` func native `sub_809CE0` wrapper `SqratWrap_int__noargs`
- `void SetNextMoveTable(object)` func native `sub_809C40` wrapper `SqratWrap_void__object_2`
- `void SetDeleteMoveTable()` func native `sub_809C30` wrapper `SqratWrap_void__noargs_2`
- `void SetFinalize(int)` func native `sub_809C00` wrapper `SqratWrap_void__int`
- `void SetFinalizeCode(int)` func native `sub_809BE0` wrapper `SqratWrap_void__int`
- `int GetFinalizeCode()` func native `sub_809BC0` wrapper `SqratWrap_int__noargs`
- `int FromFinalize()` func native `sub_809BA0` wrapper `SqratWrap_int__noargs`
- `void SetMoveCode(int)` func native `sub_809B40` wrapper `SqratWrap_void__int`
- `int GetMoveCode()` func native `sub_809B20` wrapper `SqratWrap_int__noargs`
- `void SetMoveCodeEx(int, int)` func native `sub_809B80` wrapper `SqratWrap_void__int_int`
- `int GetMoveCodeEx(int)` func native `sub_809B60` wrapper `SqratWrap_int__int`
- `int GetCmdParam(int)` func native `sub_809A00` wrapper `SqratWrap_int__int`
- `int GetCmdNumber()` func native `sub_8099E0` wrapper `SqratWrap_int__noargs`
- `int GetSkillLv()` func native `sub_809A20` wrapper `SqratWrap_int__noargs`
- `void SetSkillLv(int)` func native `sub_809A40` wrapper `SqratWrap_void__int`
- `void SetMvAction(int)` func native `sub_809B00` wrapper `SqratWrap_void__int`
- `int GetMvAction()` func native `sub_809AE0` wrapper `SqratWrap_int__noargs`
- `void SetSparkDisableAttack(int)` func native `sub_809AA0` wrapper `SqratWrap_void__int`
- `int GetSparkDisableAttack()` func native `sub_809AC0` wrapper `SqratWrap_int__noargs`
- `void SetSparkDisableMove(int)` func native `sub_809A60` wrapper `SqratWrap_void__int`
- `int GetSparkDisableMove()` func native `sub_809A80` wrapper `SqratWrap_int__noargs`
- `int ChangeMv_GetMoveCodeEx(int)` func native `sub_8099C0` wrapper `SqratWrap_int__int`
- `string ChangeMv_GetMvName()` func native `sub_809990` wrapper `SqratWrap_string__noargs`
- `int ChangeMv_GetMvAction()` func native `sub_809970` wrapper `SqratWrap_int__noargs`
- `int GetSP(int)` func native `sub_8098E0` wrapper `SqratWrap_int__int`
- `void SetSP(int, int)` func native `sub_809890` wrapper `SqratWrap_void__int_int`
- `int AddSP(int, int)` func native `sub_809840` wrapper `SqratWrap_int__int_int`
- `int GetPP(int)` func native `sub_8097F0` wrapper `SqratWrap_int__int`
- `void SetPP(int, int)` func native `sub_8097B0` wrapper `SqratWrap_void__int_int`
- `int AddPP(int, int)` func native `sub_809770` wrapper `SqratWrap_int__int_int`
- `int GetLP(int)` func native `sub_8096B0` wrapper `SqratWrap_int__int`
- `void SetLP(int, int)` func native `sub_809690` wrapper `SqratWrap_void__int_int`
- `int AddLP(int, int)` func native `sub_809660` wrapper `SqratWrap_int__int_int`
- `int GetLPEx(int, int)` func native `sub_809740` wrapper `SqratWrap_int__int_int`
- `void SetLPEx(int, int, int)` func native `sub_809710` wrapper `SqratWrap_void__int_int_int`
- `int AddLPEx(int, int, int)` func native `sub_8096D0` wrapper `SqratWrap_int__int_int_int`
- `void SetComboLimit(int)` func native `sub_809610` wrapper `SqratWrap_void__int`
- `void SetWallCount(int)` func native `sub_8095D0` wrapper `SqratWrap_void__int`
- `int AddAirCount(int, int)` func native `sub_809570` wrapper `SqratWrap_int__int_int`
- `void SetAirJumpOK(int)` func native `sub_809550` wrapper `SqratWrap_void__int`
- `int AddAirJumpCount(int)` func native `sub_809530` wrapper `SqratWrap_int__int`
- `int TobiParam_Func(string)` func native `sub_809450` wrapper `SqratWrap_int__string`
- `int TobiParamEndFlag_Func(string)` func native `sub_809370` wrapper `SqratWrap_int__string`
- `int CheckPosState(int)` func native `sub_809300` wrapper `SqratWrap_int__int`
- `int CheckPattern(string)` func native `sub_809010` wrapper `SqratWrap_int__string`
- `int GetFrameParam(int)` func native `sub_808FC0` wrapper `SqratWrap_int__int`
- `int GetFrameID()` func native `sub_808FF0` wrapper `SqratWrap_int__noargs`
- `int JumpFrameID(int)` func native `sub_808170` wrapper `SqratWrap_int__int`
- `int JumpFrameID_Reserve(int)` func native `sub_808140` wrapper `SqratWrap_int__int`
- `int Frame_Proc(int, int)` func native `sub_8080F0` wrapper `SqratWrap_int__int_int`
- `int CheckFrameFinalize(int)` func native `sub_8080A0` wrapper `SqratWrap_int__int`
- `void SetPrio(int)` func native `sub_808F90` wrapper `SqratWrap_void__int`
- `void SetSpPrio(string)` func native `sub_808F00` wrapper `SqratWrap_void__string`
- `void SetCommandLongDelay(int)` func native `SetCommandLongDelay` wrapper `SqratWrap_void__int`
- `void SetCommandShortDelay(int)` func native `SetCommandShortDelay` wrapper `SqratWrap_void__int`
- `int CheckCommand(string)` func native `sub_808AF0` wrapper `SqratWrap_int__string`
- `int CheckCommandString(string)` func native `sub_808890` wrapper `SqratWrap_int__string`
- `int CheckLastStick(string)` func native `sub_8084F0` wrapper `SqratWrap_int__string`
- `int ClearCommand()` func native `sub_808430` wrapper `SqratWrap_int__noargs`
- `int SetCommandFailTime(string)` func native `sub_8082B0` wrapper `SqratWrap_int__string`
- `int CheckCommandFailTime(string)` func native `sub_8081F0` wrapper `SqratWrap_int__string`
- `instance GetMvStatus()` func native `sub_80A430` wrapper `SqratWrap_instance__noargs_8`
- `int CheckMvUniqueID(string)` func native `sub_80A390` wrapper `SqratWrap_int__string`
- `string GetMvName()` func native `sub_80A350` wrapper `SqratWrap_string__noargs`
- `void SetMvHitStatus(instance)` func native `sub_80A200` wrapper `SqratWrap_void__instance_2`
- `instance GetMvHitStatus()` func native `sub_80A230` wrapper `SqratWrap_instance__noargs_9`
- `void ClearHitStatus()` func native `sub_80A1E0` wrapper `SqratWrap_void__noargs_2`
- `void SetHitStop(int, int)` func native `sub_80A1B0` wrapper `SqratWrap_void__int_int`
- `int GetHitStop()` func native `sub_80A170` wrapper `SqratWrap_int__noargs`
- `int SetVector(string)` func native `sub_80AE80` wrapper `SqratWrap_int__string`
- `instance GetVector(string)` func native `__std_fs_get_stats` wrapper `SqratWrap_instance__string_2`
- `int SetVector_CCharaVector(string)` func native `sub_80AE00` wrapper `SqratWrap_int__string_2`
- `int ClearTriVector(string)` func native `sub_80AD80` wrapper `SqratWrap_int__string`
- `void SetVector_MaxX(int)` func native `sub_80AE50` wrapper `SqratWrap_void__int`
- `value GetPosition(string)` func native `sub_80AB20` wrapper `SqratWrap_value__string_2`
- `int SetPosition(string)` func native `sub_80A8D0` wrapper `SqratWrap_int__string`
- `int MoveStartPosition()` func native `sub_80A890` wrapper `SqratWrap_int__noargs`
- `instance GetScreenPosition()` func native `sub_80AA70` wrapper `SqratWrap_instance__noargs_7`
- `int SetAngle(string)` func native `sub_80A6A0` wrapper `SqratWrap_int__string`
- `int GetAngle()` func native `sub_80A680` wrapper `SqratWrap_int__noargs`
- `void SetScale(string)` func native `sub_80A600` wrapper `SqratWrap_void__string`
- `instance GetScale()` func native `sub_80A5A0` wrapper `SqratWrap_instance__noargs_7`
- `int TalkScript_IsDone()` func native `sub_808070` wrapper `SqratWrap_int__noargs`
- `instance GetMvRoundStatus()` func native `sub_807DB0` wrapper `SqratWrap_instance__noargs_10`
- `instance GetMvStageStatus()` func native `sub_807F90` wrapper `SqratWrap_instance__noargs_11`
- `instance GetMvNetworkInfo()` func native `sub_807CD0` wrapper `SqratWrap_instance__noargs_12`
- `void SetRoundEnd()` func native `sub_807C60` wrapper `SqratWrap_void__noargs_2`
- `void SetRoundStart()` func native `sub_807C20` wrapper `SqratWrap_void__noargs_2`
- `void SetRoundStartType(int)` func native `sub_807BE0` wrapper `SqratWrap_void__int`
- `int SetMoveableFlag(string)` func native `sub_807960` wrapper `SqratWrap_int__string`
- `int SetMoveableFlagEx(string)` func native `sub_807890` wrapper `SqratWrap_int__string`
- `int SetAsFlag(string)` func native `sub_8076B0` wrapper `SqratWrap_int__string`
- `int SetAsStatusFlag(string)` func native `sub_8077D0` wrapper `SqratWrap_int__string`
- `int SetAsPosStatusFlag(string)` func native `sub_807740` wrapper `SqratWrap_int__string`
- `int SetAtkGuardFlag(string)` func native `sub_807620` wrapper `SqratWrap_int__string`
- `int GetAtkGuardFlag()` func native `sub_8075E0` wrapper `SqratWrap_int__noargs`
- `int SetChipDamStatus(string)` func native `sub_807530` wrapper `SqratWrap_int__string`
- `int SetAtkCatchFlag(string)` func native `sub_807420` wrapper `SqratWrap_int__string`
- `int SetSousaiFlag(string)` func native `sub_806B40` wrapper `SqratWrap_int__string`
- `int CalcHitValue(int)` func native `sub_8074F0` wrapper `SqratWrap_int__int`
- `instance GetRecoverStatus()` func native `sub_807CA0` wrapper `SqratWrap_instance__noargs_13`
- `int PcGauge_Set(string)` func native `sub_807280` wrapper `SqratWrap_int__string`
- `int BoundCorrect_Calc(string)` func native `sub_8071D0` wrapper `SqratWrap_int__string`
- `int ComboPoint_Calc(string)` func native `sub_807120` wrapper `SqratWrap_int__string`
- `int UseSkill_Check(string)` func native `sub_8070C0` wrapper `SqratWrap_int__string`
- `int HitPat_Check(string)` func native `sub_806FF0` wrapper `SqratWrap_int__string`
- `int SetHitCheckFlag(string)` func native `sub_806F40` wrapper `SqratWrap_int__string`
- `int GetHitCheckFlag(int)` func native `sub_806F00` wrapper `SqratWrap_int__int`
- `int GetHitCheckMvFlag()` func native `sub_806EE0` wrapper `SqratWrap_int__noargs`
- `int SetNoHoseiFlag(string)` func native `sub_806E40` wrapper `SqratWrap_int__string`
- `int SetNoUkemiTimeLimitFlag(string)` func native `sub_806DB0` wrapper `SqratWrap_int__string`
- `int SetSuperArmorFlag(string)` func native `sub_806D80` wrapper `SqratWrap_int__string`
- `int GetSuperArmorFlag()` func native `sub_806D50` wrapper `SqratWrap_int__noargs`
- `int GetSuperArmorTime()` func native `sub_806D30` wrapper `SqratWrap_int__noargs`
- `int SetVirtualGuardFlag(string)` func native `sub_806D00` wrapper `SqratWrap_int__string`
- `int SetKasanariValue(string)` func native `sub_806CD0` wrapper `SqratWrap_int__string`
- `int SetDrawAlpha(string)` func native `Sq_SetDrawAlpha` wrapper `SqratWrap_int__string`
- `int GetDrawAlpha(string)` func native `Sq_GetDrawAlpha` wrapper `SqratWrap_int__string`
- `int SetCaptureHitFlag(string)` func native `sub_806C40` wrapper `SqratWrap_int__string`
- `int GetNoHoseiHitFlag()` func native `sub_806DE0` wrapper `SqratWrap_int__noargs`
- `int SetNoHoseiHitFlag(string)` func native `sub_806E10` wrapper `SqratWrap_int__string`
- `int SetAliveFlag(string)` func native `sub_806C10` wrapper `SqratWrap_int__string`
- `int SetCounterHitFlag(string)` func native `sub_806BD0` wrapper `SqratWrap_int__string`
- `int SetPartnerFlag(string)` func native `sub_806BA0` wrapper `SqratWrap_int__string`
- `int SetForceUkemiTimeLimitFlag(string)` func native `sub_806B70` wrapper `SqratWrap_int__string`
- `int SetVsValue(string)` func native `sub_8068F0` wrapper `SqratWrap_int__string`
- `int GetVsValue_HP(int)` func native `sub_806920` wrapper `SqratWrap_int__int`
- `int SetVsAtkValue(string)` func native `sub_806A80` wrapper `SqratWrap_int__string`
- `int SetVsDefValue(string)` func native `sub_806A50` wrapper `SqratWrap_int__string`
- `int SetInvAtkFlag(string)` func native `sub_806A20` wrapper `SqratWrap_int__string`
- `int SetInvDefFlag(string)` func native `sub_8069F0` wrapper `SqratWrap_int__string`
- `int GetInvAtkFlag(int)` func native `sub_8069D0` wrapper `SqratWrap_int__int`
- `int GetInvDefFlag(int)` func native `sub_8069B0` wrapper `SqratWrap_int__int`
- `int SetHoseiMinValue(int)` func native `sub_8068A0` wrapper `SqratWrap_int__int`
- `int GetHoseiMinValue()` func native `sub_806860` wrapper `SqratWrap_int__noargs`
- `int SetGrdLimit(int)` func native `sub_806830` wrapper `SqratWrap_int__int`
- `void AddComboRate()` func native `sub_806780` wrapper `SqratWrap_void__noargs_2`
- `void SetComboRateValue(int)` func native `sub_806750` wrapper `SqratWrap_void__int`
- `int GetComboInfo(int)` func native `sub_806650` wrapper `SqratWrap_int__int`
- `void AddComboCount(int)` func native `sub_8066F0` wrapper `SqratWrap_void__int`
- `int SetHan6LayerMask(string)` func native `sub_806610` wrapper `SqratWrap_int__string`
- `int CheckBlast_IsNGStatus(string)` func native `sub_805E40` wrapper `SqratWrap_int__string`
- `int CheckBlast_BoundInit(string)` func native `sub_805D30` wrapper `SqratWrap_int__string`
- `int DamageFlag_Func(int, int)` func native `sub_805D00` wrapper `SqratWrap_int__int_int`
- `int DamageFlag_IsBound(string)` func native `sub_805CE0` wrapper `SqratWrap_int__string`
- `int GetSelectColor()` func native `sub_80A150` wrapper `SqratWrap_int__noargs`
- `int GetCharaNo()` func native `Sq_GetObjCharaNo` wrapper `SqratWrap_int__noargs`
- `void SetTutoMvFlag(int)` func native `sub_8065F0` wrapper `SqratWrap_void__int`
- `int GetTutoMvFlag()` func native `sub_8065D0` wrapper `SqratWrap_int__noargs`
- `void AirSkill_Begin()` func native `sub_8063F0` wrapper `SqratWrap_void__noargs_2`
- `void AirSkill_Check()` func native `sub_8063A0` wrapper `SqratWrap_void__noargs_2`
- `void TM_SetDownStatus()` func native `sub_806340` wrapper `SqratWrap_void__noargs_2`
- `int TM_GetCancelShieldTiming()` func native `TrainingMenu_CheckShieldCounterCondition` wrapper `SqratWrap_int__noargs`
- `int GetMvCancel(int)` func native `sub_806220` wrapper `SqratWrap_int__int`
- `int IsCpu()` func native `sub_8065B0` wrapper `SqratWrap_int__noargs`
- `int IsMissionSample()` func native `sub_806570` wrapper `SqratWrap_int__noargs`
- `int GetBoss()` func native `sub_806440` wrapper `SqratWrap_int__noargs`
- `int GetPositionBuffer(int)` func native `sub_806540` wrapper `SqratWrap_int__int`
- `int GetControlType()` func native `sub_806520` wrapper `SqratWrap_int__noargs`
- `int GetRapidBeatType(int)` func native `sub_8064C0` wrapper `SqratWrap_int__int`
- `void Achievement_Unlock(int)` func native `sub_805BF0` wrapper `SqratWrap_void__int`
- `void Achievement_Increment(int, int)` func native `sub_805BE0` wrapper `SqratWrap_void__int_int`
- `void BattleActivity_Increment(int)` func native `sub_805BD0` wrapper `SqratWrap_void__int`

## CCharaRect (4)

- var `sx` @0 (Sqrat_Class_VarF)
- var `sy` @4 (Sqrat_Class_VarF)
- var `ex` @8 (Sqrat_Class_VarF)
- var `ey` @12 (Sqrat_Class_VarF)

## CCharaPoint4 (8)

- var `x0` @0 (Sqrat_Class_VarG)
- var `y0` @4 (Sqrat_Class_VarG)
- var `x1` @8 (Sqrat_Class_VarG)
- var `y1` @12 (Sqrat_Class_VarG)
- var `x2` @16 (Sqrat_Class_VarG)
- var `y2` @20 (Sqrat_Class_VarG)
- var `x3` @24 (Sqrat_Class_VarG)
- var `y3` @28 (Sqrat_Class_VarG)

## MvStatus (33)

- var `CharaNo` @0 (Sqrat_Class_VarH)
- var `CallCount` @2 (Sqrat_Class_VarH)
- var `FrameCallCount` @4 (Sqrat_Class_VarH)
- var `isUpdate` @16 (Sqrat_Class_VarH)
- var `isFrameUpdate` @18 (Sqrat_Class_VarH)
- var `isLanding` @24 (Sqrat_Class_VarH)
- var `DataPattern` @20 (Sqrat_Class_VarH)
- var `DataFrame` @22 (Sqrat_Class_VarH)
- var `DataDelayVal` @26 (Sqrat_Class_VarH)
- var `DataDelay` @28 (Sqrat_Class_VarH)
- var `flags` @30 (Sqrat_Class_VarH)
- var `FrameID` @32 (Sqrat_Class_VarH)
- var `Param0` @34 (Sqrat_Class_VarH)
- var `Param1` @36 (Sqrat_Class_VarH)
- var `Param2` @38 (Sqrat_Class_VarH)
- var `Param3` @40 (Sqrat_Class_VarH)
- `int GetCharaNo()` func native `sub_446400` wrapper `SqratWrap_int__noargs_2`
- `int GetCallCount()` func native `sub_446410` wrapper `SqratWrap_int__noargs_2`
- `int GetMvCount()` func native `sub_429080` wrapper `SqratWrap_int__noargs_3`
- `int GetMvCountFrame()` func native `sub_4464C0` wrapper `SqratWrap_int__noargs_3`
- `int IsUpdate()` func native `sub_446430` wrapper `SqratWrap_int__noargs_2`
- `int IsFrameUpdate()` func native `sub_446440` wrapper `SqratWrap_int__noargs_2`
- `int GetDataPattern()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetDataFrame()` func native `sub_4464E0` wrapper `SqratWrap_int__noargs_2`
- `int GetFrameID()` func native `sub_446480` wrapper `SqratWrap_int__noargs_2`
- `int IsLanding()` func native `sub_4289D0` wrapper `SqratWrap_int__noargs_2`
- `int GetDataDelayVal()` func native `sub_4289C0` wrapper `SqratWrap_int__noargs_2`
- `int GetDataDelay()` func native `sub_446450` wrapper `SqratWrap_int__noargs_2`
- `int GetFlags()` func native `sub_446460` wrapper `SqratWrap_int__noargs_2`
- `int GetParam0()` func native `sub_446490` wrapper `SqratWrap_int__noargs_2`
- `int GetParam1()` func native `sub_428A10` wrapper `SqratWrap_int__noargs_2`
- `int GetParam2()` func native `sub_4464A0` wrapper `SqratWrap_int__noargs_2`
- `int GetParam3()` func native `sub_4464B0` wrapper `SqratWrap_int__noargs_2`

## MvHitStatus (8)

- var `Type` @0 (Sqrat_Class_VarI)
- var `PosState` @2 (Sqrat_Class_VarI)
- var `isDown` @4 (Sqrat_Class_VarI)
- var `isFirstUpdate` @8 (Sqrat_Class_VarI)
- var `isCatchFlag` @10 (Sqrat_Class_VarI)
- var `isMoveable` @12 (Sqrat_Class_VarI)
- var `Count` @6 (Sqrat_Class_VarI)
- `int IsDown()` func native `sub_446500` wrapper `SqratWrap_int__noargs_3`

## MvHitImpactInfo (8)

- var `type` @0 (Sqrat_Class_VarJ)
- var `d_state` @2 (Sqrat_Class_VarJ)
- var `atk_throw` @8 (Sqrat_Class_VarJ)
- var `def_guard` @10 (Sqrat_Class_VarJ)
- var `grd_break` @12 (Sqrat_Class_VarJ)
- var `armor_break` @14 (Sqrat_Class_VarJ)
- var `counterhit` @16 (Sqrat_Class_VarJ)
- var `vectortype` @18 (Sqrat_Class_VarJ)

## MvInterruptInfo (6)

- var `type` @0 (Sqrat_Class_VarK)
- var `Param0` @4 (Sqrat_Class_VarK)
- var `Param1` @8 (Sqrat_Class_VarK)
- var `Param2` @12 (Sqrat_Class_VarK)
- var `Param3` @16 (Sqrat_Class_VarK)
- var `Param4` @20 (Sqrat_Class_VarK)

## MvRoundStatus (11)

- var `Time` @12 (Sqrat_Class_VarL)
- var `TimeUpdate` @14 (Sqrat_Class_VarL)
- var `Round` @16 (Sqrat_Class_VarL)
- var `WinType` @18 (Sqrat_Class_VarL)
- var `isKo` @0 (Sqrat_Class_VarL)
- var `isMyKo` @2 (Sqrat_Class_VarL)
- var `isWinRound` @4 (Sqrat_Class_VarL)
- var `isLoseRound` @6 (Sqrat_Class_VarL)
- var `isExtraRound` @8 (Sqrat_Class_VarL)
- var `isFirstLoseRound` @10 (Sqrat_Class_VarL)
- var `CharaMoveMode` @20 (Sqrat_Class_VarL)

## MvStageStatus (5)

- `int IsStoryBattle()` func native `sub_808060` wrapper `SqratWrap_int__noargs_3`
- `int IsTrainingBattle()` func native `Sq_IsTrainingBattle` wrapper `SqratWrap_int__noargs_3`
- `int GetCpuChara()` func native `sub_808000` wrapper `SqratWrap_int__noargs_3`
- `int GetPcChara()` func native `sub_807FE0` wrapper `SqratWrap_int__noargs_3`
- `int GetStartSide()` func native `sub_4464F0` wrapper `SqratWrap_int__noargs_3`

## BtlMvStd (10)

- `instance GetBoundStatus()` func native `sub_80B860` wrapper `SqratWrap_instance__noargs_14`
- `int SetBoundStatus()` func native `sub_80B730` wrapper `SqratWrap_int__noargs`
- `int SetBoundFinalize()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int ClearBound()` func native `sub_80B620` wrapper `SqratWrap_int__noargs`
- `int SetGuardStatus()` func native `sub_80B5C0` wrapper `SqratWrap_int__noargs`
- `int SetGuardFinalize()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int ClearGuard()` func native `sub_80B460` wrapper `SqratWrap_int__noargs`
- `int IsFileAlive(object)` func native `sub_416010` wrapper `sub_416010`
- `int CheckHan6Data(string)` func native `sub_80B450` wrapper `SqratWrap_int__string`
- `instance GetBoundVectorStatus(int, int)` func native `sub_80B790` wrapper `SqratWrap_instance__int_int`

## MvBoundStatus (33)

- var `isDone` @0 (Sqrat_Class_VarC)
- var `isCapture` @2 (Sqrat_Class_VarC)
- var `isBound` @4 (Sqrat_Class_VarC)
- var `Num` @10 (Sqrat_Class_VarC)
- var `VecCount` @12 (Sqrat_Class_VarC)
- var `WallCount` @18 (Sqrat_Class_VarC)
- var `Power` @24 (Sqrat_Class_VarC)
- var `AirUkemi` @16 (Sqrat_Class_VarC)
- var `VecMuki` @14 (Sqrat_Class_VarC)
- var `BoundTime` @26 (Sqrat_Class_VarC)
- var `BoundTimeAll` @28 (Sqrat_Class_VarC)
- var `BoundFlag` @30 (Sqrat_Class_VarC)
- `int IsDone()` func native `sub_446400` wrapper `SqratWrap_int__noargs_2`
- `int IsCapture()` func native `sub_446410` wrapper `SqratWrap_int__noargs_2`
- `int IsBound()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetNum()` func native `sub_428980` wrapper `SqratWrap_int__noargs_2`
- `int GetVecCount()` func native `sub_428990` wrapper `SqratWrap_int__noargs_2`
- `int GetVecMuki()` func native `sub_446420` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemi()` func native `sub_446430` wrapper `SqratWrap_int__noargs_2`
- `int GetWallCount()` func native `sub_446440` wrapper `SqratWrap_int__noargs_2`
- `string GetName()` func native `sub_4289B0` wrapper `SqratWrap_string__noargs_2`
- `int GetPower()` func native `sub_4289D0` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundTime()` func native `sub_4289C0` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundTimeAll()` func native `sub_446450` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundFlag()` func native `sub_446460` wrapper `SqratWrap_int__noargs_2`
- `int GetCountMax()` func native `sub_446480` wrapper `SqratWrap_int__noargs_2`
- `int GetCountMaxTrue()` func native `sub_446490` wrapper `SqratWrap_int__noargs_2`
- `int GetCountNow()` func native `sub_428A10` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemiTimeNow()` func native `sub_4464A0` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemiTimeMax()` func native `sub_4464B0` wrapper `SqratWrap_int__noargs_2`
- `int GetVecX()` func native `sub_440170` wrapper `SqratWrap_int__noargs_3`
- `int GetVecY()` func native `sub_440190` wrapper `SqratWrap_int__noargs_3`
- `int IsDown()` func native `sub_446470` wrapper `SqratWrap_int__noargs_3`

## MvBoundVectorStatus (5)

- `int IsDone()` func native `sub_446400` wrapper `SqratWrap_int__noargs_2`
- `int GetVectime()` func native `sub_446410` wrapper `SqratWrap_int__noargs_2`
- `int GetUkemifailedtime()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetUkeminum_g()` func native `sub_428960` wrapper `SqratWrap_int__noargs_2`
- `int GetPower()` func native `sub_428970` wrapper `SqratWrap_int__noargs_2`

## debuglog (2)

- `void write(object)` func native `nullsub_63` wrapper `SqratWrap_void__object_2`
- `void write_r(object)` func native `nullsub_62` wrapper `SqratWrap_void__object_2`

## _LogsFile (6)

- `void opennew(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void write(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void write_r(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void writeD(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void writeD_r(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void close()` func native `_guard_check_icall_nop` wrapper `SqratWrap_void__noargs`

## debugfont (3)

- `void setprio(int)` func native `sub_811DA0` wrapper `SqratWrap_void__int`
- `void setcolor(int)` func native `sub_811D90` wrapper `SqratWrap_void__int`
- `void draw(int, int, object)` func native `sub_811D50` wrapper `SqratWrap_void__int_int_object`

## debugfontL (4)

- `void setprio(int)` func native `nullsub_61` wrapper `SqratWrap_void__int`
- `void setcolor(int)` func native `nullsub_60` wrapper `SqratWrap_void__int`
- `void draw(int, int, object)` func native `nullsub_59` wrapper `SqratWrap_void__int_int_object`
- `void draw_message(object)` func native `nullsub_58` wrapper `SqratWrap_void__object_2`

## BCMDTbl (29)

- `int GetHP()` func native `sub_811CF0` wrapper `SqratWrap_int__noargs`
- `int GetHPRatio()` func native `sub_811CA0` wrapper `SqratWrap_int__noargs`
- `int GetComboGauge()` func native `sub_811C60` wrapper `SqratWrap_int__noargs`
- `int GetComboGaugeEx()` func native `sub_811C00` wrapper `SqratWrap_int__noargs`
- `int CheckPosState(int)` func native `sub_811B40` wrapper `SqratWrap_int__int`
- `int IsMoveAble()` func native `sub_811BD0` wrapper `SqratWrap_int__noargs`
- `int IsAddCommand()` func native `sub_811B20` wrapper `SqratWrap_int__noargs`
- `int CheckPattern(string)` func native `sub_8118E0` wrapper `SqratWrap_int__string`
- `int GetFrameID()` func native `sub_8118C0` wrapper `SqratWrap_int__noargs`
- `int CheckFireballLimit(int)` func native `sub_811590` wrapper `SqratWrap_int__int`
- `int CheckState(int)` func native `sub_811460` wrapper `SqratWrap_int__int`
- `int CheckCancelFlag(int)` func native `sub_8114D0` wrapper `SqratWrap_int__int`
- `int CheckCancel(int)` func native `sub_8114B0` wrapper `SqratWrap_int__int`
- `int CheckDash(int, int)` func native `sub_8113F0` wrapper `SqratWrap_int__int_int`
- `int CheckAirJump(int)` func native `sub_8113C0` wrapper `SqratWrap_int__int`
- `int ResetAirJump()` func native `sub_8113A0` wrapper `SqratWrap_int__noargs`
- `int SetAirJump(int)` func native `sub_811370` wrapper `SqratWrap_int__int`
- `int CheckComboLimit(int)` func native `sub_811510` wrapper `SqratWrap_int__int`
- `int GetComboLimitLevel()` func native `sub_8114F0` wrapper `SqratWrap_int__noargs`
- `int CheckUserParam(int)` func native `sub_811570` wrapper `SqratWrap_int__int`
- `int CheckSeedCost(int)` func native `sub_811BF0` wrapper `SqratWrap_int__int`
- `int CheckMoveName(string)` func native `sub_8115C0` wrapper `SqratWrap_int__string`
- `int SetSpUseBufferTmp(string)` func native `sub_8112F0` wrapper `SqratWrap_int__string`
- `int GetSpCommandFlag(int)` func native `sub_8112A0` wrapper `SqratWrap_int__int`
- `int IsDirectCommand()` func native `sub_811290` wrapper `SqratWrap_int__noargs`
- `instance GetCmdInfo()` func native `sub_811260` wrapper `SqratWrap_instance__noargs_15`
- `instance GetCmdInfoEx(int)` func native `sub_811230` wrapper `SqratWrap_instance__int_3`
- `int GetSuccessButton()` func native `sub_811220` wrapper `SqratWrap_int__noargs`
- `int GetSuccessButtonPos()` func native `sub_811210` wrapper `SqratWrap_int__noargs`

## CmdInfo (5)

- `int GetLastButton(int)` func native `sub_44A440` wrapper `SqratWrap_int__int_3`
- `int GetCancel(int)` func native `sub_56FC10` wrapper `SqratWrap_int__int_3`
- `int GetCancelFlag(int)` func native `sub_44A460` wrapper `SqratWrap_int__int_3`
- `int GetFrame()` func native `sub_442910` wrapper `SqratWrap_int__noargs_3`
- `int GetNum(int)` func native `sub_44A430` wrapper `SqratWrap_int__int_3`

## BCMDConst (15)

- const `_PosState_Stand = 1`
- const `_PosState_Air = 2`
- const `_PosState_Crouch = 4`
- const `_PosState_Ground = 5`
- const `_PosState_Always = 7`
- const `_SkillType_None = 0`
- const `_SkillType_Normal = 1`
- const `_SkillType_Special = 2`
- const `_SkillType_ExSpecial = 6`
- const `_SkillType_Ex = 4`
- const `_SkillType_ChainShift = 8`
- const `_State_Guard = 1`
- const `_State_GuardCancel = 2`
- const `_SpCommandFlag_2AB = 0`
- const `_SpCommandFlag_BC = 1`

## CDat (2)

- `int Load(object)` func native `sub_5CB420` wrapper `SqratWrap_int__object`
- `void SetPosition(int, int)` func native `sub_5CB370` wrapper `SqratWrap_void__int_int_2`

## CAnime (19)

- `void SetCDat(instance)` func native `unknown_libname_5` wrapper `SqratWrap_void__instance_3`
- `void AddFrame(object, float)` func native `sub_5CD6A0` wrapper `SqratWrap_void__object_float`
- `void AddFrameEx(object, float, int)` func native `sub_5CC6C0` wrapper `SqratWrap_void__object_float_int`
- `void AddJump(int)` func native `sub_5CD680` wrapper `SqratWrap_void__int_2`
- `void AddModeJump(int, int, int)` func native `sub_5CC510` wrapper `SqratWrap_void__int_int_int_2`
- `void AddTagJump(object)` func native `sub_5CC970` wrapper `SqratWrap_void__object`
- `void AddModeTagJump(object, int, int)` func native `sub_5CC410` wrapper `SqratWrap_void__object_int_int`
- `void AddTag(object)` func native `sub_5CC880` wrapper `SqratWrap_void__object`
- `void AddSe(int)` func native `sub_5CD100` wrapper `SqratWrap_void__int_2`
- `void AddModeTag(int)` func native `sub_5CCAD0` wrapper `SqratWrap_void__int_2`
- `void SetPrio(int)` func native `sub_41C9B0` wrapper `SqratWrap_void__int_2`
- `void SetMode(int)` func native `sub_434A00` wrapper `SqratWrap_void__int_2`
- `int GetMode()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void AddMode(int)` func native `sub_44C1F0` wrapper `SqratWrap_void__int_2`
- `int IsNull()` func native `sub_4426C0` wrapper `SqratWrap_int__noargs_3`
- `void SetView(int)` func native `sub_41D460` wrapper `SqratWrap_void__int_2`
- `int GetView()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void ClearFrame()` func native `sub_5CD620` wrapper `SqratWrap_void__noargs`
- `void SetStartWait(float)` func native `sub_41C9C0` wrapper `SqratWrap_void__float`

## CAniManager (7)

- `instance Add(instance, object)` func native `__std_stacktrace_to_string` wrapper `SqratWrap_instance__instance_object`
- `int Render()` func native `sub_5CD0B0` wrapper `SqratWrap_int__noargs_3`
- `int Update()` func native `sub_5CD3F0` wrapper `SqratWrap_int__noargs_3`
- `int Reset()` func native `sub_5CC840` wrapper `SqratWrap_int__noargs_3`
- `void Finalize()` func native `sub_5CD440` wrapper `SqratWrap_void__noargs`
- `void SetMode(int)` func native `sub_44C210` wrapper `SqratWrap_void__int_2`
- `instance GetCAnime(object)` func native `sub_44C260` wrapper `SqratWrap_instance__object`

## tPAniConst (11)

- const `_PAniFlag_JpEqual = 1`
- const `_PAniFlag_JpGrater = 2`
- const `_PAniFlag_JpLess = 3`
- const `_PAniFrame_IpNormal = 0`
- const `_PAniFrame_IpAccel = 1`
- const `_PAniFrame_IpDecel = 2`
- const `_PAniFrame_IpAccelEx = 3`
- const `_PAniFrame_IpDecelEx = 4`
- const `_PAniFrame_IpAtoD = 5`
- const `_PAniFrame_IpDtoA = 6`
- const `_PAniFrame_IpNone = 7`

## TalkSqCallback (2)

- `void UseFileInSquirrel(string)` func native `sub_844070` wrapper `SqratWrap_void__string`
- `void UseFileInSquirrel(string)` func native `SqratWrap_void__string` wrapper `SqratWrap_void__string`

