# Native Squirrel API (generated from the IDB, do not edit)

Source: `uni2_raw.json`. Counts: const 525, func 674, method 150, table 65, var 60.

Types are inferred from the Sqrat wrapper (`sq_get*`/`sq_push*` calls); `value` = pushes a non-primitive, `string` may also accept an int via Sqrat::Object.

## (root/unbound:CSquirrel_ctor) (2)

- `value SQ_LoadString()` func native `sub_418EE0` wrapper `sub_418EE0`
- `int __dofile__(object)` func native `SqratWrap_int__object` wrapper `SqratWrap_int__object`

## SeFunc (2)

- `int GetParam(int)` func native `sub_4D8650` wrapper `SqratWrap_int__int`
- `void SetParam(int, int)` func native `sub_4D8660` wrapper `SqratWrap_void__int_int`

## BCom (7)

- `int Move_Jump(string)` func native `sub_56F980` wrapper `SqratWrap_int__string`
- `int Move_Next()` func native `sub_56F960` wrapper `SqratWrap_int__noargs`
- `int Move_End(string)` func native `sub_56F860` wrapper `SqratWrap_int__string`
- `string GetMoveTag()` func native `sub_56F820` wrapper `SqratWrap_string__noargs`
- `int SetReversalID(int)` func native `sub_56F7E0` wrapper `SqratWrap_int__int`
- `int Tutorial_GetSkillPos()` func native `sub_56F7D0` wrapper `SqratWrap_int__noargs`
- `instance GetStatus()` func native `sub_56F780` wrapper `SqratWrap_instance__noargs`

## UpdateSt (18)

- `int GetLevel()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetSkill()` func native `sub_4300D0` wrapper `SqratWrap_int__noargs_2`
- `int GetMove()` func native `sub_4300E0` wrapper `SqratWrap_int__noargs_2`
- `int GetSkillTime()` func native `sub_4300F0` wrapper `SqratWrap_int__noargs_2`
- `int GetMoveTime()` func native `sub_430100` wrapper `SqratWrap_int__noargs_2`
- `int GetELenX()` func native `sub_421510` wrapper `SqratWrap_int__noargs_3`
- `int GetELenY()` func native `sub_430110` wrapper `SqratWrap_int__noargs_3`
- `int GetHPP()` func native `sub_430120` wrapper `SqratWrap_int__noargs_2`
- `int GetEXS()` func native `sub_430130` wrapper `SqratWrap_int__noargs_2`
- `int IsPMoveable()` func native `sub_42F900` wrapper `SqratWrap_int__noargs_2`
- `int IsPBound()` func native `sub_430140` wrapper `SqratWrap_int__noargs_2`
- `int CheckPosState(int)` func native `sub_42F910` wrapper `SqratWrap_int__int_2`
- `int CheckPosState_Enemy(int)` func native `sub_42F930` wrapper `SqratWrap_int__int_2`
- `int CheckPState(int)` func native `sub_42F950` wrapper `SqratWrap_int__int_2`
- `int CheckEState(int)` func native `sub_42F970` wrapper `SqratWrap_int__int_2`
- `int GetTobiTime()` func native `sub_430170` wrapper `SqratWrap_int__noargs_2`
- `int GetComJumpLen()` func native `sub_430180` wrapper `SqratWrap_int__noargs_3`
- `int GetPlayerMuteki(int)` func native `sub_430190` wrapper `SqratWrap_int__int_2`

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
- const `_eComMoveFlag_EnemyAirNext = &loc_7FFFFF + 1`
- const `_eComMoveFlag_EnemyGroundNext = &unk_1000000`
- const `_eComMoveFlag_EnemyAirEnd = &unk_2000000`
- const `_eComMoveFlag_EnemyGroundEnd = &unk_4000000`
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
- const `_eCMF_EnemyAirNext = &loc_7FFFFF + 1`
- const `_eCMF_EnemyGroundNext = &unk_1000000`
- const `_eCMF_EnemyAirEnd = &unk_2000000`
- const `_eCMF_EnemyGroundEnd = &unk_4000000`
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

## BtlTrainingMenu (2)

- `int CheckTrainingDummyReplayMoveEnemy()` func native `sub_5A8650` wrapper `SqratWrap_int__noargs`
- `int CheckTrainingDummyReplayMoveEnemy()` func native `sub_5A8650` wrapper `SqratWrap_int__noargs`

## tSceneOpenSecChar (1)

- `instance GetStatus()` func native `sub_7F1E60` wrapper `SqratWrap_instance__noargs_2`

## sqfunc (1)

- `string GetExStringText(object)` func native `sub_82C950` wrapper `SqratWrap_string__object`

## tSceneVS (1)

- `instance GetStatus()` func native `sub_8446D0` wrapper `SqratWrap_instance__noargs_3`

## CStatus (31)

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
- var `Rankchange` @112 (Sqrat_Class_VarA)
- var `RankBefore` @116 (Sqrat_Class_VarA)
- var `RankAfter` @120 (Sqrat_Class_VarA)
- var `RankStageBefore` @124 (Sqrat_Class_VarA)
- var `RankStageAfter` @128 (Sqrat_Class_VarA)
- var `RipBefore0` @140 (Sqrat_Class_VarA)
- var `RipBefore1` @144 (Sqrat_Class_VarA)
- var `RipAfter0` @148 (Sqrat_Class_VarA)
- var `RipAfter1` @152 (Sqrat_Class_VarA)
- var `NetworkBattleMode` @156 (Sqrat_Class_VarA)

## tSceneWinDemo (1)

- `instance GetStatus()` func native `sub_84CF10` wrapper `SqratWrap_instance__noargs_4`

## eNetworkBattleModeConst (5)

- const `_eNetworkBattleMode_None = -1`
- const `_eNetworkBattleMode_Offline = 0`
- const `_eNetworkBattleMode_CasualMatch = 1`
- const `_eNetworkBattleMode_RankMatch = 2`
- const `_eNetworkBattleMode_PlayerMatch = 3`

## tWinMessageTableCSVCallback (1)

- `instance GetWinMesTableLotCallbackInfo()` func native `sub_85DCD0` wrapper `SqratWrap_instance__noargs_5`

## WinMesTableLotCallbackInfo (12)

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

## WinMesTableConst (15)

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
- const `_defBtlFinish_Special = 2`
- const `_defBtlFinish_ExSpecial = 3`

## BMvCore (23)

- `instance GetMyCharaData()` func native `sub_881BF0` wrapper `SqratWrap_instance__noargs_6`
- `instance GetCaptureCharaData()` func native `sub_881B80` wrapper `SqratWrap_instance__noargs_6`
- `instance GetParentCharaData()` func native `sub_881B30` wrapper `SqratWrap_instance__noargs_6`
- `instance GetPlayerCharaData()` func native `sub_881AD0` wrapper `SqratWrap_instance__noargs_6`
- `instance GetNearEnemyCharaData()` func native `sub_881A70` wrapper `SqratWrap_instance__noargs_6`
- `value GetEnemyCharaData(string)` func native `sub_8819D0` wrapper `SqratWrap_value__string`
- `instance GetLastDamageCharaData(int)` func native `sub_881910` wrapper `SqratWrap_instance__int`
- `void SetLastDamageCharaData(instance)` func native `sub_881970` wrapper `SqratWrap_void__instance`
- `instance GetLastHitCharaData(int)` func native `sub_8818B0` wrapper `SqratWrap_instance__int`
- `instance GetLastCatchCharaData(int)` func native `sub_881850` wrapper `SqratWrap_instance__int`
- `int CallEntryBCCachePreTransfer(string)` func native `sub_881390` wrapper `SqratWrap_int__string`
- `int Hiskoha_CallSwitch()` func native `sub_881770` wrapper `SqratWrap_int__noargs`
- `int GetTagStatus()` func native `sub_881710` wrapper `SqratWrap_int__noargs`
- `int GetCharaNo()` func native `sub_8816D0` wrapper `SqratWrap_int__noargs`
- `int GetBossFlag()` func native `sub_881690` wrapper `SqratWrap_int__noargs`
- `int GetBossRushFlag()` func native `sub_881660` wrapper `SqratWrap_int__noargs`
- `int VsScreen_GetCharaNo()` func native `sub_881650` wrapper `SqratWrap_int__noargs`
- `instance GetActiveCharaData()` func native `sub_8815C0` wrapper `SqratWrap_instance__noargs_6`
- `instance GetPertnerCharaData()` func native `sub_881520` wrapper `SqratWrap_instance__noargs_6`
- `int PushCharaData(instance)` func native `sub_881470` wrapper `SqratWrap_int__instance`
- `int PopCharaData()` func native `sub_881460` wrapper `SqratWrap_int__noargs`
- `instance GetParamCharaData(int)` func native `sub_8814F0` wrapper `SqratWrap_instance__int`
- `int IsHan6Squirrel()` func native `sub_881400` wrapper `SqratWrap_int__noargs`

## CCharaData (4)

- `int push()` func native `sub_881440` wrapper `SqratWrap_int__noargs_3`
- `int pop()` func native `sub_881430` wrapper `SqratWrap_int__noargs_3`
- `int isdone()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int isPlayer()` func native `sub_881410` wrapper `SqratWrap_int__noargs_3`

## CFindObject (2)

- `void SetFirst()` func native `sub_44A440` wrapper `SqratWrap_void__noargs`
- `instance Get(int)` func native `sub_44A460` wrapper `SqratWrap_instance__int_2`

## tDDC (69)

- `int Kirifuda_Add(int)` func native `sub_882C60` wrapper `SqratWrap_int__int`
- `int Kirifuda_GetValue()` func native `sub_882BB0` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_GetMax()` func native `sub_882BA0` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_TimeAdd(int, int)` func native `sub_882C40` wrapper `SqratWrap_int__int_int`
- `int Kirifuda_CheckTime()` func native `sub_882C10` wrapper `SqratWrap_int__noargs`
- `int Kirifuda_CheckStatus()` func native `sub_882C00` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_SetTimeValue(int, int)` func native `nullsub_66` wrapper `SqratWrap_void__int_int`
- `int Kirifuda_GetTime()` func native `sub_882BE0` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_SetInstallCount(int)` func native `nullsub_65` wrapper `SqratWrap_void__int`
- `void Kirifuda_TimeDamageMinus(int)` func native `nullsub_64` wrapper `SqratWrap_void__int`
- `int Kirifuda_FinalizeProc()` func native `sub_882C30` wrapper `SqratWrap_int__noargs`
- `void Kirifuda_TimeClear()` func native `nullsub_67` wrapper `SqratWrap_void__noargs_2`
- `int Kirifuda_SubMax()` func native `sub_882C50` wrapper `SqratWrap_int__noargs`
- `void KirifudaWatch_SetEndPalette(int, int)` func native `nullsub_63` wrapper `SqratWrap_void__int_int`
- `void KirifudaWatch_SetParam(int, int)` func native `nullsub_62` wrapper `SqratWrap_void__int_int`
- `int KirifudaWatch_GetParam(int)` func native `sub_882B70` wrapper `SqratWrap_int__int`
- `int Blast_AddPoint(int)` func native `sub_882B60` wrapper `SqratWrap_int__int`
- `int Blast_IsUse()` func native `sub_882B50` wrapper `SqratWrap_int__noargs`
- `int Blast_Use(int)` func native `sub_882B40` wrapper `SqratWrap_int__int`
- `int Blast_SetParam(string)` func native `sub_882AF0` wrapper `SqratWrap_int__string`
- `int Blast_SetFailed(int)` func native `sub_882AE0` wrapper `SqratWrap_int__int`
- `int Blast_GetUseMode()` func native `sub_882B30` wrapper `SqratWrap_int__noargs`
- `int Blast_CheckClimaxMode()` func native `sub_882B20` wrapper `SqratWrap_int__noargs`
- `int Blast_EndClimaxMode()` func native `sub_882B00` wrapper `SqratWrap_int__noargs`
- `int Blast_SetClimaxTime(int, int)` func native `sub_882B10` wrapper `SqratWrap_int__int_int`
- `int Heat_CheckMode()` func native `sub_882AD0` wrapper `SqratWrap_int__noargs`
- `int Heat_GetTimeCost()` func native `sub_882AC0` wrapper `SqratWrap_int__noargs`
- `int Heat_UseGauge()` func native `sub_882AC0` wrapper `SqratWrap_int__noargs`
- `int Moon_GetType()` func native `sub_882AB0` wrapper `SqratWrap_int__noargs`
- `int Moon_SetType(int)` func native `sub_882AA0` wrapper `SqratWrap_int__int`
- `int Moon_GetValue()` func native `sub_882A90` wrapper `SqratWrap_int__noargs`
- `int Moon_SetValue(int)` func native `sub_882A80` wrapper `SqratWrap_int__int`
- `int Moon_AddValue(int)` func native `sub_882A60` wrapper `SqratWrap_int__int`
- `int Moon_ChangeMoonDriveTimeSpeed(int)` func native `sub_882A70` wrapper `SqratWrap_int__int`
- `int Moon_MoonDriveStart(string)` func native `sub_882A50` wrapper `SqratWrap_int__string`
- `int Moon_MoonDriveEnd()` func native `sub_882A40` wrapper `SqratWrap_int__noargs`
- `int Moon_MoonDriveIsInfinite()` func native `sub_882A30` wrapper `SqratWrap_int__noargs`
- `int ReverseCorrect_Set(string)` func native `sub_882980` wrapper `SqratWrap_int__string`
- `int Support_IsUse()` func native `sub_882970` wrapper `SqratWrap_int__noargs`
- `int Support_Use()` func native `sub_882960` wrapper `SqratWrap_int__noargs`
- `int Support_SetFailed(int)` func native `sub_882950` wrapper `SqratWrap_int__int`
- `int Support_StartRecast(int)` func native `sub_882940` wrapper `SqratWrap_int__int`
- `void Support_SetPowerupRecastMax(int)` func native `nullsub_61` wrapper `SqratWrap_void__int`
- `int Support_GetCallCount()` func native `sub_882920` wrapper `SqratWrap_int__noargs`
- `int Support_SetRecastCost(int)` func native `sub_882910` wrapper `SqratWrap_int__int`
- `int Support_GetRecastCost()` func native `sub_882900` wrapper `SqratWrap_int__noargs`
- `int Support_GetType()` func native `sub_8828D0` wrapper `SqratWrap_int__noargs`
- `int Support_GetCharaNo()` func native `sub_8828E0` wrapper `SqratWrap_int__noargs`
- `int Support_GetSelectColor()` func native `sub_8828F0` wrapper `SqratWrap_int__noargs`
- `int Support_SetMessage(object, int, int)` func native `sub_8828C0` wrapper `SqratWrap_int__object_int_int`
- `void ExJem_Attack(int, int)` func native `nullsub_60` wrapper `SqratWrap_void__int_int`
- `void Jem_AddCount(int)` func native `nullsub_59` wrapper `SqratWrap_void__int`
- `int Jem_GetRunFlag(int)` func native `sub_882890` wrapper `SqratWrap_int__int`
- `int Jem_IsWorking(int)` func native `sub_882880` wrapper `SqratWrap_int__int`
- `int Jem_IsWorkingEff(int)` func native `sub_882870` wrapper `SqratWrap_int__int`
- `int Jem_SetMessage(object, int)` func native `sub_882850` wrapper `SqratWrap_int__object_int`
- `int Jem_SetMessageEx(int, int, object, int)` func native `sub_882860` wrapper `SqratWrap_int__int_int_object_int`
- `int HP_Add(string)` func native `sub_8827B0` wrapper `SqratWrap_int__string`
- `int HP_SourceDamMode(string)` func native `sub_882700` wrapper `SqratWrap_int__string`
- `void SkillCountPlus(int)` func native `nullsub_58` wrapper `SqratWrap_void__int`
- `void AchievementSousai()` func native `nullsub_57` wrapper `SqratWrap_void__noargs_2`
- `int PowerUpSys_GetCount(int)` func native `sub_8826A0` wrapper `SqratWrap_int__int`
- `int PowerUpSys_GetLast()` func native `sub_8826C0` wrapper `SqratWrap_int__noargs`
- `int PowerUpSys_IsDouble(int)` func native `sub_8826B0` wrapper `SqratWrap_int__int`
- `int SubSp_GetChara()` func native `sub_882690` wrapper `SqratWrap_int__noargs`
- `int SubSp_GetType()` func native `sub_882680` wrapper `SqratWrap_int__noargs`
- `int IsSingleMode()` func native `sub_882640` wrapper `SqratWrap_int__noargs`
- `int IsNotPlayMode()` func native `sub_882610` wrapper `SqratWrap_int__noargs`
- `int IsNotEnemySide()` func native `sub_8825C0` wrapper `SqratWrap_int__noargs`

## BMvEff (164)

- `int Random()` func native `sub_8893D0` wrapper `SqratWrap_int__noargs`
- `float Random_F()` func native `sub_889350` wrapper `SqratWrap_float__noargs`
- `int Random_Limit(int)` func native `sub_8892D0` wrapper `SqratWrap_int__int`
- `float Random_FAbs(int)` func native `sub_889240` wrapper `SqratWrap_float__int`
- `value Random_PointRect(string)` func native `sub_888F00` wrapper `SqratWrap_value__string_2`
- `value Random_PointRad(string)` func native `sub_889130` wrapper `SqratWrap_value__string_2`
- `int StdCall(string)` func native `sub_88D5D0` wrapper `SqratWrap_int__string`
- `int ThrowRelease(string)` func native `sub_88CAC0` wrapper `SqratWrap_int__string`
- `int ThrowParam(string)` func native `sub_88D010` wrapper `SqratWrap_int__string`
- `int ThrowChara_SetJoint(int)` func native `sub_88CF70` wrapper `SqratWrap_int__int`
- `int ThrowChara_SetCamera(int)` func native `sub_88CF00` wrapper `SqratWrap_int__int`
- `int ThrowChara_Transfer(string)` func native `sub_88CC50` wrapper `SqratWrap_int__string`
- `int CapturePlayer(string)` func native `sub_88C980` wrapper `SqratWrap_int__string`
- `int SetHitLimitCancel(int)` func native `sub_88C940` wrapper `SqratWrap_int__int`
- `int GetFrameIDNum(int)` func native `sub_88B0D0` wrapper `SqratWrap_int__int`
- `int GetFrameIDNum_Pat(int, int)` func native `sub_88B090` wrapper `SqratWrap_int__int_int`
- `int GetPatternNum(string)` func native `sub_88B100` wrapper `SqratWrap_int__string`
- `value CreateObject(string)` func native `sub_88B1E0` wrapper `SqratWrap_value__string`
- `int SetObjectFlags(string)` func native `sub_88B020` wrapper `SqratWrap_int__string`
- `int CheckObjectFlags(string)` func native `sub_88AFC0` wrapper `SqratWrap_int__string`
- `void SetObjectRender(string)` func native `sub_88AAE0` wrapper `SqratWrap_void__string`
- `int EraseObjectFlags(string)` func native `sub_88AF40` wrapper `SqratWrap_int__string`
- `int ObjProcFlags_Set(string)` func native `sub_88AED0` wrapper `SqratWrap_int__string`
- `int ObjProcFlags_Erase(string)` func native `sub_88AE60` wrapper `SqratWrap_int__string`
- `int ObjType_Set(string)` func native `sub_88ADF0` wrapper `SqratWrap_int__string`
- `int SetCamera_Quake(string)` func native `sub_88A960` wrapper `SqratWrap_int__string`
- `int SetCamera_Focus(string)` func native `sub_88A6F0` wrapper `SqratWrap_int__string`
- `void SetCamera_FocusKeep(int)` func native `sub_88A6C0` wrapper `SqratWrap_void__int`
- `void SetCamera_Clipping(int)` func native `sub_88A6A0` wrapper `SqratWrap_void__int`
- `int GetCamera_Clipping()` func native `sub_88A690` wrapper `SqratWrap_int__noargs`
- `void SetCamera_UpdateFlag(int, int)` func native `sub_88A670` wrapper `SqratWrap_void__int_int`
- `void ResetCamera()` func native `sub_88A660` wrapper `SqratWrap_void__noargs_2`
- `void UpdateCamera()` func native `sub_88A650` wrapper `SqratWrap_void__noargs_2`
- `void ResetViewCamera()` func native `sub_88A610` wrapper `SqratWrap_void__noargs_2`
- `void CameraShift_Set(string)` func native `sub_88A530` wrapper `SqratWrap_void__string`
- `void CameraShift_Clear()` func native `sub_88A4F0` wrapper `SqratWrap_void__noargs_2`
- `int SetStopTime(string)` func native `sub_88A340` wrapper `SqratWrap_int__string`
- `int SetStopTimeAll(string)` func native `sub_88A2D0` wrapper `SqratWrap_int__string`
- `int IsMyStopTime(int)` func native `sub_88A270` wrapper `SqratWrap_int__int`
- `void ForceKO()` func native `sub_88A1F0` wrapper `SqratWrap_void__noargs_2`
- `int GetKOMode()` func native `sub_88A1C0` wrapper `SqratWrap_int__noargs`
- `int GetKOFadeOutTime()` func native `sub_88A1B0` wrapper `SqratWrap_int__noargs`
- `int IsKOSkip()` func native `sub_88A1A0` wrapper `SqratWrap_int__noargs`
- `int IsKoFinishType()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `void SetNoDamageFlag(int)` func native `sub_88A170` wrapper `SqratWrap_void__int`
- `int SetHpGauge(string)` func native `sub_889F30` wrapper `SqratWrap_int__string`
- `int SetSpGauge(string)` func native `sub_889C30` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimit(string)` func native `sub_889B90` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimitCombo(string)` func native `sub_889A00` wrapper `SqratWrap_int__string`
- `int SpGauge_SetLimitBound(string)` func native `sub_889910` wrapper `SqratWrap_int__string`
- `int SpGauge_SetZeroTime(string)` func native `sub_889AF0` wrapper `SqratWrap_int__string`
- `int SpGauge_SetAwakens(int)` func native `sub_889860` wrapper `SqratWrap_int__int`
- `int SpGauge_GetAwakens()` func native `sub_889870` wrapper `SqratWrap_int__noargs`
- `int SpGauge_GetPlusCorrect_ComboTimePos()` func native `sub_889880` wrapper `SqratWrap_int__noargs`
- `void SetExist(string)` func native `sub_88AD50` wrapper `SqratWrap_void__string`
- `void SetPaletteSlot(int, int)` func native `sub_88AC90` wrapper `SqratWrap_void__int_int`
- `int GetPaletteSlot()` func native `sub_88ABA0` wrapper `SqratWrap_int__noargs`
- `void SetOtherPalette(int, int)` func native `sub_88ABD0` wrapper `SqratWrap_void__int_int`
- `instance GetPointStatus(string)` func native `sub_8895D0` wrapper `SqratWrap_instance__string`
- `instance GetVector_FromAngle(string)` func native `sub_8894A0` wrapper `SqratWrap_instance__string_2`
- `instance GetAngle_FromVector(string)` func native `sub_889420` wrapper `SqratWrap_instance__string`
- `int SetBoundSt(string)` func native `sub_88C860` wrapper `SqratWrap_int__string`
- `int SetUkemiTime(string)` func native `sub_88C7A0` wrapper `SqratWrap_int__string`
- `int VecHitFlag_Set(int, int)` func native `sub_88C710` wrapper `SqratWrap_int__int_int`
- `value CheckHantei(string)` func native `sub_88C4C0` wrapper `SqratWrap_value__string`
- `value CheckHantei2(string)` func native `sub_88B720` wrapper `SqratWrap_value__string`
- `instance GetHanteiRect(string)` func native `sub_88B4E0` wrapper `SqratWrap_instance__string_3`
- `int IsAttackRect()` func native `sub_88B490` wrapper `SqratWrap_int__noargs`
- `int IsKuraiRect()` func native `sub_88B450` wrapper `SqratWrap_int__noargs`
- `value GetCameraPosition(string)` func native `sub_888DA0` wrapper `SqratWrap_value__string_2`
- `instance GetCameraRect(string)` func native `sub_888C20` wrapper `SqratWrap_instance__string_3`
- `int SetPlayerTimer(string)` func native `sub_888AD0` wrapper `SqratWrap_int__string`
- `int GetPlayerMutekiTimer(int)` func native `sub_888A60` wrapper `SqratWrap_int__int`
- `int GetPlayerMuteki(int)` func native `sub_888A30` wrapper `SqratWrap_int__int`
- `float GetBoundVectorAngle(int)` func native `sub_888970` wrapper `SqratWrap_float__int`
- `int AddAttackHitNum(int)` func native `sub_888920` wrapper `SqratWrap_int__int`
- `void ClearAttackHitNum()` func native `sub_8888A0` wrapper `SqratWrap_void__noargs_2`
- `instance GetAttackHitPos()` func native `sub_8888D0` wrapper `SqratWrap_instance__noargs_7`
- `int Liberate_OK()` func native `sub_888830` wrapper `SqratWrap_int__noargs`
- `void Liberate_Set()` func native `sub_888580` wrapper `SqratWrap_void__noargs_2`
- `void Liberate_SetMinusValue(int)` func native `sub_888510` wrapper `SqratWrap_void__int`
- `int Liberate_Get()` func native `sub_888450` wrapper `SqratWrap_int__noargs`
- `int Liberate_Calc(string)` func native `sub_8882D0` wrapper `SqratWrap_int__string`
- `void Liberate_End()` func native `sub_888490` wrapper `SqratWrap_void__noargs_2`
- `void Liberate_SetType(int)` func native `sub_888420` wrapper `SqratWrap_void__int`
- `int Liberate_GetType()` func native `sub_8883E0` wrapper `SqratWrap_int__noargs`
- `int Liberate_GetTimeValue()` func native `sub_8882C0` wrapper `SqratWrap_int__noargs`
- `int GRD_UseStock(string)` func native `sub_887C80` wrapper `SqratWrap_int__string`
- `int GRD_CheckStock(string)` func native `sub_887DC0` wrapper `SqratWrap_int__string`
- `int GRD_GetJudgeResult()` func native `sub_887D50` wrapper `SqratWrap_int__noargs`
- `int GRD_GetJudgeWinCount()` func native `sub_887D10` wrapper `SqratWrap_int__noargs`
- `int GRD_IsBreakImpact(int)` func native `sub_888260` wrapper `SqratWrap_int__int`
- `void GRD_SetBreak(int, int, int)` func native `sub_888200` wrapper `SqratWrap_void__int_int_int`
- `int GRD_GetBreak(int)` func native `sub_8880E0` wrapper `SqratWrap_int__int`
- `void GRD_CorrectBreakTime(int, int)` func native `sub_888150` wrapper `SqratWrap_void__int_int`
- `int GRD_ClearBreak(int)` func native `sub_887F80` wrapper `SqratWrap_int__int`
- `int GRD_SetConvertBreak(string)` func native `sub_887EA0` wrapper `SqratWrap_int__string`
- `int GRD_GetConvertCount()` func native `sub_887E60` wrapper `SqratWrap_int__noargs`
- `int GRD_ClearVorpal(int)` func native `sub_888080` wrapper `SqratWrap_int__int`
- `int GRD_GetTime(int)` func native `sub_888040` wrapper `SqratWrap_int__int`
- `void GRD_SetSpFlag(int)` func native `sub_888000` wrapper `SqratWrap_void__int`
- `int GRD_AddValue(string)` func native `sub_887BC0` wrapper `SqratWrap_int__string`
- `int GRD_TS_SetPlusValue(string)` func native `sub_887B40` wrapper `SqratWrap_int__string`
- `int GRD_TS_AddValue(string)` func native `sub_887AD0` wrapper `SqratWrap_int__string`
- `void SetPositionBufferFlag(int)` func native `sub_887A70` wrapper `SqratWrap_void__int`
- `int GuardSP_Set(string)` func native `sub_887910` wrapper `SqratWrap_int__string`
- `int GuardSP_Get()` func native `sub_8878E0` wrapper `SqratWrap_int__noargs`
- `int GuardSP_Success()` func native `sub_8878B0` wrapper `SqratWrap_int__noargs`
- `int ClearGuardSP_Success()` func native `sub_887880` wrapper `SqratWrap_int__noargs`
- `void AttackInfoString_Set(string)` func native `sub_887810` wrapper `SqratWrap_void__string`
- `void FadeProc_SetRenderFlag(int)` func native `sub_8877F0` wrapper `SqratWrap_void__int`
- `int FadeProc_Set(string)` func native `sub_8874F0` wrapper `SqratWrap_int__string`
- `int FadeProc_Erase(string)` func native `sub_887400` wrapper `SqratWrap_int__string`
- `int CutInProc_Set(string)` func native `sub_8870E0` wrapper `SqratWrap_int__string`
- `int CutInProc_Erase(string)` func native `sub_887010` wrapper `SqratWrap_int__string`
- `int CutInProc_SetResourceView(string)` func native `sub_886FA0` wrapper `SqratWrap_int__string`
- `int SetCharaFlash(string)` func native `sub_886ED0` wrapper `SqratWrap_int__string`
- `int SetCharaColor(string)` func native `sub_886DF0` wrapper `SqratWrap_int__string`
- `int SetCharaDrawType(string)` func native `sub_886D90` wrapper `SqratWrap_int__string`
- `int PcAfterImage_Set(string)` func native `sub_886C20` wrapper `SqratWrap_int__string`
- `int PcAfterImage_Clear()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int PcAfterImage_ClearEx(int)` func native `sub_886B40` wrapper `SqratWrap_int__int`
- `int PcAuraEffect_Set(string)` func native `SqBind_CharaAura_Set` wrapper `SqratWrap_int__string`
- `int PcAuraEffect_Clear()` func native `SqBind_CharaAura_Clear` wrapper `SqratWrap_int__noargs`
- `int Slowmotion_Set(string)` func native `sub_8866D0` wrapper `SqratWrap_int__string`
- `int LinePrim_Set(string)` func native `sub_886480` wrapper `SqratWrap_int__string`
- `int LinePrim_SetTypeParam(string)` func native `sub_886220` wrapper `SqratWrap_int__string`
- `int Cockpit_SetView(string)` func native `sub_886190` wrapper `SqratWrap_int__string`
- `int Cockpit_SetPrioU(int)` func native `sub_886140` wrapper `SqratWrap_int__int`
- `void ComboView_Clear()` func native `sub_886100` wrapper `SqratWrap_void__noargs_2`
- `int ComboView_Set(string)` func native `sub_886010` wrapper `SqratWrap_int__string`
- `int ComboView_Get()` func native `sub_885FC0` wrapper `SqratWrap_int__noargs`
- `void ChangeStartCorrectTiming()` func native `sub_885F30` wrapper `SqratWrap_void__noargs_2`
- `void ChangeStartCorrectTimingPlus()` func native `sub_885F80` wrapper `SqratWrap_void__noargs_2`
- `void BGM_Set(string)` func native `sub_885EE0` wrapper `SqratWrap_void__string`
- `int BG_GetNum()` func native `sub_885ED0` wrapper `SqratWrap_int__noargs`
- `int AddSkillCount(int)` func native `sub_887970` wrapper `SqratWrap_int__int`
- `int GetEntranceCallCount()` func native `sub_885EC0` wrapper `SqratWrap_int__noargs`
- `int IsReplayEntrance()` func native `sub_885EB0` wrapper `SqratWrap_int__noargs`
- `void SetBgPrioEx(string)` func native `sub_885E30` wrapper `SqratWrap_void__string`
- `int GetMyObjectCount()` func native `sub_885E00` wrapper `SqratWrap_int__noargs`
- `void SetGuardPlusFlag(int)` func native `sub_885DD0` wrapper `SqratWrap_void__int`
- `int Chara_SetMessage(int, int, object, int, int)` func native `sub_885BC0` wrapper `SqratWrap_int__int_int_object_int_int`
- `int Chara_SetMessageBattle(int, int, object, int, int)` func native `sub_885B70` wrapper `SqratWrap_int__int_int_object_int_int`
- `int Chara_SetMessagePC(int, int, object, int, int)` func native `sub_885BC0` wrapper `SqratWrap_int__int_int_object_int_int`
- `void Chara_SetMessageFontSize(int)` func native `sub_885B10` wrapper `SqratWrap_void__int`
- `void Chara_SetMessageDelay(int)` func native `sub_885AB0` wrapper `SqratWrap_void__int`
- `int Chara_JoinMessage(int)` func native `sub_885A20` wrapper `SqratWrap_int__int`
- `void Chara_ClearMessageAll()` func native `sub_8859A0` wrapper `SqratWrap_void__noargs_2`
- `void SetAllKeyCheck(int)` func native `sub_885980` wrapper `SqratWrap_void__int`
- `void SetDefaultExtendedEffectDrawing(int)` func native `sub_885940` wrapper `SqratWrap_void__int`
- `void Score_Add(int)` func native `sub_8858F0` wrapper `SqratWrap_void__int`
- `void SetBgColor(string)` func native `SqBind_SetBgColor` wrapper `SqratWrap_void__string`
- `void SetBgSpeculer(string)` func native `SqBind_SetBgSpeculer` wrapper `SqratWrap_void__string`
- `int GetSeType()` func native `sub_885800` wrapper `SqratWrap_int__noargs`
- `int TM_IsHudHidden()` func native `sub_8857D0` wrapper `SqratWrap_int__noargs`
- `int TM_GetCharaGauge(int)` func native `sub_8857A0` wrapper `SqratWrap_int__int`
- `int TM_IsEnemyAllShield()` func native `sub_885760` wrapper `SqratWrap_int__noargs`
- `int TM_IsCelestialVorpalGRDMode()` func native `sub_8856E0` wrapper `SqratWrap_int__noargs`
- `int TM_IsNormalVorpalGRDMode()` func native `sub_885650` wrapper `SqratWrap_int__noargs`
- `int TM_IsQuickRecoverVorpalMode()` func native `sub_885620` wrapper `SqratWrap_int__noargs`
- `void SetCounterAnnounce()` func native `sub_8855F0` wrapper `SqratWrap_void__noargs_2`
- `void SetPunishAnnounce()` func native `sub_8855A0` wrapper `SqratWrap_void__noargs_2`
- `void SetGrdBreakAnnounce()` func native `sub_885560` wrapper `SqratWrap_void__noargs_2`

## CPointStatus (5)

- var `distance` @0 (Sqrat_Class_VarD)
- var `distance_x` @4 (Sqrat_Class_VarD)
- var `distance_y` @8 (Sqrat_Class_VarD)
- var `pos_x` @12 (Sqrat_Class_VarD)
- var `pos_y` @16 (Sqrat_Class_VarD)

## BMvConst (295)

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
- const `_ObjFlags_NoCamera = 0x800000`
- const `_ObjFlags_NoGround = 64`
- const `_ObjFlags_MoveTimeStop = 0x400000`
- const `_ObjProcFlags_Hud = 16`
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
- const `_GuardFlag_ThroughRemoveGuardBound = 0x400000`
- const `_DataType_Me = 0`
- const `_DataType_Player = 1`
- const `_DataType_Parent = 2`
- const `_KOMode_ToEndWait = 208`
- const `_KOMode_ToEndWait_Fade = 209`
- const `_LiberateType_Normal = 0`
- const `_LiberateType_Max = 1`
- const `_LiberateType_Combo = 2`
- const `_isDebugModeExe = 0`
- const `_PlatformType = 0`
- const `_PlatformAPM = 0`
- const `_DevWindows = 1`
- const `_DevPS3 = 0`
- const `_DevVITA = 0`
- const `_DevPS4 = 0`
- const `_DevPS5 = 0`
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
- const `_ImpactFlag_IsHitNoPlus = 1024`
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
- const `_ClearFlag_Landing = 32`
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
- const `_HitCheckFlag_Dive = 64`
- const `_HitCheckFlag_Reverse = 0x40000000`
- const `_HitCheckFlag_FlagEx00 = 32`
- const `_HitCheckFlag_FlagEx01 = 64`
- const `_HitCheckFlag_FlagEx02 = 128`
- const `_HitCheckFlag_FlagEx03 = 256`
- const `_HitCheckFlag_FlagEx04 = 512`
- const `_HitCheckFlag_FlagEx05 = 1024`
- const `_HitCheckFlag_FlagEx06 = 2048`
- const `_HitCheckFlag_FlagEx07 = 4096`
- const `_HitCheckFlag_FlagEx08 = 0x2000`
- const `_HitCheckFlag_FlagEx09 = 0x4000`
- const `_HitCheckFlag_FlagEx10 = 0x8000`
- const `_HitCheckFlag_FlagEx11 = 0x10000`
- const `_HitCheckFlag_FlagEx12 = 0x20000`
- const `_HitCheckFlag_FlagEx13 = 0x40000`
- const `_HitCheckFlag_FlagEx14 = 0x80000`
- const `_HitCheckFlag_FlagEx15 = 0x100000`
- const `_HitCheckFlag_FlagEx16 = 0x200000`
- const `_HitCheckFlag_FlagEx17 = 0x400000`
- const `_HitCheckFlag_FlagEx18 = 0x800000`
- const `_HitCheckFlag_FlagEx19 = (int)&unk_1000000`
- const `_HitCheckFlag_FlagEx20 = (int)&unk_2000000`
- const `_HitCheckFlag_FlagEx21 = (int)&unk_4000000`
- const `_HitCheckFlag_FlagEx22 = 0x8000000`
- const `_HitCheckFlag_FlagEx23 = 0x10000000`
- const `_HitCheckFlag_FlagEx24 = 0x20000000`
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
- const `_MoveCode0_NoMoveBasicAction = (int)&unk_1000000`

## CCheckRect (7)

- `void Reset()` func native `SqratWrap_void__noargs` wrapper `SqratWrap_void__noargs`
- `void SetPlayer()` func native `SqratWrap_void__noargs` wrapper `SqratWrap_void__noargs`
- `void SetSrc(string)` func native `sub_44C7B0` wrapper `sub_44C7B0`
- `void SetDst(string)` func native `sub_44C7B0` wrapper `sub_44C7B0`
- `void SetFlags(int)` func native `SqratWrap_void__int_2` wrapper `SqratWrap_void__int_2`
- `int Check()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `instance GetCharaData()` func native `sub_44C6F0` wrapper `sub_44C6F0`

## ProcTimer (8)

- `void End()` func native `sub_44D584` wrapper `SqratWrap_void__noargs`
- `void ReStart()` func native `sub_44D57F` wrapper `SqratWrap_void__noargs`
- `void SetName(object)` func native `sub_44CF10` wrapper `SqratWrap_void__object`
- `float GetTime()` func native `sub_44CF80` wrapper `SqratWrap_float__noargs_2`
- `void PrintTime_Int()` func native `nullsub_68` wrapper `SqratWrap_void__noargs`
- `float GetNowTime()` func native `sub_44D589` wrapper `SqratWrap_float__noargs_2`
- `void PrintNowTime_Int()` func native `nullsub_69` wrapper `SqratWrap_void__noargs`
- `void EndPrint_Off()` func native `sub_44D0B0` wrapper `SqratWrap_void__noargs`

## BSound (9)

- `void SE_Play(string)` func native `sub_88F280` wrapper `SqratWrap_void__string`
- `void SE_Stop(string)` func native `sub_88F0E0` wrapper `SqratWrap_void__string`
- `int SE_GetUserParam(string)` func native `sub_88EF00` wrapper `SqratWrap_int__string`
- `int SE_GetBufferSize(string)` func native `sub_88EEC0` wrapper `SqratWrap_int__string`
- `void BGM_Mute(int)` func native `nullsub_70` wrapper `SqratWrap_void__int`
- `void BGM_SetNum(int)` func native `sub_88EEA0` wrapper `SqratWrap_void__int`
- `void BGM_SetFade(string)` func native `sub_88EE60` wrapper `SqratWrap_void__string`
- `void SE_SetFade(string)` func native `sub_88EE20` wrapper `SqratWrap_void__string`
- `void Voice_SetFade(string)` func native `sub_88EDE0` wrapper `SqratWrap_void__string`

## BMvSoundConst (3)

- const `_SeType_All = -2`
- const `_SeType_Normal = -1`
- const `_SeType_Player = 0`

## CBtlInfo (8)

- `int CheckCharaAlive(int)` func native `SqratWrap_int__int_3` wrapper `SqratWrap_int__int_3`
- `int CheckCharaAlive_str(object)` func native `SqratWrap_int__object_2` wrapper `SqratWrap_int__object_2`
- `int IsSelectBgm()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int GetSelectBgm()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int IsBgmAuto()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int IsBgmMute()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `int GetFixStageBGM()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void SetBattleBgm(int)` func native `SqratWrap_void__int_2` wrapper `SqratWrap_void__int_2`

## BMvTbl (203)

- `int GetPlayerSide()` func native `sub_899310` wrapper `SqratWrap_int__noargs`
- `int GetPlayerNo()` func native `sub_8992E0` wrapper `SqratWrap_int__noargs`
- `int IsActivePlayer(int)` func native `sub_899210` wrapper `SqratWrap_int__int`
- `int SetPattern(string)` func native `sub_89A390` wrapper `SqratWrap_int__string`
- `int SetPattern_Reserve(string)` func native `sub_89A350` wrapper `SqratWrap_int__string`
- `void SetMuki(int)` func native `sub_899140` wrapper `SqratWrap_void__int`
- `int GetMuki()` func native `sub_899100` wrapper `SqratWrap_int__noargs`
- `int CheckFurimuki()` func native `sub_8990C0` wrapper `SqratWrap_int__noargs`
- `int VGuard_CheckKeep()` func native `sub_899090` wrapper `SqratWrap_int__noargs`
- `int VGuard_Time(int)` func native `sub_899050` wrapper `SqratWrap_int__int`
- `int CheckStickHold(int)` func native `sub_898FD0` wrapper `SqratWrap_int__int`
- `int CheckStickHoldFrameKeyBuffer(int, int)` func native `sub_898F40` wrapper `SqratWrap_int__int_int`
- `int CheckButtonHold(int)` func native `sub_898DF0` wrapper `SqratWrap_int__int`
- `int CheckButtonHoldFrame(int, int)` func native `sub_898D50` wrapper `SqratWrap_int__int_int`
- `int GetStickHold()` func native `sub_898EE0` wrapper `SqratWrap_int__noargs`
- `int GetButtonHold()` func native `sub_898E80` wrapper `SqratWrap_int__noargs`
- `int IsStartTrigger()` func native `sub_898CE0` wrapper `SqratWrap_int__noargs`
- `void SetNextMoveTable(object)` func native `sub_898C30` wrapper `SqratWrap_void__object_2`
- `void SetDeleteMoveTable()` func native `sub_898C20` wrapper `SqratWrap_void__noargs_2`
- `void SetFinalize(int)` func native `sub_898BE0` wrapper `SqratWrap_void__int`
- `void SetFinalizeCode(int)` func native `sub_898BB0` wrapper `SqratWrap_void__int`
- `int GetFinalizeCode()` func native `sub_898B80` wrapper `SqratWrap_int__noargs`
- `int FromFinalize()` func native `sub_898B50` wrapper `SqratWrap_int__noargs`
- `void SetMoveCode(int)` func native `sub_898AC0` wrapper `SqratWrap_void__int`
- `int GetMoveCode()` func native `sub_898A90` wrapper `SqratWrap_int__noargs`
- `void SetMoveCodeEx(int, int)` func native `sub_898B20` wrapper `SqratWrap_void__int_int`
- `int GetMoveCodeEx(int)` func native `sub_898AF0` wrapper `SqratWrap_int__int`
- `int GetCmdParam(int)` func native `sub_898870` wrapper `SqratWrap_int__int`
- `int GetCmdNumber()` func native `sub_898840` wrapper `SqratWrap_int__noargs`
- `int GetSkillLv()` func native `sub_8988A0` wrapper `SqratWrap_int__noargs`
- `void SetSkillLv(int)` func native `sub_8988D0` wrapper `SqratWrap_void__int`
- `void SetMvAction(int)` func native `sub_898A60` wrapper `SqratWrap_void__int`
- `int GetMvAction()` func native `sub_898A30` wrapper `SqratWrap_int__noargs`
- `void SetMvDamageRateCorrect(int)` func native `sub_8989F0` wrapper `SqratWrap_void__int`
- `int GetMvDamageRateCorrect()` func native `sub_8989C0` wrapper `SqratWrap_int__noargs`
- `void SetSparkDisableAttack(int)` func native `sub_898960` wrapper `SqratWrap_void__int`
- `int GetSparkDisableAttack()` func native `sub_898990` wrapper `SqratWrap_int__noargs`
- `void SetSparkDisableMove(int)` func native `sub_898900` wrapper `SqratWrap_void__int`
- `int GetSparkDisableMove()` func native `sub_898930` wrapper `SqratWrap_int__noargs`
- `int ChangeMv_GetMoveCodeEx(int)` func native `sub_898810` wrapper `SqratWrap_int__int`
- `string ChangeMv_GetMvName()` func native `sub_8987D0` wrapper `SqratWrap_string__noargs`
- `int ChangeMv_GetMvAction()` func native `sub_8987A0` wrapper `SqratWrap_int__noargs`
- `int ChangeMv_GetMvDamageRateCorrect()` func native `sub_898770` wrapper `SqratWrap_int__noargs`
- `int GetSP(int)` func native `sub_8986D0` wrapper `SqratWrap_int__int`
- `void SetSP(int, int)` func native `sub_898670` wrapper `SqratWrap_void__int_int`
- `int AddSP(int, int)` func native `sub_898610` wrapper `SqratWrap_int__int_int`
- `int GetPP(int)` func native `sub_8985B0` wrapper `SqratWrap_int__int`
- `void SetPP(int, int)` func native `sub_898560` wrapper `SqratWrap_void__int_int`
- `int AddPP(int, int)` func native `sub_898510` wrapper `SqratWrap_int__int_int`
- `int GetLP(int)` func native `sub_898410` wrapper `SqratWrap_int__int`
- `void SetLP(int, int)` func native `sub_8983E0` wrapper `SqratWrap_void__int_int`
- `int AddLP(int, int)` func native `sub_8983A0` wrapper `SqratWrap_int__int_int`
- `int GetLPEx(int, int)` func native `sub_8984D0` wrapper `SqratWrap_int__int_int`
- `void SetLPEx(int, int, int)` func native `sub_898490` wrapper `SqratWrap_void__int_int_int`
- `int AddLPEx(int, int, int)` func native `sub_898440` wrapper `SqratWrap_int__int_int_int`
- `void SetComboLimit(int)` func native `sub_898340` wrapper `SqratWrap_void__int`
- `void ClearComboLimit(int)` func native `sub_8982B0` wrapper `SqratWrap_void__int`
- `int SetWallCount(int)` func native `sub_898260` wrapper `SqratWrap_int__int`
- `int AddAirCount(int, int)` func native `sub_8981E0` wrapper `SqratWrap_int__int_int`
- `void SetAirJumpOK(int)` func native `sub_8981B0` wrapper `SqratWrap_void__int`
- `int AddAirJumpCount(int)` func native `sub_898190` wrapper `SqratWrap_int__int`
- `void ClearAllAirCounts()` func native `sub_898160` wrapper `SqratWrap_void__noargs_2`
- `int TobiParam_Func(string)` func native `sub_898070` wrapper `SqratWrap_int__string`
- `int TobiParamEndFlag_Func(string)` func native `sub_897F80` wrapper `SqratWrap_int__string`
- `int CheckPosState(int)` func native `sub_897F00` wrapper `SqratWrap_int__int`
- `int CheckPattern(string)` func native `sub_897C00` wrapper `SqratWrap_int__string`
- `int GetFrameParam(int)` func native `sub_897B90` wrapper `SqratWrap_int__int`
- `int GetFrameID()` func native `sub_897BD0` wrapper `SqratWrap_int__noargs`
- `int JumpFrameID(int)` func native `sub_896C50` wrapper `SqratWrap_int__int`
- `int JumpFrameID_Reserve(int)` func native `sub_896C10` wrapper `SqratWrap_int__int`
- `int Frame_Proc(int, int)` func native `sub_896BB0` wrapper `SqratWrap_int__int_int`
- `int CheckFrameFinalize(int)` func native `sub_896B50` wrapper `SqratWrap_int__int`
- `void SetPrio(int)` func native `sub_897B50` wrapper `SqratWrap_void__int`
- `void SetSpPrio(string)` func native `sub_897AB0` wrapper `SqratWrap_void__string`
- `void SetCommandLongDelay(int)` func native `sub_897A60` wrapper `SqratWrap_void__int`
- `void SetCommandShortDelay(int)` func native `sub_897A10` wrapper `SqratWrap_void__int`
- `int CheckCommand(string)` func native `sub_897650` wrapper `SqratWrap_int__string`
- `int CheckCommandString(string)` func native `sub_897350` wrapper `SqratWrap_int__string`
- `int CheckLastStick(string)` func native `sub_896FA0` wrapper `SqratWrap_int__string`
- `int ClearCommand()` func native `sub_896F50` wrapper `SqratWrap_int__noargs`
- `int SetCommandFailTime(string)` func native `sub_896DB0` wrapper `SqratWrap_int__string`
- `int CheckCommandFailTime(string)` func native `sub_896CE0` wrapper `SqratWrap_int__string`
- `instance GetMvStatus()` func native `sub_899710` wrapper `SqratWrap_instance__noargs_8`
- `instance GetMvStatusBF()` func native `sub_899670` wrapper `SqratWrap_instance__noargs_8`
- `int CheckMvUniqueID(string)` func native `sub_8995B0` wrapper `SqratWrap_int__string`
- `string GetMvName()` func native `sub_899560` wrapper `SqratWrap_string__noargs`
- `void SetMvHitStatus(instance)` func native `sub_899460` wrapper `SqratWrap_void__instance_2`
- `instance GetMvHitStatus()` func native `sub_899520` wrapper `SqratWrap_instance__noargs_9`
- `instance GetMvHitStatusBF()` func native `sub_8994A0` wrapper `SqratWrap_instance__noargs_9`
- `void ClearHitStatus()` func native `sub_899430` wrapper `SqratWrap_void__noargs_2`
- `void SetHitStop(int, int)` func native `sub_8993F0` wrapper `SqratWrap_void__int_int`
- `int GetHitStop()` func native `sub_8993A0` wrapper `SqratWrap_int__noargs`
- `int SetVector(string)` func native `sub_89A120` wrapper `SqratWrap_int__string`
- `instance GetVector(string)` func native `sub_899E00` wrapper `SqratWrap_instance__string_2`
- `int SetVector_CCharaVector(string)` func native `sub_89A080` wrapper `SqratWrap_int__string_2`
- `int ClearTriVector(string)` func native `sub_899FF0` wrapper `SqratWrap_int__string`
- `void SetVector_MaxX(int)` func native `sub_89A0E0` wrapper `SqratWrap_void__int`
- `value GetPosition(string)` func native `sub_899D60` wrapper `SqratWrap_value__string_2`
- `int SetPosition(string)` func native `sub_899AF0` wrapper `SqratWrap_int__string`
- `int MoveStartPosition()` func native `sub_899AA0` wrapper `SqratWrap_int__noargs`
- `instance GetScreenPosition()` func native `Script_GetScreenPosition` wrapper `SqratWrap_instance__noargs_7`
- `int SetAngle(string)` func native `sub_8998A0` wrapper `SqratWrap_int__string`
- `int GetAngle()` func native `sub_899870` wrapper `SqratWrap_int__noargs`
- `void SetScale(string)` func native `sub_8997E0` wrapper `SqratWrap_void__string`
- `instance GetScale()` func native `sub_899770` wrapper `SqratWrap_instance__noargs_7`
- `int TalkScript_IsDone()` func native `sub_896B20` wrapper `SqratWrap_int__noargs`
- `instance GetMvRoundStatus()` func native `sub_896870` wrapper `SqratWrap_instance__noargs_10`
- `instance GetMvStageStatus()` func native `sub_896A40` wrapper `SqratWrap_instance__noargs_11`
- `instance GetMvNetworkInfo()` func native `sub_896780` wrapper `SqratWrap_instance__noargs_12`
- `void SetRoundEnd()` func native `sub_8966B0` wrapper `SqratWrap_void__noargs_2`
- `void SetRoundStart()` func native `sub_896660` wrapper `SqratWrap_void__noargs_2`
- `void SetRoundStartType(int)` func native `sub_896610` wrapper `SqratWrap_void__int`
- `int SetMoveableFlag(string)` func native `sub_896350` wrapper `SqratWrap_int__string`
- `int SetMoveableFlagEx(string)` func native `sub_896240` wrapper `SqratWrap_int__string`
- `int SetAsFlag(string)` func native `sub_896000` wrapper `SqratWrap_int__string`
- `int SetAsStatusFlag(string)` func native `sub_896160` wrapper `SqratWrap_int__string`
- `int SetAsPosStatusFlag(string)` func native `sub_8960B0` wrapper `SqratWrap_int__string`
- `int SetAtkGuardFlag(string)` func native `sub_895F50` wrapper `SqratWrap_int__string`
- `int GetAtkGuardFlag()` func native `sub_895F00` wrapper `SqratWrap_int__noargs`
- `int SetChipDamStatus(string)` func native `sub_895E30` wrapper `SqratWrap_int__string`
- `int SetNoBoundCorrectFlag(string)` func native `sub_895D50` wrapper `SqratWrap_int__string`
- `int SetAtkCatchFlag(string)` func native `sub_895C10` wrapper `SqratWrap_int__string`
- `int GetAtkCatchIsDone(string)` func native `sub_895BD0` wrapper `SqratWrap_int__string`
- `int IsExistAtkCatchRect(string)` func native `sub_895BA0` wrapper `SqratWrap_int__string`
- `int GetAtkCatchHanteiPos(string)` func native `sub_895B60` wrapper `SqratWrap_int__string`
- `int SetSousaiFlag(string)` func native `sub_894EC0` wrapper `SqratWrap_int__string`
- `int CalcHitValue(int)` func native `sub_895D00` wrapper `SqratWrap_int__int`
- `instance GetRecoverStatus()` func native `sub_896740` wrapper `SqratWrap_instance__noargs_13`
- `int CheckRecoverCommandTiming()` func native `sub_896700` wrapper `SqratWrap_int__noargs`
- `int PcGauge_Set(string)` func native `sub_895A30` wrapper `SqratWrap_int__string`
- `int BoundCorrect_Calc(string)` func native `sub_895970` wrapper `SqratWrap_int__string`
- `int ComboPoint_Calc(string)` func native `sub_895890` wrapper `SqratWrap_int__string`
- `int UseSkill_Check(string)` func native `sub_895810` wrapper `SqratWrap_int__string`
- `int HitPat_Check(string)` func native `sub_895730` wrapper `SqratWrap_int__string`
- `int SetHitCheckFlag(string)` func native `sub_895640` wrapper `SqratWrap_int__string`
- `int GetHitCheckFlag(int)` func native `sub_8955B0` wrapper `SqratWrap_int__int`
- `int AddHitCheckFlag(int, int)` func native `sub_895550` wrapper `SqratWrap_int__int_int`
- `int EraseHitCheckFlag(int, int)` func native `sub_8954B0` wrapper `SqratWrap_int__int_int`
- `int GetHitCheckMvFlag()` func native `sub_895480` wrapper `SqratWrap_int__noargs`
- `int SetNoHoseiFlag(string)` func native `sub_895340` wrapper `SqratWrap_int__string`
- `int SetNoUkemiTimeLimitFlag(string)` func native `sub_895280` wrapper `SqratWrap_int__string`
- `int SetSuperArmorFlag(string)` func native `sub_895240` wrapper `SqratWrap_int__string`
- `int GetSuperArmorFlag()` func native `sub_895200` wrapper `SqratWrap_int__noargs`
- `int SetVirtualGuardFlag(string)` func native `sub_8951C0` wrapper `SqratWrap_int__string`
- `int SetKasanariValue(string)` func native `sub_895180` wrapper `SqratWrap_int__string`
- `int SetDrawAlpha(string)` func native `sub_895140` wrapper `SqratWrap_int__string`
- `int GetDrawAlpha(string)` func native `sub_895100` wrapper `SqratWrap_int__string`
- `int SetCaptureHitFlag(string)` func native `sub_8950C0` wrapper `SqratWrap_int__string`
- `int GetNoHoseiHitFlag()` func native `sub_8952C0` wrapper `SqratWrap_int__noargs`
- `int SetNoHoseiHitFlag(string)` func native `sub_895300` wrapper `SqratWrap_int__string`
- `int SetAliveFlag(string)` func native `sub_895080` wrapper `SqratWrap_int__string`
- `int SetCounterHitFlag(string)` func native `sub_895030` wrapper `SqratWrap_int__string`
- `int GetCounterHitFlag()` func native `sub_895000` wrapper `SqratWrap_int__noargs`
- `int SetPartnerFlag(string)` func native `sub_894FC0` wrapper `SqratWrap_int__string`
- `int SetForceUkemiTimeLimitFlag(string)` func native `sub_894F80` wrapper `SqratWrap_int__string`
- `int SetVsValue(string)` func native `sub_894BE0` wrapper `SqratWrap_int__string`
- `int GetVsValue_HP(int)` func native `sub_894C20` wrapper `SqratWrap_int__int`
- `int SetVsAtkValue(string)` func native `sub_894DE0` wrapper `SqratWrap_int__string`
- `int SetVsDefValue(string)` func native `sub_894DA0` wrapper `SqratWrap_int__string`
- `int SetInvAtkFlag(string)` func native `sub_894D60` wrapper `SqratWrap_int__string`
- `int SetInvDefFlag(string)` func native `sub_894D20` wrapper `SqratWrap_int__string`
- `int GetInvAtkFlag(int)` func native `sub_894CF0` wrapper `SqratWrap_int__int`
- `int GetInvDefFlag(int)` func native `sub_894CC0` wrapper `SqratWrap_int__int`
- `int SetHoseiMinValue(int)` func native `sub_894B80` wrapper `SqratWrap_int__int`
- `int GetHoseiMinValue()` func native `sub_894B30` wrapper `SqratWrap_int__noargs`
- `int SetHoseiBaseMinValue(int)` func native `sub_894AD0` wrapper `SqratWrap_int__int`
- `int GetHoseiBaseMinValue()` func native `sub_894A80` wrapper `SqratWrap_int__noargs`
- `int SetCelestialVorpalMode(int)` func native `sub_894A10` wrapper `SqratWrap_int__int`
- `int GetCelestialVorpalMode()` func native `sub_8949B0` wrapper `SqratWrap_int__noargs`
- `int SetGrdLimit(int)` func native `sub_894970` wrapper `SqratWrap_int__int`
- `void AddComboRate()` func native `sub_8948B0` wrapper `SqratWrap_void__noargs_2`
- `void SetComboRateValue(int)` func native `sub_894860` wrapper `SqratWrap_void__int`
- `int GetComboInfo(int)` func native `sub_894750` wrapper `SqratWrap_int__int`
- `void AddComboCount(int)` func native `sub_894800` wrapper `SqratWrap_void__int`
- `int SetHan6LayerMask(string)` func native `sub_894700` wrapper `SqratWrap_int__string`
- `int CheckBlast_IsNGStatus(string)` func native `sub_893CD0` wrapper `SqratWrap_int__string`
- `int CheckBlast_BoundInit(string)` func native `sub_893BA0` wrapper `SqratWrap_int__string`
- `int DamageFlag_Func(int, int)` func native `sub_893B60` wrapper `SqratWrap_int__int_int`
- `int DamageFlag_IsBound(string)` func native `sub_893B30` wrapper `SqratWrap_int__string`
- `int GetSelectColor()` func native `sub_899370` wrapper `SqratWrap_int__noargs`
- `int GetCharaNo()` func native `sub_899340` wrapper `SqratWrap_int__noargs`
- `void SetTutoMvFlag(int)` func native `sub_8946D0` wrapper `SqratWrap_void__int`
- `int GetTutoMvFlag()` func native `sub_8946A0` wrapper `SqratWrap_int__noargs`
- `void AirSkill_Begin()` func native `sub_894410` wrapper `SqratWrap_void__noargs_2`
- `void AirSkill_Check()` func native `sub_8943B0` wrapper `SqratWrap_void__noargs_2`
- `void TM_SetDownStatus()` func native `sub_894340` wrapper `SqratWrap_void__noargs_2`
- `int TM_GetCancelShieldTiming()` func native `sub_8942A0` wrapper `SqratWrap_int__noargs`
- `int GetMvCancel(int)` func native `sub_8941E0` wrapper `SqratWrap_int__int`
- `int IsCpu()` func native `sub_894670` wrapper `SqratWrap_int__noargs`
- `int IsStartSide()` func native `sub_894630` wrapper `SqratWrap_int__noargs`
- `int IsMissionSample()` func native `sub_8945F0` wrapper `SqratWrap_int__noargs`
- `int GetBoss()` func native `sub_894470` wrapper `SqratWrap_int__noargs`
- `int GetPositionBuffer(int)` func native `sub_8945B0` wrapper `SqratWrap_int__int`
- `int GetControlType()` func native `sub_894580` wrapper `SqratWrap_int__noargs`
- `int GetRapidBeatType(int)` func native `sub_894500` wrapper `SqratWrap_int__int`
- `void Achievement_Unlock(int)` func native `nullsub_71` wrapper `SqratWrap_void__int`
- `void BattleActivity_Increment(int)` func native `sub_893B10` wrapper `SqratWrap_void__int`
- `void SetRecoverMvFlags(int)` func native `sub_893AE0` wrapper `SqratWrap_void__int`
- `int GetRecoverMvFlags()` func native `sub_893AA0` wrapper `SqratWrap_int__noargs`
- `int GenarateCmdUniqueID()` func native `sub_893A30` wrapper `SqratWrap_int__noargs`
- `int SetCmdUniqueID(int)` func native `sub_8939C0` wrapper `SqratWrap_int__int`
- `int GetCmdUniqueID()` func native `sub_893990` wrapper `SqratWrap_int__noargs`
- `int GetSubtitleFlag()` func native `sub_893980` wrapper `SqratWrap_int__noargs`

## CCharaVector (8)

- `int x()` method native `sub_442C80` wrapper `sub_442C80`
- `void x(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y()` method native `sub_442C80` wrapper `sub_442C80`
- `void y(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int addx()` method native `sub_442C80` wrapper `sub_442C80`
- `void addx(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int addy()` method native `sub_442C80` wrapper `sub_442C80`
- `void addy(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## CCharaPosition (4)

- `int x()` method native `sub_442C80` wrapper `sub_442C80`
- `void x(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y()` method native `sub_442C80` wrapper `sub_442C80`
- `void y(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## CCharaRect (8)

- `int sx()` method native `sub_442C80` wrapper `sub_442C80`
- `void sx(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int sy()` method native `sub_442C80` wrapper `sub_442C80`
- `void sy(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int ex()` method native `sub_442C80` wrapper `sub_442C80`
- `void ex(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int ey()` method native `sub_442C80` wrapper `sub_442C80`
- `void ey(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## CCharaPoint4 (16)

- `int x0()` method native `sub_442C80` wrapper `sub_442C80`
- `void x0(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y0()` method native `sub_442C80` wrapper `sub_442C80`
- `void y0(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int x1()` method native `sub_442C80` wrapper `sub_442C80`
- `void x1(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y1()` method native `sub_442C80` wrapper `sub_442C80`
- `void y1(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int x2()` method native `sub_442C80` wrapper `sub_442C80`
- `void x2(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y2()` method native `sub_442C80` wrapper `sub_442C80`
- `void y2(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int x3()` method native `sub_442C80` wrapper `sub_442C80`
- `void x3(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int y3()` method native `sub_442C80` wrapper `sub_442C80`
- `void y3(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## MvStatus (53)

- `int CharaNo()` method native `sub_450C40` wrapper `sub_450C40`
- `void CharaNo(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int CallCount()` method native `sub_450C40` wrapper `sub_450C40`
- `void CallCount(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int FrameCallCount()` method native `sub_450C40` wrapper `sub_450C40`
- `void FrameCallCount(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isUpdate()` method native `sub_450C40` wrapper `sub_450C40`
- `void isUpdate(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int MvCount()` method native `sub_442C80` wrapper `sub_442C80`
- `void MvCount(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int MvCountFrame()` method native `sub_442C80` wrapper `sub_442C80`
- `void MvCountFrame(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int isFrameUpdate()` method native `sub_450C40` wrapper `sub_450C40`
- `void isFrameUpdate(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isLanding()` method native `sub_450C40` wrapper `sub_450C40`
- `void isLanding(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int DataPattern()` method native `sub_450C40` wrapper `sub_450C40`
- `void DataPattern(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int DataFrame()` method native `sub_450C40` wrapper `sub_450C40`
- `void DataFrame(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int DataDelayVal()` method native `sub_450C40` wrapper `sub_450C40`
- `void DataDelayVal(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int DataDelay()` method native `sub_450C40` wrapper `sub_450C40`
- `void DataDelay(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int flags()` method native `sub_450C40` wrapper `sub_450C40`
- `void flags(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int FrameID()` method native `sub_450C40` wrapper `sub_450C40`
- `void FrameID(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Param0()` method native `sub_450C40` wrapper `sub_450C40`
- `void Param0(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Param1()` method native `sub_450C40` wrapper `sub_450C40`
- `void Param1(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Param2()` method native `sub_450C40` wrapper `sub_450C40`
- `void Param2(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Param3()` method native `sub_450C40` wrapper `sub_450C40`
- `void Param3(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int GetCharaNo()` func native `sub_44D750` wrapper `SqratWrap_int__noargs_2`
- `int GetCallCount()` func native `sub_44D760` wrapper `SqratWrap_int__noargs_2`
- `int GetMvCount()` func native `sub_4307E0` wrapper `SqratWrap_int__noargs_3`
- `int GetMvCountFrame()` func native `sub_44D810` wrapper `SqratWrap_int__noargs_3`
- `int IsUpdate()` func native `sub_44D780` wrapper `SqratWrap_int__noargs_2`
- `int IsFrameUpdate()` func native `sub_44D790` wrapper `SqratWrap_int__noargs_2`
- `int GetDataPattern()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetDataFrame()` func native `sub_44D830` wrapper `SqratWrap_int__noargs_2`
- `int GetFrameID()` func native `sub_44D7D0` wrapper `SqratWrap_int__noargs_2`
- `int IsLanding()` func native `sub_430130` wrapper `SqratWrap_int__noargs_2`
- `int GetDataDelayVal()` func native `sub_430120` wrapper `SqratWrap_int__noargs_2`
- `int GetDataDelay()` func native `sub_44D7A0` wrapper `SqratWrap_int__noargs_2`
- `int GetFlags()` func native `sub_44D7B0` wrapper `SqratWrap_int__noargs_2`
- `int GetParam0()` func native `sub_44D7E0` wrapper `SqratWrap_int__noargs_2`
- `int GetParam1()` func native `sub_430170` wrapper `SqratWrap_int__noargs_2`
- `int GetParam2()` func native `sub_44D7F0` wrapper `SqratWrap_int__noargs_2`
- `int GetParam3()` func native `sub_44D800` wrapper `SqratWrap_int__noargs_2`

## MvHitStatus (15)

- `int Type()` method native `sub_450C40` wrapper `sub_450C40`
- `void Type(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int PosState()` method native `sub_450C40` wrapper `sub_450C40`
- `void PosState(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isDown()` method native `sub_450C40` wrapper `sub_450C40`
- `void isDown(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isFirstUpdate()` method native `sub_450C40` wrapper `sub_450C40`
- `void isFirstUpdate(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isCatchFlag()` method native `sub_450C40` wrapper `sub_450C40`
- `void isCatchFlag(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isMoveable()` method native `sub_450C40` wrapper `sub_450C40`
- `void isMoveable(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Count()` method native `sub_450C40` wrapper `sub_450C40`
- `void Count(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int IsDown()` func native `sub_44D850` wrapper `SqratWrap_int__noargs_3`

## MvHitImpactInfo (22)

- `int type()` method native `sub_450C40` wrapper `sub_450C40`
- `void type(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int d_state()` method native `sub_450C40` wrapper `sub_450C40`
- `void d_state(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int atk_guard()` method native `sub_442C80` wrapper `sub_442C80`
- `void atk_guard(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int atk_throw()` method native `sub_450C40` wrapper `sub_450C40`
- `void atk_throw(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int def_guard()` method native `sub_450C40` wrapper `sub_450C40`
- `void def_guard(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int grd_break()` method native `sub_450C40` wrapper `sub_450C40`
- `void grd_break(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int armor_break()` method native `sub_450C40` wrapper `sub_450C40`
- `void armor_break(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int counterhit()` method native `sub_450C40` wrapper `sub_450C40`
- `void counterhit(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int counterhittmp()` method native `sub_450C40` wrapper `sub_450C40`
- `void counterhittmp(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int vectortype()` method native `sub_450C40` wrapper `sub_450C40`
- `void vectortype(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int flags()` method native `sub_442C80` wrapper `sub_442C80`
- `void flags(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## MvInterruptInfo (12)

- `int type()` method native `sub_442C80` wrapper `sub_442C80`
- `void type(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Param0()` method native `sub_442C80` wrapper `sub_442C80`
- `void Param0(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Param1()` method native `sub_442C80` wrapper `sub_442C80`
- `void Param1(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Param2()` method native `sub_442C80` wrapper `sub_442C80`
- `void Param2(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Param3()` method native `sub_442C80` wrapper `sub_442C80`
- `void Param3(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Param4()` method native `sub_442C80` wrapper `sub_442C80`
- `void Param4(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## MvRecoverStatus (4)

- `int Type()` method native `sub_442C80` wrapper `sub_442C80`
- `void Type(int)` method native `sub_442CD0` wrapper `sub_442CD0`
- `int Stick()` method native `sub_442C80` wrapper `sub_442C80`
- `void Stick(int)` method native `sub_442CD0` wrapper `sub_442CD0`

## MvRoundStatus (22)

- `int Time()` method native `sub_450C40` wrapper `sub_450C40`
- `void Time(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int TimeUpdate()` method native `sub_450C40` wrapper `sub_450C40`
- `void TimeUpdate(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int Round()` method native `sub_450C40` wrapper `sub_450C40`
- `void Round(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int WinType()` method native `sub_450C40` wrapper `sub_450C40`
- `void WinType(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isKo()` method native `sub_450C40` wrapper `sub_450C40`
- `void isKo(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isMyKo()` method native `sub_450C40` wrapper `sub_450C40`
- `void isMyKo(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isWinRound()` method native `sub_450C40` wrapper `sub_450C40`
- `void isWinRound(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isLoseRound()` method native `sub_450C40` wrapper `sub_450C40`
- `void isLoseRound(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isExtraRound()` method native `sub_450C40` wrapper `sub_450C40`
- `void isExtraRound(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int isFirstLoseRound()` method native `sub_450C40` wrapper `sub_450C40`
- `void isFirstLoseRound(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int CharaMoveMode()` method native `sub_450C40` wrapper `sub_450C40`
- `void CharaMoveMode(int)` method native `sub_450C90` wrapper `sub_450C90`

## MvNetworkInfo (4)

- `int NetworkVsMode()` method native `sub_450C40` wrapper `sub_450C40`
- `void NetworkVsMode(int)` method native `sub_450C90` wrapper `sub_450C90`
- `int NetworkVsControlType()` method native `sub_450C40` wrapper `sub_450C40`
- `void NetworkVsControlType(int)` method native `sub_450C90` wrapper `sub_450C90`

## MvStageStatus (5)

- `int IsStoryBattle()` func native `sub_896B10` wrapper `SqratWrap_int__noargs_3`
- `int IsTrainingBattle()` func native `sub_896AD0` wrapper `SqratWrap_int__noargs_3`
- `int GetCpuChara()` func native `sub_896AB0` wrapper `SqratWrap_int__noargs_3`
- `int GetPcChara()` func native `sub_896A90` wrapper `SqratWrap_int__noargs_3`
- `int GetStartSide()` func native `sub_44D840` wrapper `SqratWrap_int__noargs_3`

## BtlMvStd (10)

- `instance GetBoundStatus()` func native `sub_89AB60` wrapper `SqratWrap_instance__noargs_14`
- `int SetBoundStatus()` func native `sub_89AA20` wrapper `SqratWrap_int__noargs`
- `int SetBoundFinalize()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int ClearBound()` func native `sub_89A8E0` wrapper `SqratWrap_int__noargs`
- `int SetGuardStatus()` func native `sub_89A870` wrapper `SqratWrap_int__noargs`
- `int SetGuardFinalize()` func native `SqratWrap_int__noargs` wrapper `SqratWrap_int__noargs`
- `int ClearGuard()` func native `sub_89A6D0` wrapper `SqratWrap_int__noargs`
- `int IsFileAlive(object)` func native `sub_89A6A0` wrapper `SqratWrap_int__object`
- `int CheckHan6Data(string)` func native `sub_89A6C0` wrapper `SqratWrap_int__string`
- `instance GetBoundVectorStatus(int, int)` func native `sub_89AA90` wrapper `SqratWrap_instance__int_int`

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
- `int IsDone()` func native `sub_44D750` wrapper `SqratWrap_int__noargs_2`
- `int IsCapture()` func native `sub_44D760` wrapper `SqratWrap_int__noargs_2`
- `int IsBound()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetNum()` func native `sub_4300F0` wrapper `SqratWrap_int__noargs_2`
- `int GetVecCount()` func native `sub_430100` wrapper `SqratWrap_int__noargs_2`
- `int GetVecMuki()` func native `sub_44D770` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemi()` func native `sub_44D780` wrapper `SqratWrap_int__noargs_2`
- `int GetWallCount()` func native `sub_44D790` wrapper `SqratWrap_int__noargs_2`
- `string GetName()` func native `sub_430110` wrapper `SqratWrap_string__noargs_2`
- `int GetPower()` func native `sub_430130` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundTime()` func native `sub_430120` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundTimeAll()` func native `sub_44D7A0` wrapper `SqratWrap_int__noargs_2`
- `int GetBoundFlag()` func native `sub_44D7B0` wrapper `SqratWrap_int__noargs_2`
- `int GetCountMax()` func native `sub_44D7D0` wrapper `SqratWrap_int__noargs_2`
- `int GetCountMaxTrue()` func native `sub_44D7E0` wrapper `SqratWrap_int__noargs_2`
- `int GetCountNow()` func native `sub_430170` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemiTimeNow()` func native `sub_44D7F0` wrapper `SqratWrap_int__noargs_2`
- `int GetAirUkemiTimeMax()` func native `sub_44D800` wrapper `SqratWrap_int__noargs_2`
- `int GetVecX()` func native `sub_446AA0` wrapper `SqratWrap_int__noargs_3`
- `int GetVecY()` func native `sub_446AC0` wrapper `SqratWrap_int__noargs_3`
- `int IsDown()` func native `sub_44D7C0` wrapper `SqratWrap_int__noargs_3`

## MvBoundVectorStatus (5)

- `int IsDone()` func native `sub_44D750` wrapper `SqratWrap_int__noargs_2`
- `int GetVectime()` func native `sub_44D760` wrapper `SqratWrap_int__noargs_2`
- `int GetUkemifailedtime()` func native `SqratWrap_int__noargs_2` wrapper `SqratWrap_int__noargs_2`
- `int GetUkeminum_g()` func native `sub_4300D0` wrapper `SqratWrap_int__noargs_2`
- `int GetPower()` func native `sub_4300E0` wrapper `SqratWrap_int__noargs_2`

## debuglog (2)

- `void write(object)` func native `nullsub_77` wrapper `SqratWrap_void__object_2`
- `void write_r(object)` func native `nullsub_76` wrapper `SqratWrap_void__object_2`

## _LogsFile (7)

- `void open(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void opennew(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void write(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void write_r(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void writeD(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void writeD_r(object)` func native `nullsub_1` wrapper `SqratWrap_void__object`
- `void close()` func native `_guard_check_icall_nop` wrapper `SqratWrap_void__noargs`

## debugfont (3)

- `void setprio(int)` func native `sub_8A2B40` wrapper `SqratWrap_void__int`
- `void setcolor(int)` func native `sub_8A2B30` wrapper `SqratWrap_void__int`
- `void draw(int, int, object)` func native `sub_8A2AF0` wrapper `SqratWrap_void__int_int_object`

## debugfontL (4)

- `void setprio(int)` func native `nullsub_75` wrapper `SqratWrap_void__int`
- `void setcolor(int)` func native `nullsub_74` wrapper `SqratWrap_void__int`
- `void draw(int, int, object)` func native `nullsub_73` wrapper `SqratWrap_void__int_int_object`
- `void draw_message(object)` func native `nullsub_72` wrapper `SqratWrap_void__object_2`

## BCMDTbl (31)

- `int GetHP()` func native `sub_8A2A80` wrapper `SqratWrap_int__noargs`
- `int GetHPRatio()` func native `sub_8A2A20` wrapper `SqratWrap_int__noargs`
- `int GetComboGauge()` func native `sub_8A29E0` wrapper `SqratWrap_int__noargs`
- `int GetComboGaugeEx()` func native `sub_8A2990` wrapper `SqratWrap_int__noargs`
- `int CheckPosState(int)` func native `sub_8A28B0` wrapper `SqratWrap_int__int`
- `int IsMoveAble()` func native `sub_8A2950` wrapper `SqratWrap_int__noargs`
- `int IsAddCommand()` func native `sub_8A2880` wrapper `SqratWrap_int__noargs`
- `int CheckPattern(string)` func native `sub_8A2630` wrapper `SqratWrap_int__string`
- `int GetFrameID()` func native `sub_8A2600` wrapper `SqratWrap_int__noargs`
- `int CheckFireballLimit(int)` func native `sub_8A22C0` wrapper `SqratWrap_int__int`
- `int CheckState(int)` func native `sub_8A20D0` wrapper `SqratWrap_int__int`
- `int CheckCancelFlag(int)` func native `sub_8A21B0` wrapper `SqratWrap_int__int`
- `int GetCancelFlagData(int)` func native `sub_8A2160` wrapper `SqratWrap_int__int`
- `int CheckCancel(int)` func native `sub_8A2130` wrapper `SqratWrap_int__int`
- `int CheckDash(int, int)` func native `sub_8A2050` wrapper `SqratWrap_int__int_int`
- `int CheckAirJump(int)` func native `sub_8A2010` wrapper `SqratWrap_int__int`
- `int ResetAirJump()` func native `sub_8A1FE0` wrapper `SqratWrap_int__noargs`
- `int SetAirJump(int)` func native `sub_8A1FA0` wrapper `SqratWrap_int__int`
- `int CheckComboLimit(int)` func native `sub_8A2220` wrapper `SqratWrap_int__int`
- `int GetComboLimitLevel()` func native `sub_8A21E0` wrapper `SqratWrap_int__noargs`
- `int CheckUserParam(int)` func native `sub_8A2290` wrapper `SqratWrap_int__int`
- `int CheckSeedCost(int)` func native `sub_8A2980` wrapper `SqratWrap_int__int`
- `int CheckMoveName(string)` func native `sub_8A22F0` wrapper `SqratWrap_int__string`
- `int SetSpUseBufferTmp(string)` func native `sub_8A1F10` wrapper `SqratWrap_int__string`
- `int GetSpUseBufferTmp()` func native `sub_8A1EE0` wrapper `SqratWrap_int__noargs`
- `int GetSpCommandFlag(int)` func native `sub_8A1E80` wrapper `SqratWrap_int__int`
- `int IsDirectCommand()` func native `sub_8A1E70` wrapper `SqratWrap_int__noargs`
- `instance GetCmdInfo()` func native `sub_8A1E30` wrapper `SqratWrap_instance__noargs_15`
- `instance GetCmdInfoEx(int)` func native `sub_8A1DF0` wrapper `SqratWrap_instance__int_3`
- `int GetSuccessButton()` func native `sub_8A1DE0` wrapper `SqratWrap_int__noargs`
- `int GetSuccessButtonPos()` func native `sub_8A1DD0` wrapper `SqratWrap_int__noargs`

## CmdInfo (5)

- `int GetLastButton(int)` func native `sub_4515C0` wrapper `SqratWrap_int__int_3`
- `int GetCancel(int)` func native `sub_5A28D0` wrapper `SqratWrap_int__int_3`
- `int GetCancelFlag(int)` func native `sub_4515E0` wrapper `SqratWrap_int__int_3`
- `int GetFrame()` func native `sub_449C20` wrapper `SqratWrap_int__noargs_3`
- `int GetNum(int)` func native `sub_4515B0` wrapper `SqratWrap_int__int_3`

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

## vConst (104)

- const `_Status_Stand = 0`
- const `_Status_Air = 1`
- const `_Status_Crouch = 2`
- const `_GuardFlag_Stand = 1`
- const `_GuardFlag_Air = 2`
- const `_GuardFlag_Crouch = 4`
- const `_GuardFlag_ShStand = 16`
- const `_GuardFlag_ShStand = 16`
- const `_GuardFlag_ShAir = 32`
- const `_GuardFlag_ShCrouch = 64`
- const `_GuardFlag_ShXStand = 0x80000`
- const `_GuardFlag_ShXAir = 0x100000`
- const `_GuardFlag_ShXCrouch = 0x200000`
- const `_GuardFlag_XStand = 256`
- const `_GuardFlag_XAir = 512`
- const `_GuardFlag_XCrouch = 1024`
- const `_GuardFlag_XBound = 2048`
- const `_GuardFlag_XGuard = 4096`
- const `_GuardFlag_XDown = 0x2000`
- const `_GuardFlag_XExceptGuard = 0x4000`
- const `_GuardFlag_XExceptDown = 0x8000`
- const `_GuardFlag_XRemoveBound = 0x10000`
- const `_GuardFlag_XRemoveDamage = 0x20000`
- const `_GuardFlag_XRemoveDamagePlus = 0x40000`
- const `_GuardFlag_XRemoveGuardBound = 0x400000`
- const `_ThrowType_IsDone = 1`
- const `_ThrowType_Kasanari = 2`
- const `_ThrowType_Center = 4`
- const `_ThrowType_HitNageMuteki = 8`
- const `_ThrowType_NoRecover = 16`
- const `_ThrowType_NoRecoverEx = 32`
- const `_ThrowType_TypeBlow = 64`
- const `_FrameFlagEx_ChakutiEnd = 1`
- const `_FrameFlagEx_LoopCheck = 2`
- const `_FrameFlagEx_JumpRel = 4`
- const `_FrameFlagEx_LEJumpRel = 8`
- const `_StatusFlag0_GuardS = 256`
- const `_StatusFlag0_GuardC = 512`
- const `_StatusFlag0_GuardA = 1024`
- const `_StatusFlag1_ExCancel = 1`
- const `_StatusFlag1_JumpCancel = 4`
- const `_StatusFlag1_ChainShift = 8`
- const `_StatusFlag1_GuardRev = 0x80000000`
- const `_StatusFlag1_HitEraseDisable = 16`
- const `_AtkFlag_HpCut = 1`
- const `_AtkFlag_NoKo = 2`
- const `_AtkFlag_ComboEnd = 4`
- const `_AtkFlag_ChgCounterVec = 16`
- const `_AtkFlag_NoHitPlus = 32`
- const `_AtkFlag_HitYure = 64`
- const `_AtkFlag_NoAirUkemi = 128`
- const `_AtkFlag_NoGroundUkemi = 256`
- const `_AtkFlag_TeamHit = 512`
- const `_AtkFlag_NoHitStop = 1024`
- const `_AtkFlag_NoFirstAttack = 4096`
- const `_AtkFlag_ChgReflexHitVec = 0x4000`
- const `_AtkFlag_GuardStop = 0x10000`
- const `_AtkFlag_NoBoundCorrect = 0x20000`
- const `_AtkFlag_NoCounter = 0x40000`
- const `_AtkFlag_VectorSp = 0x80000`
- const `_AtkFlag_NoBoundCorrectPlus = 0x100000`
- const `_AtkFlag_NoPrio = 0x200000`
- const `_AtkFlag_NoWallUkemi = 0x800000`
- const `_AtkFlag_SwingPlus = &unk_1000000`
- const `_AtkFlag_AttackStop = &unk_2000000`
- const `_AtkFlag_ChgHitFlagVec = &unk_4000000`
- const `_AtkFlag_TimeCorrect = 0x8000000`
- const `_AtkFlag_NoBurst = 0x20000000`
- const `_AtkFlag_ArmorBreak = 0x8000`
- const `_HanteiFlag_Head = 1`
- const `_HanteiFlag_Body = 2`
- const `_HanteiFlag_Legs = 4`
- const `_HanteiFlag_FireBall = 8`
- const `_HanteiFlag_Throw = 16`
- const `_HanteiFlag_Reverse = 0x40000000`
- const `_HanteiFlag_FlagEx00 = 32`
- const `_HanteiFlag_FlagEx01 = 64`
- const `_HanteiFlag_FlagEx02 = 128`
- const `_HanteiFlag_FlagEx03 = 256`
- const `_HanteiFlag_FlagEx04 = 512`
- const `_HanteiFlag_FlagEx05 = 1024`
- const `_HanteiFlag_FlagEx06 = 2048`
- const `_HanteiFlag_FlagEx07 = 4096`
- const `_HanteiFlag_FlagEx08 = 0x2000`
- const `_HanteiFlag_FlagEx09 = 0x4000`
- const `_HanteiFlag_FlagEx10 = 0x8000`
- const `_HanteiFlag_FlagEx11 = 0x10000`
- const `_HanteiFlag_FlagEx12 = 0x20000`
- const `_HanteiFlag_FlagEx13 = 0x40000`
- const `_HanteiFlag_FlagEx14 = 0x80000`
- const `_HanteiFlag_FlagEx15 = 0x100000`
- const `_HanteiFlag_FlagEx16 = 0x200000`
- const `_HanteiFlag_FlagEx17 = 0x400000`
- const `_HanteiFlag_FlagEx18 = 0x800000`
- const `_HanteiFlag_FlagEx19 = &unk_1000000`
- const `_HanteiFlag_FlagEx20 = &unk_2000000`
- const `_HanteiFlag_FlagEx21 = &unk_4000000`
- const `_HanteiFlag_FlagEx22 = 0x8000000`
- const `_HanteiFlag_FlagEx23 = 0x10000000`
- const `_HanteiFlag_FlagEx24 = 0x20000000`
- const `_Han6Hantei_Attack = 0`
- const `_Han6Hantei_Kasanari = 1`
- const `_Han6Hantei_Kurai = 2`
- const `_Han6Hantei_Etc = 3`

## CDat (2)

- `int Load(object)` func native `sub_6063B0` wrapper `SqratWrap_int__object_2`
- `void SetPosition(int, int)` func native `sub_606300` wrapper `SqratWrap_void__int_int_2`

## CAnime (19)

- `void SetCDat(instance)` func native `unknown_libname_4` wrapper `SqratWrap_void__instance_3`
- `void AddFrame(object, float)` func native `sub_608940` wrapper `SqratWrap_void__object_float`
- `void AddFrameEx(object, float, int)` func native `sub_607D70` wrapper `SqratWrap_void__object_float_int`
- `void AddJump(int)` func native `sub_608920` wrapper `SqratWrap_void__int_2`
- `void AddModeJump(int, int, int)` func native `sub_607BC0` wrapper `SqratWrap_void__int_int_int_2`
- `void AddTagJump(object)` func native `sub_607FE0` wrapper `SqratWrap_void__object`
- `void AddModeTagJump(object, int, int)` func native `sub_6081C0` wrapper `SqratWrap_void__object_int_int`
- `void AddTag(object)` func native `sub_6080D0` wrapper `SqratWrap_void__object`
- `void AddSe(int)` func native `sub_6083B0` wrapper `SqratWrap_void__int_2`
- `void AddModeTag(int)` func native `sub_607B50` wrapper `SqratWrap_void__int_2`
- `void SetPrio(int)` func native `sub_423620` wrapper `SqratWrap_void__int_2`
- `void SetMode(int)` func native `sub_424120` wrapper `SqratWrap_void__int_2`
- `int GetMode()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void AddMode(int)` func native `sub_453000` wrapper `SqratWrap_void__int_2`
- `int IsNull()` func native `sub_4494C0` wrapper `SqratWrap_int__noargs_3`
- `void SetView(int)` func native `sub_424110` wrapper `SqratWrap_void__int_2`
- `int GetView()` func native `SqratWrap_int__noargs_3` wrapper `SqratWrap_int__noargs_3`
- `void ClearFrame()` func native `sub_6088C0` wrapper `SqratWrap_void__noargs`
- `void SetStartWait(float)` func native `sub_423630` wrapper `SqratWrap_void__float`

## CAniManager (7)

- `instance Add(instance, object)` func native `__std_stacktrace_to_string` wrapper `SqratWrap_instance__instance_object`
- `int Render()` func native `sub_608370` wrapper `SqratWrap_int__noargs_3`
- `int Update()` func native `sub_6086A0` wrapper `SqratWrap_int__noargs_3`
- `int Reset()` func native `sub_607F10` wrapper `SqratWrap_int__noargs_3`
- `void Finalize()` func native `sub_6086E0` wrapper `SqratWrap_void__noargs`
- `void SetMode(int)` func native `sub_424140` wrapper `SqratWrap_void__int_2`
- `instance GetCAnime(object)` func native `sub_453010` wrapper `SqratWrap_instance__object`

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

- `void UseFileInSquirrel(string)` func native `sub_8D20F0` wrapper `SqratWrap_void__string`
- `void UseFileInSquirrel(string)` func native `SqratWrap_void__string` wrapper `SqratWrap_void__string`

