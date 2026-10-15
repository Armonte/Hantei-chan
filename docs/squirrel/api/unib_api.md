# Native Squirrel API (generated from the IDB, do not edit)

Source: `unib_raw.json`. Counts: const 228, func 219, method 21, table 38, var 81.

Types are inferred from the Sqrat wrapper (`sq_get*`/`sq_push*` calls); `value` = pushes a non-primitive, `string` may also accept an int via Sqrat::Object.

## CStatus (5)

- var `Winner` @16 (Sqrat_Class_VarA_46B6F0)
- var `WinnerNum` @44 (Sqrat_Class_VarA_46B750)
- var `Player` @8 (Sqrat_Class_VarA_46B750)
- var `Stage` @0 (Sqrat_Class_VarA_46B750)
- var `NextStage` @4 (Sqrat_Class_VarA_46B750)

## tSceneWinDemo (1)

- `int GetStatus()` func native `sub_72D3A0` wrapper `sub_72D3A0`

## CPlayerSt (2)

- var `Player1` @8 (Sqrat_Class_VarA_46AB70)
- var `Player2` @36 (Sqrat_Class_VarA_46AB70)

## tSceneVS (1)

- `int GetStatus()` func native `sub_72DF00` wrapper `sub_72DF00`

## debuglog (2)

- `void write()` func native `sub_72FF90` wrapper `sub_72FF90`
- `void write_r()` func native `sub_72FF80` wrapper `sub_72FF80`

## BCMDTbl (18)

- `int GetHP()` func native `sub_464A20` wrapper `sub_464A20`
- `int GetHPRatio()` func native `sub_464A50` wrapper `sub_464A50`
- `int GetComboGauge()` func native `sub_464AD0` wrapper `sub_464AD0`
- `int CheckPosState(int)` func native `sub_45E780` wrapper `sub_45E780`
- `int IsMoveAble()` func native `sub_464B00` wrapper `sub_464B00`
- `int IsAddCommand()` func native `sub_464B30` wrapper `sub_464B30`
- `int CheckPattern(int)` func native `sub_464B60` wrapper `sub_464B60`
- `int GetFrameID()` func native `sub_45EA40` wrapper `sub_45EA40`
- `int CheckFireballLimit(int)` func native `sub_464D90` wrapper `sub_464D90`
- `int CheckState(int)` func native `sub_730030` wrapper `sub_730030`
- `int CheckCancel(int)` func native `sub_464E60` wrapper `sub_464E60`
- `int CheckDash(int, int)` func native `sub_464E90` wrapper `sub_464E90`
- `int CheckAirJump(int)` func native `sub_464F20` wrapper `sub_464F20`
- `int CheckComboLimit(int)` func native `sub_464E10` wrapper `sub_464E10`
- `int CheckUserParam(int)` func native `sub_464DD0` wrapper `sub_464DD0`
- `int CheckSeedCost()` func native `sub_72FF70` wrapper `sub_72FF70`
- `int CheckMoveName(int)` func native `sub_7300B0` wrapper `sub_7300B0`
- `int SetSpUseBufferTmp(int)` func native `sub_4593B0` wrapper `sub_4593B0`

## BCMDConst (13)

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

## BtlMvStd (7)

- `int GetBoundStatus(int)` func native `sub_45C3F0` wrapper `sub_45C3F0`
- `int SetBoundStatus()` func native `sub_45C5C0` wrapper `sub_45C5C0`
- `int SetBoundFinalize()` func native `sub_45C630` wrapper `sub_45C630`
- `int ClearBound()` func native `sub_45C6E0` wrapper `sub_45C6E0`
- `int SetGuardStatus()` func native `sub_45C720` wrapper `sub_45C720`
- `int SetGuardFinalize()` func native `sub_45C780` wrapper `sub_45C780`
- `int ClearGuard()` func native `sub_45C830` wrapper `sub_45C830`

## CCharaVector (13)

- var `isDone` @0 (Sqrat_Class_VarA_463080)
- var `isCapture` @2 (Sqrat_Class_VarA_463080)
- var `isBound` @4 (Sqrat_Class_VarA_463080)
- var `VecCount` @8 (Sqrat_Class_VarA_463080)
- var `WallCount` @14 (Sqrat_Class_VarA_463080)
- var `Name` @16 (Sqrat_Class_VarA_4630E0)
- var `Power` @20 (Sqrat_Class_VarA_463080)
- var `AirUkemi` @12 (Sqrat_Class_VarA_463080)
- var `VecMuki` @10 (Sqrat_Class_VarA_463080)
- var `x` @0 (Sqrat_Class_VarA_463440)
- var `y` @4 (Sqrat_Class_VarA_463440)
- var `addx` @8 (Sqrat_Class_VarA_463440)
- var `addy` @12 (Sqrat_Class_VarA_463440)

## BMvTbl (69)

- `int GetPlayerSide()` func native `sub_45DC90` wrapper `sub_45DC90`
- `int SetPattern(int)` func native `sub_45CA30` wrapper `sub_45CA30`
- `int SetMuki(int)` func native `sub_45DCD0` wrapper `sub_45DCD0`
- `int GetMuki()` func native `sub_45DD50` wrapper `sub_45DD50`
- `int CheckFurimuki()` func native `sub_45DDA0` wrapper `sub_45DDA0`
- `int VGuard_CheckKeep()` func native `sub_45DDF0` wrapper `sub_45DDF0`
- `int VGuard_Time(int)` func native `sub_45DE30` wrapper `sub_45DE30`
- `int CheckStickHold(int)` func native `sub_45DE70` wrapper `sub_45DE70`
- `int CheckButtonHold(int)` func native `sub_45DEF0` wrapper `sub_45DEF0`
- `int IsStartTrigger()` func native `sub_45DF70` wrapper `sub_45DF70`
- `instance SetNextMoveTable(string)` func native `sub_45DFE0` wrapper `sub_45DFE0`
- `int SetDeleteMoveTable()` func native `sub_45E080` wrapper `sub_45E080`
- `instance SetFinalize(int)` func native `sub_45E0A0` wrapper `sub_45E0A0`
- `instance SetFinalizeCode(int)` func native `sub_45E0E0` wrapper `sub_45E0E0`
- `int GetFinalizeCode()` func native `sub_45E120` wrapper `sub_45E120`
- `int FromFinalize()` func native `sub_45E160` wrapper `sub_45E160`
- `void SetMoveCode(int)` func native `sub_45E1A0` wrapper `sub_45E1A0`
- `int GetMoveCode()` func native `sub_45E1E0` wrapper `sub_45E1E0`
- `int GetPP(int)` func native `sub_45E220` wrapper `sub_45E220`
- `instance SetPP(int, int)` func native `sub_45E280` wrapper `sub_45E280`
- `int AddPP(int, int)` func native `sub_45E2E0` wrapper `sub_45E2E0`
- `int GetLP(int)` func native `sub_45E350` wrapper `sub_45E350`
- `int SetLP(int, int)` func native `sub_45E390` wrapper `sub_45E390`
- `int AddLP(int, int)` func native `sub_45E3D0` wrapper `sub_45E3D0`
- `instance SetComboLimit(int)` func native `sub_45E420` wrapper `sub_45E420`
- `int SetWallCount(int)` func native `sub_45E470` wrapper `sub_45E470`
- `int SetAirJumpOK(int)` func native `sub_45E4B0` wrapper `sub_45E4B0`
- `int AddAirJumpCount(int)` func native `sub_45E4E0` wrapper `sub_45E4E0`
- `int TobiParam_Func(int)` func native `sub_45E550` wrapper `sub_45E550`
- `int TobiParamEndFlag_Func(int)` func native `sub_45E680` wrapper `sub_45E680`
- `int CheckPosState(int)` func native `sub_45E780` wrapper `sub_45E780`
- `int CheckPattern(int)` func native `sub_45E900` wrapper `sub_45E900`
- `int GetFrameParam(int)` func native `sub_45EA80` wrapper `sub_45EA80`
- `int GetFrameID()` func native `sub_45EA40` wrapper `sub_45EA40`
- `int JumpFrameID(int)` func native `sub_45F0F0` wrapper `sub_45F0F0`
- `int Frame_Proc(int, int)` func native `sub_45F150` wrapper `sub_45F150`
- `instance SetPrio(int)` func native `sub_45EAD0` wrapper `sub_45EAD0`
- `int SetCommandLongDelay(int)` func native `sub_45EB10` wrapper `sub_45EB10`
- `int CheckCommand(int)` func native `sub_45EC20` wrapper `sub_45EC20`
- `int CheckCommandString(int)` func native `sub_45EDE0` wrapper `sub_45EDE0`
- `int ClearCommand()` func native `sub_45F030` wrapper `sub_45F030`
- `instance GetMvStatus(instance)` func native `sub_45D900` wrapper `sub_45D900`
- `int GetMvName()` func native `sub_45DAA0` wrapper `sub_45DAA0`
- `int SetMvHitStatus(int)` func native `sub_45DC10` wrapper `sub_45DC10`
- `int GetMvHitStatus(int)` func native `sub_45DAF0` wrapper `sub_45DAF0`
- `int ClearHitStatus()` func native `sub_45DC60` wrapper `sub_45DC60`
- `int SetVector(int)` func native `sub_45CD80` wrapper `sub_45CD80`
- `instance GetVector(instance)` func native `sub_45CF70` wrapper `sub_45CF70`
- `? SetVector_CCharaVector()` func native `sub_45CF10` wrapper `sub_45CF10`
- `int SetVector_MaxX(int)` func native `sub_45CEB0` wrapper `sub_45CEB0`
- `instance SetSpPrio(int)` func native `sub_45D2B0` wrapper `sub_45D2B0`
- `instance GetPosition(instance, int)` func native `sub_45D1E0` wrapper `sub_45D1E0`
- `int SetPosition(int)` func native `sub_45D350` wrapper `sub_45D350`
- `int SetAngle(int)` func native `sub_45D560` wrapper `sub_45D560`
- `int GetAngle()` func native `sub_45D7D0` wrapper `sub_45D7D0`
- `instance SetScale(int)` func native `sub_45D800` wrapper `sub_45D800`
- `instance GetScale(instance)` func native `sub_45D880` wrapper `sub_45D880`
- `int TalkScript_IsDone()` func native `sub_45F190` wrapper `sub_45F190`
- `instance GetMvRoundStatus(instance)` func native `sub_45F1A0` wrapper `sub_45F1A0`
- `int SetRoundEnd()` func native `sub_45F380` wrapper `sub_45F380`
- `int SetRoundStart()` func native `sub_45F3D0` wrapper `sub_45F3D0`
- `int SetMoveableFlag(int)` func native `sub_45F600` wrapper `sub_45F600`
- `int SetAsFlag(int)` func native `sub_45F750` wrapper `sub_45F750`
- `int SetAsStatusFlag(int)` func native `sub_45F690` wrapper `sub_45F690`
- `int SetAtkCatchFlag(int)` func native `sub_45F860` wrapper `sub_45F860`
- `int CalcHitValue(int)` func native `sub_45F7E0` wrapper `sub_45F7E0`
- `int GetRecoverStatus(int)` func native `sub_45F330` wrapper `sub_45F330`
- `int PcGauge_Set(int)` func native `sub_45F940` wrapper `sub_45F940`
- `int BoundCorrect_Calc(int)` func native `sub_45FAD0` wrapper `sub_45FAD0`

## CCharaPosition (2)

- var `x` @0 (Sqrat_Class_VarA_4634A0)
- var `y` @4 (Sqrat_Class_VarA_4634A0)

## CCharaRect (4)

- var `sx` @0 (Sqrat_Class_VarA_463500)
- var `sy` @4 (Sqrat_Class_VarA_463500)
- var `ex` @8 (Sqrat_Class_VarA_463500)
- var `ey` @12 (Sqrat_Class_VarA_463500)

## CCharaPoint4 (8)

- var `x0` @0 (Sqrat_Class_VarA_463560)
- var `y0` @4 (Sqrat_Class_VarA_463560)
- var `x1` @8 (Sqrat_Class_VarA_463560)
- var `y1` @12 (Sqrat_Class_VarA_463560)
- var `x2` @16 (Sqrat_Class_VarA_463560)
- var `y2` @20 (Sqrat_Class_VarA_463560)
- var `x3` @24 (Sqrat_Class_VarA_463560)
- var `y3` @28 (Sqrat_Class_VarA_463560)

## MvStatus (14)

- var `CharaNo` @0 (Sqrat_Class_VarA_4635C0)
- var `isUpdate` @8 (Sqrat_Class_VarA_4635C0)
- var `MvCount` @4 (Sqrat_Class_VarA_463620)
- var `FrameID` @16 (Sqrat_Class_VarA_4635C0)
- var `isFrameUpdate` @10 (Sqrat_Class_VarA_4635C0)
- var `isLanding` @18 (Sqrat_Class_VarA_4635C0)
- var `DataPattern` @12 (Sqrat_Class_VarA_4635C0)
- var `DataFrame` @14 (Sqrat_Class_VarA_4635C0)
- var `DataDelayVal` @20 (Sqrat_Class_VarA_4635C0)
- var `flags` @22 (Sqrat_Class_VarA_4635C0)
- var `Param0` @24 (Sqrat_Class_VarA_4635C0)
- var `Param1` @26 (Sqrat_Class_VarA_4635C0)
- var `Param2` @28 (Sqrat_Class_VarA_4635C0)
- var `Param3` @30 (Sqrat_Class_VarA_4635C0)

## MvHitStatus (5)

- var `Type` @0 (Sqrat_Class_VarA_463680)
- var `PosState` @2 (Sqrat_Class_VarA_463680)
- var `isDown` @4 (Sqrat_Class_VarA_463680)
- var `isFirstUpdate` @8 (Sqrat_Class_VarA_463680)
- var `Count` @6 (Sqrat_Class_VarA_463680)

## MvHitImpactInfo (8)

- var `type` @0 (Sqrat_Class_VarA_4636E0)
- var `d_state` @2 (Sqrat_Class_VarA_4636E0)
- var `atk_guard` @4 (Sqrat_Class_VarA_4636E0)
- var `atk_throw` @6 (Sqrat_Class_VarA_4636E0)
- var `def_guard` @8 (Sqrat_Class_VarA_4636E0)
- var `grd_break` @10 (Sqrat_Class_VarA_4636E0)
- var `counterhit` @12 (Sqrat_Class_VarA_4636E0)
- var `flags` @16 (Sqrat_Class_VarA_463740)

## MvInterruptInfo (1)

- var `type` @0 (Sqrat_Class_VarA_4637A0)

## MvRecoverStatus (1)

- var `Type` @0 (Sqrat_Class_VarA_463800)

## MvRoundStatus (10)

- var `Time` @10 (Sqrat_Class_VarA_463860)
- var `TimeUpdate` @12 (Sqrat_Class_VarA_463860)
- var `Round` @14 (Sqrat_Class_VarA_463860)
- var `WinType` @16 (Sqrat_Class_VarA_463860)
- var `isKo` @0 (Sqrat_Class_VarA_463860)
- var `isMyKo` @2 (Sqrat_Class_VarA_463860)
- var `isWinRound` @4 (Sqrat_Class_VarA_463860)
- var `isLoseRound` @6 (Sqrat_Class_VarA_463860)
- var `isFinalRound` @8 (Sqrat_Class_VarA_463860)
- var `CharaMoveMode` @18 (Sqrat_Class_VarA_463860)

## BMvConst (164)

- const `_isDebugModeExe = 0`
- const `_PosState_Stand = 1`
- const `_PosState_Air = 2`
- const `_PosState_Crouch = 4`
- const `_PosState_Ground = 5`
- const `_PosState_Always = 7`
- const `_PrioType_ = 7`
- const `_HitType_Hit = 3`
- const `_HitType_Damage = 2`
- const `_HitType_Guard = 1`
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
- const `_InterruptType_Ko_Atk = 0`
- const `_InterruptType_Ko_Def = 1`
- const `_InterruptType_Judge = 2`
- const `_ImpactFlag_IsBound = 1`
- const `_ImpactFlag_IsGuard = 2`
- const `_ImpactFlag_IsCounter = 128`
- const `_ImpactFlag_IsDangerHP = 64`
- const `_CancelFlag_None = 0`
- const `_CancelFlag_Hit = 1`
- const `_CancelFlag_Always = 2`
- const `_CancelFlag_Damage = 3`
- const `_CancelFlag_Invalid = 255`
- const `_ClearFlag_ChangeFrame = 2`
- const `_ClearFlag_ChangePattern = 4`
- const `_ClearFlag_ChangeMv = 1`
- const `_AsFlag_ExCancel = 1`
- const `_AsFlag_ChainShift = 2`
- const `_AsFlag_GuardRev = 4`
- const `_AsFlag_Guard_Stand = 16`
- const `_AsFlag_Guard_Air = 32`
- const `_AsFlag_Guard_Crouch = 64`
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
- const `_CatchFlag_AtkStandGuard = 1`
- const `_CatchFlag_AtkAirGuard = 2`
- const `_CatchFlag_AtkCrouchGuard = 4`
- const `_CatchFlag_AtkNoGuard = 8`
- const `_CatchFlag_AtkNoGuardThrow = 16`
- const `_CatchFlag_StateStand = 32`
- const `_CatchFlag_StateAir = 64`
- const `_CatchFlag_StateCrouch = 128`
- const `_CatchFlag_AtkAllGuard = 7`
- const `_CatchSuccess_HitSub = 1`
- const `_CatchSuccess_HitSub_Enemy = 2`
- const `_CharaMoveMode_Enable = 0`
- const `_CharaMoveMode_Limit = 1`
- const `_CharaMoveMode_Disable = 2`
- const `_PCGaugeType_Eltnum = 2`
- const `_MvStFlag_CounterAtk = 1`
- const `_MvStFlag_CounterDef = 2`
- const `_MvStFlag_DangerHPAtk = 8`
- const `_MvStFlag_DangerHPDef = 4`
- const `_ObjFlags_ParentMove = 4`
- const `_ObjFlags_EraseParentDamage = 1`
- const `_ObjFlags_NoRender = 0x20000000`
- const `_ObjFlags_NoRenderBlackOut = 0x200000`
- const `_ObjFlags_NoRenderOrder = 0x100000`
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
- const `_ObjFlags_NoCamera = &off_800000`
- const `_ObjFlags_NoGround = 64`
- const `_ObjProcFlags_EraseParentNull = 1`
- const `_ObjProcFlags_EraseChangeParentMv = 2`
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

## BMvIf (1)

- `int StdCall(int)` func native `sub_45F420` wrapper `sub_45F420`

## BSound (3)

- `int SE_Play(int)` func native `sub_45AD70` wrapper `sub_45AD70`
- `void SE_Stop(int)` func native `sub_45B010` wrapper `sub_45B010`
- `int BGM_Mute()` func native `sub_45B160` wrapper `sub_45B160`

## BMvSoundConst (3)

- const `_SeType_All = -2`
- const `_SeType_Normal = -1`
- const `_SeType_Player = 0`

## BMvEff (80)

- `int Random()` func native `sub_457140` wrapper `sub_457140`
- `float Random_F()` method native `sub_457150` wrapper `sub_457150`
- `int Random_Limit(int)` func native `sub_457170` wrapper `sub_457170`
- `float Random_FAbs(int)` func native `sub_457190` wrapper `sub_457190`
- `instance Random_PointRect(instance, int)` func native `sub_457280` wrapper `sub_457280`
- `instance Random_PointRad(instance, int)` func native `sub_4571C0` wrapper `sub_4571C0`
- `int StdCall(int)` func native `sub_454680` wrapper `sub_454680`
- `int ThrowRelease(int)` func native `sub_455000` wrapper `sub_455000`
- `int ThrowParam(int)` func native `sub_454910` wrapper `sub_454910`
- `int ThrowChara_SetJoint(int)` func native `sub_454D00` wrapper `sub_454D00`
- `int ThrowChara_SetCamera(int)` func native `sub_454DD0` wrapper `sub_454DD0`
- `int ThrowChara_Transfer(int)` func native `sub_454F10` wrapper `sub_454F10`
- `int CapturePlayer(int)` func native `sub_455250` wrapper `sub_455250`
- `instance CreateObject(instance, int)` func native `sub_455810` wrapper `sub_455810`
- `int SetObjectFlags(int)` func native `sub_455B80` wrapper `sub_455B80`
- `instance SetObjectRender(int)` func native `sub_455EF0` wrapper `sub_455EF0`
- `int EraseObjectFlags(int)` func native `sub_455C30` wrapper `sub_455C30`
- `int ObjProcFlags_Set(int)` func native `sub_455CC0` wrapper `sub_455CC0`
- `int ObjProcFlags_Erase(int)` func native `sub_455D40` wrapper `sub_455D40`
- `int ObjType_Set(int)` func native `sub_455DC0` wrapper `sub_455DC0`
- `int SetCamera_Quake(int)` func native `sub_455F90` wrapper `sub_455F90`
- `int SetCamera_Focus(int)` func native `sub_456030` wrapper `sub_456030`
- `int SetCamera_FocusKeep(int)` func native `sub_4562C0` wrapper `sub_4562C0`
- `int ResetCamera()` func native `sub_4562E0` wrapper `sub_4562E0`
- `int ResetViewCamera()` func native `sub_456300` wrapper `sub_456300`
- `instance CameraShift_Set(int)` func native `sub_456350` wrapper `sub_456350`
- `int CameraShift_Clear()` func native `sub_456410` wrapper `sub_456410`
- `int SetStopTime(int)` func native `sub_456450` wrapper `sub_456450`
- `int SetHpGauge(int)` func native `sub_456560` wrapper `sub_456560`
- `int SetSpGauge(int)` func native `sub_4567A0` wrapper `sub_4567A0`
- `int SpGauge_SetLimit(int)` func native `sub_456950` wrapper `sub_456950`
- `int SpGauge_GetPlusCorrect_ComboTimePos()` func native `sub_4569C0` wrapper `sub_4569C0`
- `instance SetExist(int)` func native `sub_455E20` wrapper `sub_455E20`
- `instance GetPointStatus(instance, int)` func native `sub_456C70` wrapper `sub_456C70`
- `instance GetVector_FromAngle(instance, int)` func native `sub_456FA0` wrapper `sub_456FA0`
- `instance GetAngle_FromVector(instance, int)` func native `sub_457080` wrapper `sub_457080`
- `instance CheckHantei(instance, int)` func native `sub_455340` wrapper `sub_455340`
- `instance GetHanteiRect(instance, int)` func native `sub_4555F0` wrapper `sub_4555F0`
- `instance GetCameraPosition(instance, int)` func native `sub_4573C0` wrapper `sub_4573C0`
- `instance GetCameraRect(instance, int)` func native `sub_457520` wrapper `sub_457520`
- `int SetPlayerTimer(int)` func native `sub_457700` wrapper `sub_457700`
- `int AddAttackHitNum(int)` func native `sub_457810` wrapper `sub_457810`
- `int ClearAttackHitNum()` func native `sub_4578F0` wrapper `sub_4578F0`
- `instance GetAttackHitPos(instance)` func native `sub_457870` wrapper `sub_457870`
- `int Liberate_OK()` func native `sub_457930` wrapper `sub_457930`
- `int Liberate_Set()` func native `sub_457990` wrapper `sub_457990`
- `int Liberate_Get()` func native `sub_457A40` wrapper `sub_457A40`
- `int Liberate_Calc(int)` func native `sub_457AA0` wrapper `sub_457AA0`
- `int Liberate_End()` func native `sub_457A10` wrapper `sub_457A10`
- `int GRD_UseStock(int)` func native `sub_458000` wrapper `sub_458000`
- `int GRD_CheckStock(int)` func native `sub_457EC0` wrapper `sub_457EC0`
- `int GRD_GetJudgeResult()` func native `sub_457F70` wrapper `sub_457F70`
- `instance GRD_SetBreak(int)` func native `sub_457B90` wrapper `sub_457B90`
- `int GRD_GetBreak(int)` func native `sub_457C70` wrapper `sub_457C70`
- `instance GRD_CorrectBreakTime(int, int)` func native `sub_457BF0` wrapper `sub_457BF0`
- `int GRD_ClearBreak(int)` func native `sub_457D10` wrapper `sub_457D10`
- `int GRD_SetConvertBreak(int)` func native `sub_457DA0` wrapper `sub_457DA0`
- `int GRD_GetConvertCount()` func native `sub_457E80` wrapper `sub_457E80`
- `int GRD_AddValue(int)` func native `sub_458080` wrapper `sub_458080`
- `int GuardSP_Set(int)` func native `sub_458150` wrapper `sub_458150`
- `int GuardSP_Get()` func native `sub_4581C0` wrapper `sub_4581C0`
- `int GuardSP_Success()` func native `sub_458200` wrapper `sub_458200`
- `instance AttackInfoString_Set(int)` func native `sub_458240` wrapper `sub_458240`
- `int FadeProc_Set(int)` func native `sub_4582C0` wrapper `sub_4582C0`
- `int FadeProc_Erase(int)` func native `sub_458500` wrapper `sub_458500`
- `int CutInProc_Set(int)` func native `sub_4585D0` wrapper `sub_4585D0`
- `int CutInProc_Erase()` func native `sub_458810` wrapper `sub_458810`
- `int SetCharaFlash(int)` func native `sub_458850` wrapper `sub_458850`
- `int SetCharaColor(int)` func native `sub_458900` wrapper `sub_458900`
- `int SetCharaDrawType(int)` func native `sub_4589D0` wrapper `sub_4589D0`
- `int PcAfterImage_Set(int)` func native `sub_458A30` wrapper `sub_458A30`
- `int PcAfterImage_Clear()` func native `sub_458B50` wrapper `sub_458B50`
- `int PcAuraEffect_Set(int)` func native `sub_458BA0` wrapper `sub_458BA0`
- `int PcAuraEffect_Clear()` func native `sub_458EB0` wrapper `sub_458EB0`
- `int Slowmotion_Set(int)` func native `sub_458F00` wrapper `sub_458F00`
- `int LinePrim_Set(int)` func native `sub_458FD0` wrapper `sub_458FD0`
- `void Cockpit_SetView(int)` func native `sub_459290` wrapper `sub_459290`
- `int ComboView_Clear()` func native `sub_459300` wrapper `sub_459300`
- `instance BGM_Set(int)` func native `sub_459350` wrapper `sub_459350`
- `int SetSpUseBuffer(int)` func native `sub_4593B0` wrapper `sub_4593B0`

## CPointStatus (6)

- var `distance` @0 (Sqrat_Class_VarA_459EF0)
- var `distance_x` @4 (Sqrat_Class_VarA_459EF0)
- var `distance_y` @8 (Sqrat_Class_VarA_459EF0)
- var `pos_x` @12 (Sqrat_Class_VarA_459EF0)
- var `pos_y` @16 (Sqrat_Class_VarA_459EF0)
- var `angle` @20 (Sqrat_Class_VarA_459F50)

## SeFunc (1)

- `int GetParam(int)` func native `sub_776DE0` wrapper `sub_776DE0`

## BMvCore (10)

- `instance GetCaptureCharaData(instance)` func native `sub_452DB0` wrapper `sub_452DB0`
- `instance GetParentCharaData(instance)` func native `sub_452E20` wrapper `sub_452E20`
- `instance GetPlayerCharaData(instance)` func native `sub_452E90` wrapper `sub_452E90`
- `instance GetNearEnemyCharaData(instance)` func native `sub_452F00` wrapper `sub_452F00`
- `instance GetEnemyCharaData(instance, int)` func native `sub_452F70` wrapper `sub_452F70`
- `instance GetLastDamageCharaData(instance, int)` func native `sub_453080` wrapper `sub_453080`
- `int SetLastDamageCharaData(instance)` func native `sub_453040` wrapper `sub_453040`
- `instance GetLastHitCharaData(instance, int)` func native `sub_453110` wrapper `sub_453110`
- `int PushCharaData(instance)` func native `sub_4531A0` wrapper `sub_4531A0`
- `int PopCharaData()` func native `sub_453200` wrapper `sub_453200`

## CCharaData (2)

- var `IsDone` @0 (Sqrat_Class_VarA_4537B0)
- var `data` @4 (Sqrat_Class_VarA_453810)

## BCom (5)

- `int Move_Jump(int)` func native `sub_44F4B0` wrapper `sub_44F4B0`
- `int Move_Next()` func native `sub_44F540` wrapper `sub_44F540`
- `int Move_End(int)` func native `sub_44F580` wrapper `sub_44F580`
- `int GetMoveTag()` func native `sub_44F620` wrapper `sub_44F620`
- `instance GetStatus(instance)` func native `sub_44F690` wrapper `sub_44F690`

## UpdateSt (16)

- `? GetLevel()` method native `None` wrapper `None`
- `int GetSkill()` method native `sub_420DD0` wrapper `sub_420DD0`
- `int GetMove()` method native `sub_44F280` wrapper `sub_44F280`
- `int GetSkillTime()` method native `sub_44F2A0` wrapper `sub_44F2A0`
- `int GetMoveTime()` method native `sub_44F2C0` wrapper `sub_44F2C0`
- `? GetELenX()` func native `None` wrapper `None`
- `int GetELenY()` func native `unknown_libname_126` wrapper `unknown_libname_126`
- `int GetHPP()` method native `sub_44F2E0` wrapper `sub_44F2E0`
- `int GetEXS()` method native `sub_44F300` wrapper `sub_44F300`
- `int IsPMoveable()` method native `sub_44F320` wrapper `sub_44F320`
- `int IsPBound()` method native `sub_44F340` wrapper `sub_44F340`
- `int CheckPosState(int)` method native `sub_44F380` wrapper `sub_44F380`
- `int CheckPState(int)` method native `sub_44F3A0` wrapper `sub_44F3A0`
- `int CheckEState(int)` method native `sub_44F3C0` wrapper `sub_44F3C0`
- `int GetTobiTime()` method native `sub_44F3E0` wrapper `sub_44F3E0`
- `int GetComJumpLen()` method native `sub_44F400` wrapper `sub_44F400`

## BMvCoreConst (37)

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
- const `_eComMoveFlag_StickHold = 0x40000000`
- const `_eComMoveFlag_ButtonHold = 0x80000000`
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

## CDat (2)

- `int Load(int)` method native `sub_7CFCD0` wrapper `sub_7CFCD0`
- `int SetPosition(int, int)` method native `sub_7CFA80` wrapper `sub_7CFA80`

## CAnime (18)

- `int AddFrame(string, float)` func native `sub_72CB10` wrapper `sub_72CB10`
- `int AddFrameEx(string, float, int)` func native `sub_72CA60` wrapper `sub_72CA60`
- `int AddJump(int)` func native `sub_72C780` wrapper `sub_72C780`
- `int AddModeJump(int, int, int)` method native `sub_72C6A0` wrapper `sub_72C6A0`
- `int AddTagJump(int)` func native `sub_72CA40` wrapper `sub_72CA40`
- `int AddModeTagJump(int, int, int)` func native `sub_72C900` wrapper `sub_72C900`
- `int AddTag(string)` func native `sub_72C9A0` wrapper `sub_72C9A0`
- `int AddSe(int)` func native `sub_72C5D0` wrapper `sub_72C5D0`
- `int AddModeTag(int)` func native `sub_72C570` wrapper `sub_72C570`
- `? SetPrio()` func native `None` wrapper `None`
- `instance SetMode(int)` func native `sub_4426D0` wrapper `sub_4426D0`
- `? GetMode()` func native `None` wrapper `None`
- `instance AddMode(int)` func native `sub_4680B0` wrapper `sub_4680B0`
- `int IsNull()` func native `sub_44E5C0` wrapper `sub_44E5C0`
- `instance SetView(int)` func native `sub_434BF0` wrapper `sub_434BF0`
- `? GetView()` func native `None` wrapper `None`
- `int ClearFrame()` method native `sub_72C190` wrapper `sub_72C190`
- `float SetStartWait(float)` method native `sub_4426F0` wrapper `sub_4426F0`

## CAniManager (6)

- `int Render()` func native `sub_434EA0` wrapper `sub_434EA0`
- `int Update()` func native `sub_434E30` wrapper `sub_434E30`
- `int Reset()` func native `sub_444680` wrapper `sub_444680`
- `int Finalize()` method native `sub_434D20` wrapper `sub_434D20`
- `int SetMode(int)` func native `sub_442710` wrapper `sub_442710`
- `instance GetCAnime(string)` func native `sub_4680E0` wrapper `sub_4680E0`

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

