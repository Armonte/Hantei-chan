# Native Squirrel API (generated from the IDB, do not edit)

Source: `dfci_raw.json`. Counts: func 422, method 80, table 51, var 41.

Types are inferred from the Sqrat wrapper (`sq_get*`/`sq_push*` calls); `value` = pushes a non-primitive, `string` may also accept an int via Sqrat::Object.

## SeFunc (1)

- `int GetParam(int)` func native `sub_736DF0` wrapper `sub_736DF0`

## CStatus (6)

- var `Winner` @24 (Sqrat_Class_VarA_46D470)
- var `WinnerNum` @52 (Sqrat_Class_VarA_46D4D0)
- var `Player` @12 (Sqrat_Class_VarA_46D4D0)
- var `Stage` @4 (Sqrat_Class_VarA_46D4D0)
- var `NextStage` @8 (Sqrat_Class_VarA_46D4D0)
- var `WinStreak` @20 (Sqrat_Class_VarA_46D4D0)

## tSceneWinDemo (1)

- `void GetStatus()` func native `sub_7CB710` wrapper `sub_7CB710`

## CPlayerSt (7)

- var `Player1` @8 (Sqrat_Class_VarB_46C5B0)
- var `Player2` @92 (Sqrat_Class_VarB_46C5B0)
- var `Player1s` @36 (Sqrat_Class_VarB_46C5B0)
- var `Player2s` @120 (Sqrat_Class_VarB_46C5B0)
- var `Player1ss` @64 (Sqrat_Class_VarB_46C5B0)
- var `Player2ss` @148 (Sqrat_Class_VarB_46C5B0)
- var `bBoard` @176 (Sqrat_Class_VarA_46C610)

## tSceneVS (1)

- `int GetStatus()` func native `sub_7CE930` wrapper `sub_7CE930`

## debuglog (2)

- `void write()` func native `sub_80DC80` wrapper `sub_80DC80`
- `void write_r()` func native `sub_80DC70` wrapper `sub_80DC70`

## _LogsFile (7)

- `void open(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void opennew(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void write(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void write_r(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void writeD(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void writeD_r(int)` func native `sub_45D940` wrapper `sub_45D940`
- `void close()` func native `sub_4787A0` wrapper `sub_4787A0`

## debugfont (3)

- `int setprio(int)` func native `sub_80DC60` wrapper `sub_80DC60`
- `int setcolor(int)` func native `sub_80DC50` wrapper `sub_80DC50`
- `void draw()` func native `sub_80DC40` wrapper `sub_80DC40`

## debugfontL (4)

- `int setprio(int)` func native `sub_80DC30` wrapper `sub_80DC30`
- `int setcolor(int)` func native `sub_80DC20` wrapper `sub_80DC20`
- `void draw()` func native `sub_80DC10` wrapper `sub_80DC10`
- `void draw_message()` func native `sub_80DC00` wrapper `sub_80DC00`

## BCMDTbl (25)

- `int GetHP()` func native `sub_454BE0` wrapper `sub_454BE0`
- `int GetHPRatio()` func native `sub_454C10` wrapper `sub_454C10`
- `int GetComboGauge()` func native `sub_454C90` wrapper `sub_454C90`
- `int GetComboGaugeEx()` func native `sub_454CC0` wrapper `sub_454CC0`
- `int CheckPosState(int)` func native `sub_454D80` wrapper `sub_454D80`
- `int IsMoveAble()` func native `sub_454D50` wrapper `sub_454D50`
- `int IsAddCommand()` func native `sub_454E40` wrapper `sub_454E40`
- `int CheckPattern(instance)` func native `sub_454E70` wrapper `sub_454E70`
- `int GetFrameID()` func native `sub_44CB30` wrapper `sub_44CB30`
- `int CheckFireballLimit(int)` func native `sub_4550A0` wrapper `sub_4550A0`
- `int CheckState(int)` func native `sub_80E0A0` wrapper `sub_80E0A0`
- `int CheckCancelFlag(int)` func native `sub_4551C0` wrapper `sub_4551C0`
- `int CheckCancel(int)` func native `sub_4551F0` wrapper `sub_4551F0`
- `int CheckDash(int, int)` func native `sub_455220` wrapper `sub_455220`
- `int CheckAirJump(int)` func native `sub_455290` wrapper `sub_455290`
- `int CheckComboLimit(int)` func native `sub_455120` wrapper `sub_455120`
- `int GetComboLimitLevel()` func native `sub_455170` wrapper `sub_455170`
- `int CheckUserParam(int)` func native `sub_4550E0` wrapper `sub_4550E0`
- `int CheckSeedCost()` func native `sub_80DBF0` wrapper `sub_80DBF0`
- `int CheckMoveName(int)` func native `sub_80E120` wrapper `sub_80E120`
- `int SetSpUseBufferTmp(int)` func native `sub_4552F0` wrapper `sub_4552F0`
- `int GetCmdInfo()` func native `sub_455400` wrapper `sub_455400`
- `int GetCmdInfoEx(int)` func native `sub_455410` wrapper `sub_455410`
- `int GetSuccessButton()` func native `sub_455430` wrapper `sub_455430`
- `int GetSuccessButtonPos()` func native `sub_455440` wrapper `sub_455440`

## CmdInfo (5)

- `int GetLastButton(int)` func native `sub_454900` wrapper `sub_454900`
- `int GetCancel(int)` func native `sub_81D3B0` wrapper `sub_81D3B0`
- `int GetCancelFlag(int)` func native `sub_454920` wrapper `sub_454920`
- `? GetFrame()` func native `None` wrapper `None`
- `int GetNum(int)` func native `sub_4548E0` wrapper `sub_4548E0`

## CBtlInfo (2)

- `int CheckCharaAlive(int)` func native `sub_80F580` wrapper `sub_80F580`
- `int CheckCharaAlive_str(int)` func native `sub_80F680` wrapper `sub_80F680`

## (root/unbound:sub_80FBF0) (1)

- `int IsVitaMode()` func native `sub_4043D0` wrapper `sub_4043D0`

## BtlMvStd (10)

- `instance GetBoundStatus(instance)` func native `sub_449D70` wrapper `sub_449D70`
- `int SetBoundStatus()` func native `sub_449F60` wrapper `sub_449F60`
- `int SetBoundFinalize()` func native `sub_449FD0` wrapper `sub_449FD0`
- `int ClearBound()` func native `sub_44A080` wrapper `sub_44A080`
- `int EndBoundProc()` func native `sub_44A0C0` wrapper `sub_44A0C0`
- `int SetGuardStatus()` func native `sub_44A120` wrapper `sub_44A120`
- `int SetGuardFinalize()` func native `sub_44A180` wrapper `sub_44A180`
- `int ClearGuard()` func native `sub_44A230` wrapper `sub_44A230`
- `int IsFileAlive(int)` func native `sub_44A2C0` wrapper `sub_44A2C0`
- `int CheckHan6Data()` func native `sub_4043D0` wrapper `sub_4043D0`

## MvBoundStatus (14)

- var `isDone` @0 (Sqrat_Class_VarC_452F30)
- var `isCapture` @2 (Sqrat_Class_VarC_452F30)
- var `isBound` @4 (Sqrat_Class_VarC_452F30)
- var `isBoundNext` @6 (Sqrat_Class_VarC_452F30)
- var `Num` @10 (Sqrat_Class_VarC_452F30)
- var `VecCount` @12 (Sqrat_Class_VarC_452F30)
- var `WallCount` @18 (Sqrat_Class_VarC_452F30)
- var `Name` @20 (Sqrat_Class_VarA_452F90)
- var `Power` @24 (Sqrat_Class_VarC_452F30)
- var `AirUkemi` @16 (Sqrat_Class_VarC_452F30)
- var `VecMuki` @14 (Sqrat_Class_VarC_452F30)
- var `BoundTime` @26 (Sqrat_Class_VarC_452F30)
- var `BoundTimeAll` @28 (Sqrat_Class_VarC_452F30)
- `int IsDown()` func native `sub_449A70` wrapper `sub_449A70`

## BMvTbl (113)

- `int GetPlayerSide()` func native `sub_44B9D0` wrapper `sub_44B9D0`
- `int SetPattern(instance)` func native `sub_44A450` wrapper `sub_44A450`
- `int SetMuki(int)` func native `sub_44BA90` wrapper `sub_44BA90`
- `int GetMuki()` func native `sub_44BB10` wrapper `sub_44BB10`
- `int CheckFurimuki()` func native `sub_44BB60` wrapper `sub_44BB60`
- `int VGuard_CheckKeep()` func native `sub_44BBB0` wrapper `sub_44BBB0`
- `int VGuard_Time(int)` func native `sub_44BBF0` wrapper `sub_44BBF0`
- `int CheckStickHold(int)` func native `sub_44BC30` wrapper `sub_44BC30`
- `int CheckButtonHold(int)` func native `sub_44BD60` wrapper `sub_44BD60`
- `int GetStickHold()` func native `sub_44BCB0` wrapper `sub_44BCB0`
- `int GetButtonHold()` func native `sub_44BD00` wrapper `sub_44BD00`
- `int IsStartTrigger()` func native `sub_44BDE0` wrapper `sub_44BDE0`
- `instance SetNextMoveTable(int)` func native `sub_44BE50` wrapper `sub_44BE50`
- `int SetDeleteMoveTable()` func native `sub_44BEF0` wrapper `sub_44BEF0`
- `instance SetFinalize(int)` func native `sub_44BF10` wrapper `sub_44BF10`
- `instance SetFinalizeCode(int)` func native `sub_44BF50` wrapper `sub_44BF50`
- `int GetFinalizeCode()` func native `sub_44BF90` wrapper `sub_44BF90`
- `int FromFinalize()` func native `sub_44BFD0` wrapper `sub_44BFD0`
- `int SetMoveCode(int)` func native `sub_44C090` wrapper `sub_44C090`
- `int GetMoveCode()` func native `sub_44C0B0` wrapper `sub_44C0B0`
- `instance SetMoveCodeEx(int, int)` func native `sub_44C010` wrapper `sub_44C010`
- `int GetMoveCodeEx(int)` func native `sub_44C050` wrapper `sub_44C050`
- `int GetCmdParam()` func native `sub_44C0C0` wrapper `sub_44C0C0`
- `int GetCmdNumber()` func native `sub_44C100` wrapper `sub_44C100`
- `int ChangeMv_GetMoveCodeEx(int)` func native `sub_44C140` wrapper `sub_44C140`
- `int GetSP(int)` func native `sub_44C220` wrapper `sub_44C220`
- `int SetSP(int, int)` func native `sub_44C250` wrapper `sub_44C250`
- `int AddSP(int, int)` func native `sub_44C280` wrapper `sub_44C280`
- `int GetPP(int)` func native `sub_44C2D0` wrapper `sub_44C2D0`
- `instance SetPP(int, int)` func native `sub_44C330` wrapper `sub_44C330`
- `int AddPP(int, int)` func native `sub_44C390` wrapper `sub_44C390`
- `int GetLP(int)` func native `sub_44C400` wrapper `sub_44C400`
- `int SetLP(int, int)` func native `sub_44C440` wrapper `sub_44C440`
- `int AddLP(int, int)` func native `sub_44C480` wrapper `sub_44C480`
- `instance SetComboLimit(int)` func native `sub_44C4D0` wrapper `sub_44C4D0`
- `int SetWallCount(int)` func native `sub_44C520` wrapper `sub_44C520`
- `int AddAirCount(int, int)` func native `sub_44C580` wrapper `sub_44C580`
- `int SetAirJumpOK(int)` func native `sub_44C610` wrapper `sub_44C610`
- `int AddAirJumpCount(int)` func native `sub_44C640` wrapper `sub_44C640`
- `int TobiParam_Func(int)` func native `sub_44C660` wrapper `sub_44C660`
- `int TobiParamEndFlag_Func(int)` func native `sub_44C790` wrapper `sub_44C790`
- `int CheckPosState(int)` func native `sub_44C890` wrapper `sub_44C890`
- `int CheckPattern(int)` func native `sub_44C9F0` wrapper `sub_44C9F0`
- `int GetFrameParam(int)` func native `sub_44CB70` wrapper `sub_44CB70`
- `int GetFrameID()` func native `sub_44CB30` wrapper `sub_44CB30`
- `int JumpFrameID(int)` func native `sub_44D5D0` wrapper `sub_44D5D0`
- `int Frame_Proc(int, int)` func native `sub_44D630` wrapper `sub_44D630`
- `instance SetPrio(int)` func native `sub_44CBC0` wrapper `sub_44CBC0`
- `instance SetSpPrio(int)` func native `sub_44CC00` wrapper `sub_44CC00`
- `int SetCommandLongDelay(int)` func native `sub_44CCB0` wrapper `sub_44CCB0`
- `int CheckCommand(int)` func native `sub_44CE50` wrapper `sub_44CE50`
- `int CheckCommandString(int)` func native `sub_44D010` wrapper `sub_44D010`
- `int ClearCommand()` func native `sub_44D290` wrapper `sub_44D290`
- `int SetCommandFailTime(int)` func native `sub_44D2D0` wrapper `sub_44D2D0`
- `int CheckCommandFailTime()` func native `sub_44D480` wrapper `sub_44D480`
- `instance GetMvStatus(instance)` func native `sub_44B370` wrapper `sub_44B370`
- `int CheckMvUniqueID(int)` func native `sub_44B580` wrapper `sub_44B580`
- `int SetMvHitStatus(instance)` func native `sub_44B840` wrapper `sub_44B840`
- `int GetMvHitStatus(int)` func native `sub_44B6C0` wrapper `sub_44B6C0`
- `int ClearHitStatus()` func native `sub_44B950` wrapper `sub_44B950`
- `int ClearHitInterrupt()` func native `sub_44B980` wrapper `sub_44B980`
- `int SetHitStop(int, int)` func native `sub_44B900` wrapper `sub_44B900`
- `int SetVector(int)` func native `sub_44A7A0` wrapper `sub_44A7A0`
- `instance GetVector(instance)` func native `sub_44A990` wrapper `sub_44A990`
- `int SetVector_CCharaVector(int, int, int, int, int)` func native `sub_44A930` wrapper `sub_44A930`
- `int SetVector_MaxX(int)` func native `sub_44A8D0` wrapper `sub_44A8D0`
- `instance GetPosition(instance, int)` func native `sub_44AC00` wrapper `sub_44AC00`
- `int SetPosition(int)` func native `sub_44AD70` wrapper `sub_44AD70`
- `int MoveStartPosition()` func native `sub_44AF80` wrapper `sub_44AF80`
- `instance GetScreenPosition(instance)` func native `sub_44ACD0` wrapper `sub_44ACD0`
- `int SetAngle(int)` func native `sub_44AFD0` wrapper `sub_44AFD0`
- `int GetAngle()` func native `sub_44B240` wrapper `sub_44B240`
- `instance SetScale(int)` func native `sub_44B270` wrapper `sub_44B270`
- `instance GetScale(instance)` func native `sub_44B2F0` wrapper `sub_44B2F0`
- `int TalkScript_IsDone()` func native `sub_44D670` wrapper `sub_44D670`
- `instance GetMvRoundStatus(instance)` func native `sub_44D730` wrapper `sub_44D730`
- `int GetMvStageStatus(int)` func native `sub_44D690` wrapper `sub_44D690`
- `int SetRoundEnd()` func native `sub_44D930` wrapper `sub_44D930`
- `int SetRoundStart()` func native `sub_44D980` wrapper `sub_44D980`
- `instance SetRoundStartType(int)` func native `sub_44D9D0` wrapper `sub_44D9D0`
- `int SetMoveableFlag(int)` func native `sub_44DC00` wrapper `sub_44DC00`
- `int SetMoveableFlagEx(int)` func native `sub_44DCA0` wrapper `sub_44DCA0`
- `int SetAsFlag(int)` func native `sub_44DEC0` wrapper `sub_44DEC0`
- `int SetAsStatusFlag(int)` func native `sub_44DD70` wrapper `sub_44DD70`
- `int SetAsPosStatusFlag(int)` func native `sub_44DE30` wrapper `sub_44DE30`
- `int SetAtkGuardFlag(int)` func native `sub_44DF50` wrapper `sub_44DF50`
- `int SetAtkCatchFlag(int)` func native `sub_44E060` wrapper `sub_44E060`
- `int CalcHitValue(int)` func native `sub_44DFE0` wrapper `sub_44DFE0`
- `int GetRecoverStatus(int)` func native `sub_44D8E0` wrapper `sub_44D8E0`
- `int PcGauge_Set(int)` func native `dfci_cmdmvtable_load_sq` wrapper `dfci_cmdmvtable_load_sq`
- `int BoundCorrect_Calc(int)` func native `sub_44E300` wrapper `sub_44E300`
- `int ComboPoint_Calc(int)` func native `sub_44E3E0` wrapper `sub_44E3E0`
- `int UseSkill_Check(int)` func native `sub_44E4A0` wrapper `sub_44E4A0`
- `int HitPat_Check(int)` func native `sub_44E530` wrapper `sub_44E530`
- `int SetHitCheckFlag(int)` func native `sub_44E5F0` wrapper `sub_44E5F0`
- `int GetHitCheckFlag(int)` func native `sub_44E6A0` wrapper `sub_44E6A0`
- `int SetNoHoseiFlag(int)` func native `sub_44E6F0` wrapper `sub_44E6F0`
- `int SetNoUkemiTimeLimitFlag(int)` func native `sub_44E780` wrapper `sub_44E780`
- `int SetSuperArmorFlag(int)` func native `sub_44E810` wrapper `sub_44E810`
- `int SetVirtualGuardFlag(int)` func native `sub_44E8A0` wrapper `sub_44E8A0`
- `int SetCaptureHitFlag(int)` func native `sub_44E930` wrapper `sub_44E930`
- `int AddComboRate()` func native `sub_44E9C0` wrapper `sub_44E9C0`
- `int AddComboCount(int)` func native `sub_44EA40` wrapper `sub_44EA40`
- `int SetHan6LayerMask(int)` func native `sub_44EAC0` wrapper `sub_44EAC0`
- `int CheckBlast_IsNGStatus()` func native `sub_44EB90` wrapper `sub_44EB90`
- `int CheckBlast_BoundInit()` func native `sub_44EBD0` wrapper `sub_44EBD0`
- `int DamageFlag_Func(int, int)` func native `sub_44EC10` wrapper `sub_44EC10`
- `int DamageFlag_IsBound()` func native `sub_44EC50` wrapper `sub_44EC50`
- `int GetSelectColor()` func native `sub_44BA10` wrapper `sub_44BA10`
- `int GetCharaNo()` func native `sub_44BA50` wrapper `sub_44BA50`
- `? IsCpu()` func native `None` wrapper `None`
- `? GetCpuLevel_Sub()` func native `None` wrapper `None`
- `instance Achievement_Unlock(int)` func native `sub_44EC90` wrapper `sub_44EC90`

## CCharaVector (4)

- `? x()` method native `None` wrapper `None`
- `? y()` method native `None` wrapper `None`
- `? addx()` method native `None` wrapper `None`
- `? addy()` method native `None` wrapper `None`

## CCharaPosition (2)

- var `x` @0 (Sqrat_Class_VarA_453360)
- var `y` @4 (Sqrat_Class_VarA_453360)

## CCharaRect (4)

- `? sx()` method native `None` wrapper `None`
- `? sy()` method native `None` wrapper `None`
- `? ex()` method native `None` wrapper `None`
- `? ey()` method native `None` wrapper `None`

## CCharaPoint4 (8)

- `? x0()` method native `None` wrapper `None`
- `? y0()` method native `None` wrapper `None`
- `? x1()` method native `None` wrapper `None`
- `? y1()` method native `None` wrapper `None`
- `? x2()` method native `None` wrapper `None`
- `? y2()` method native `None` wrapper `None`
- `? x3()` method native `None` wrapper `None`
- `? y3()` method native `None` wrapper `None`

## MvStatus (18)

- `? CharaNo()` method native `None` wrapper `None`
- `? CallCount()` method native `None` wrapper `None`
- `? FrameCallCount()` method native `None` wrapper `None`
- `? isUpdate()` method native `None` wrapper `None`
- var `MvCount` @8 (Sqrat_Class_VarA_4534E0)
- var `MvCountFrame` @12 (Sqrat_Class_VarA_4534E0)
- `? isFrameUpdate()` method native `None` wrapper `None`
- `? isLanding()` method native `None` wrapper `None`
- `? DataPattern()` method native `None` wrapper `None`
- `? DataFrame()` method native `None` wrapper `None`
- `? DataDelayVal()` method native `None` wrapper `None`
- `? DataDelay()` method native `None` wrapper `None`
- `? flags()` method native `None` wrapper `None`
- `? FrameID()` method native `None` wrapper `None`
- `? Param0()` method native `None` wrapper `None`
- `? Param1()` method native `None` wrapper `None`
- `? Param2()` method native `None` wrapper `None`
- `? Param3()` method native `None` wrapper `None`

## MvFrameStatus (5)

- `? FrameID()` method native `None` wrapper `None`
- `? Param0()` method native `None` wrapper `None`
- `? Param1()` method native `None` wrapper `None`
- `? Param2()` method native `None` wrapper `None`
- `? Param3()` method native `None` wrapper `None`

## MvHitStatus (8)

- `? Type()` method native `None` wrapper `None`
- `? PosState()` method native `None` wrapper `None`
- `? isDown()` method native `None` wrapper `None`
- `? isFirstUpdate()` method native `None` wrapper `None`
- `? isCatchFlag()` method native `None` wrapper `None`
- `? isMoveable()` method native `None` wrapper `None`
- `? Count()` method native `None` wrapper `None`
- `int IsDown()` func native `sub_42AD10` wrapper `sub_42AD10`

## MvHitImpactInfo (8)

- `? type()` method native `None` wrapper `None`
- `? d_state()` method native `None` wrapper `None`
- `? atk_guard()` method native `None` wrapper `None`
- `? atk_throw()` method native `None` wrapper `None`
- `? def_guard()` method native `None` wrapper `None`
- `? grd_break()` method native `None` wrapper `None`
- `? counterhit()` method native `None` wrapper `None`
- var `flags` @16 (Sqrat_Class_VarA_4536A0)

## MvInterruptInfo (6)

- `? type()` method native `None` wrapper `None`
- `? Param0()` method native `None` wrapper `None`
- `? Param1()` method native `None` wrapper `None`
- `? Param2()` method native `None` wrapper `None`
- `? Param3()` method native `None` wrapper `None`
- `? Param4()` method native `None` wrapper `None`

## MvRecoverStatus (2)

- var `Type` @0 (Sqrat_Class_VarA_453760)
- var `Stick` @4 (Sqrat_Class_VarA_453760)

## MvRoundStatus (10)

- `? Time()` method native `None` wrapper `None`
- `? TimeUpdate()` method native `None` wrapper `None`
- `? Round()` method native `None` wrapper `None`
- `? WinType()` method native `None` wrapper `None`
- `? isKo()` method native `None` wrapper `None`
- `? isMyKo()` method native `None` wrapper `None`
- `? isWinRound()` method native `None` wrapper `None`
- `? isLoseRound()` method native `None` wrapper `None`
- `? isFinalRound()` method native `None` wrapper `None`
- `? CharaMoveMode()` method native `None` wrapper `None`

## MvStageStatus (4)

- `int IsStoryBattle()` func native `sub_80F500` wrapper `sub_80F500`
- `int GetCpuChara()` func native `sub_80F4D0` wrapper `sub_80F4D0`
- `int GetPcChara()` func native `sub_80F4B0` wrapper `sub_80F4B0`
- `int GetStartSide()` func native `sub_4324D0` wrapper `sub_4324D0`

## BMvIf (1)

- `int StdCall(int)` func native `sub_44DA20` wrapper `sub_44DA20`

## BSound (4)

- `instance SE_Play(int)` func native `sub_4490D0` wrapper `sub_4490D0`
- `? SE_Stop()` func native `None` wrapper `None`
- `void BGM_Mute()` func native `sub_449340` wrapper `sub_449340`
- `int BGM_SetNum(int)` func native `sub_449350` wrapper `sub_449350`

## ProcTimer (7)

- `int ReStart()` func native `sub_448D30` wrapper `sub_448D30`
- `int SetName(int)` func native `sub_448390` wrapper `sub_448390`
- `float GetTime()` method native `sub_448450` wrapper `sub_448450`
- `void PrintTime_Int()` func native `sub_811B50` wrapper `sub_811B50`
- `int GetNowTime()` method native `sub_448D50` wrapper `sub_448D50`
- `void PrintNowTime_Int()` func native `sub_811B60` wrapper `sub_811B60`
- `instance EndPrint_Off()` func native `sub_4485A0` wrapper `sub_4485A0`

## CCheckRect (7)

- `instance Reset()` func native `sub_811E60` wrapper `sub_811E60`
- `instance SetPlayer()` func native `sub_812270` wrapper `sub_812270`
- `instance SetSrc(int)` method native `sub_811FB0` wrapper `sub_811FB0`
- `instance SetDst(int)` method native `sub_811F10` wrapper `sub_811F10`
- `instance SetFlags(int)` func native `sub_41CAB0` wrapper `sub_41CAB0`
- `int Check()` func native `sub_812060` wrapper `sub_812060`
- `instance GetCharaData(instance)` method native `sub_811E10` wrapper `sub_811E10`

## BMvEff (114)

- `int Random()` func native `sub_442FE0` wrapper `sub_442FE0`
- `? Random_F()` func native `sub_442FF0` wrapper `sub_442FF0`
- `int Random_Limit(int)` func native `sub_443010` wrapper `sub_443010`
- `float Random_FAbs(int)` func native `sub_443030` wrapper `sub_443030`
- `instance Random_PointRect(instance, int)` func native `sub_443120` wrapper `sub_443120`
- `? Random_PointRad()` func native `sub_443060` wrapper `sub_443060`
- `int StdCall(int)` func native `sub_440120` wrapper `sub_440120`
- `int ThrowRelease(int)` func native `sub_440B10` wrapper `sub_440B10`
- `int ThrowParam(int)` func native `sub_4403B0` wrapper `sub_4403B0`
- `int ThrowChara_SetJoint(int)` func native `sub_4407D0` wrapper `sub_4407D0`
- `int ThrowChara_SetCamera(int)` func native `sub_4408A0` wrapper `sub_4408A0`
- `int ThrowChara_Transfer(int)` func native `sub_4409E0` wrapper `sub_4409E0`
- `int CapturePlayer(int)` func native `sub_440D50` wrapper `sub_440D50`
- `int GetFrameIDNum(int)` func native `sub_441840` wrapper `sub_441840`
- `int GetPatternNum(int)` func native `sub_441770` wrapper `sub_441770`
- `instance CreateObject(instance, int)` func native `sub_441490` wrapper `sub_441490`
- `int SetObjectFlags(int)` func native `sub_441880` wrapper `sub_441880`
- `void SetObjectRender(int)` func native `sub_441C80` wrapper `sub_441C80`
- `int EraseObjectFlags(int)` func native `sub_4418F0` wrapper `sub_4418F0`
- `int ObjProcFlags_Set(int)` func native `sub_441960` wrapper `sub_441960`
- `int ObjProcFlags_Erase(int)` func native `sub_4419E0` wrapper `sub_4419E0`
- `int ObjType_Set(int)` func native `sub_441A60` wrapper `sub_441A60`
- `int SetCamera_Quake(int)` func native `sub_441D60` wrapper `sub_441D60`
- `int SetCamera_Focus(int)` func native `sub_441E00` wrapper `sub_441E00`
- `instance SetCamera_FocusKeep(int)` func native `sub_442090` wrapper `sub_442090`
- `instance SetCamera_Clipping(int)` func native `sub_4420B0` wrapper `sub_4420B0`
- `int SetCamera_UpdateFlag(int, int)` func native `sub_4420D0` wrapper `sub_4420D0`
- `int ResetCamera()` func native `sub_4420F0` wrapper `sub_4420F0`
- `int UpdateCamera()` func native `sub_442100` wrapper `sub_442100`
- `int ResetViewCamera()` func native `sub_442110` wrapper `sub_442110`
- `instance CameraShift_Set(int)` func native `sub_442160` wrapper `sub_442160`
- `int CameraShift_Clear()` func native `sub_442220` wrapper `sub_442220`
- `int SetStopTime(int)` func native `sub_442260` wrapper `sub_442260`
- `int ForceKO()` func native `sub_442390` wrapper `sub_442390`
- `int GetKOMode()` func native `sub_442420` wrapper `sub_442420`
- `int GetKOFadeOutTime()` func native `sub_442460` wrapper `sub_442460`
- `int IsKOSkip()` func native `sub_442480` wrapper `sub_442480`
- `int SetNoDamageFlag(int)` func native `sub_4424A0` wrapper `sub_4424A0`
- `int SetHpGauge(int)` func native `dfci_sq_native_sethpgauge` wrapper `dfci_sq_native_sethpgauge`
- `int SetSpGauge(int)` func native `sub_442810` wrapper `sub_442810`
- `int SpGauge_SetLimit(int)` func native `sub_442900` wrapper `sub_442900`
- `int SpGauge_SetLimitCombo(int)` func native `sub_442970` wrapper `sub_442970`
- `int SpGauge_GetValue(int)` func native `sub_812350` wrapper `sub_812350`
- `int SpGauge_GetPlusCorrect_ComboTimePos()` func native `sub_442A00` wrapper `sub_442A00`
- `instance SetExist(int)` func native `sub_441AE0` wrapper `sub_441AE0`
- `instance SetPaletteSlot(int, int)` func native `sub_441BB0` wrapper `sub_441BB0`
- `int GetPaletteSlot()` func native `sub_441C50` wrapper `sub_441C50`
- `instance SetOtherPalette(int, int)` func native `sub_441C00` wrapper `sub_441C00`
- `instance GetPointStatus(instance, int)` func native `sub_442B00` wrapper `sub_442B00`
- `instance GetVector_FromAngle(instance, int)` func native `sub_442E40` wrapper `sub_442E40`
- `instance GetAngle_FromVector(instance, int)` func native `sub_442F20` wrapper `sub_442F20`
- `int SetBoundSt(int)` func native `sub_440E40` wrapper `sub_440E40`
- `instance CheckHantei(instance, int)` func native `sub_440F50` wrapper `sub_440F50`
- `instance GetHanteiRect(instance, int)` func native `sub_441200` wrapper `sub_441200`
- `int IsAttackRect()` func native `sub_441420` wrapper `sub_441420`
- `instance GetCameraPosition(instance, int)` func native `sub_443260` wrapper `sub_443260`
- `instance GetCameraRect(instance, int)` func native `sub_4433C0` wrapper `sub_4433C0`
- `int SetPlayerTimer(int)` func native `sub_4435A0` wrapper `sub_4435A0`
- `int GetPlayerMuteki(int)` func native `sub_4436B0` wrapper `sub_4436B0`
- `int AddAttackHitNum(int)` func native `sub_443740` wrapper `sub_443740`
- `int ClearAttackHitNum()` func native `sub_443820` wrapper `sub_443820`
- `instance GetAttackHitPos(instance)` func native `sub_4437A0` wrapper `sub_4437A0`
- `int Liberate_OK()` func native `sub_443860` wrapper `sub_443860`
- `int Liberate_Set()` func native `sub_4438C0` wrapper `sub_4438C0`
- `int Liberate_Get()` func native `sub_443970` wrapper `sub_443970`
- `int Liberate_Calc(int)` func native `sub_4439D0` wrapper `sub_4439D0`
- `int Liberate_End()` func native `sub_443940` wrapper `sub_443940`
- `int GRD_UseStock()` func native `sub_443A80` wrapper `sub_443A80`
- `int GRD_CheckStock()` func native `sub_443A60` wrapper `sub_443A60`
- `int GRD_GetJudgeResult()` func native `sub_443A60` wrapper `sub_443A60`
- `void GRD_SetBreak()` func native `sub_449340` wrapper `sub_449340`
- `int GRD_GetBreak()` func native `sub_443A60` wrapper `sub_443A60`
- `void GRD_CorrectBreakTime()` func native `sub_449340` wrapper `sub_449340`
- `int GRD_ClearBreak()` func native `sub_443A60` wrapper `sub_443A60`
- `int GRD_SetConvertBreak()` func native `sub_443A60` wrapper `sub_443A60`
- `int GRD_GetConvertCount()` func native `sub_4043D0` wrapper `sub_4043D0`
- `int GRD_AddValue()` func native `sub_443A80` wrapper `sub_443A80`
- `int GuardSP_Set(int)` func native `sub_443C60` wrapper `sub_443C60`
- `int GuardSP_Get()` func native `sub_443CD0` wrapper `sub_443CD0`
- `int GuardSP_Success()` func native `sub_443D10` wrapper `sub_443D10`
- `instance AttackInfoString_Set(int)` func native `sub_443D50` wrapper `sub_443D50`
- `int FadeProc_Set(int)` func native `dfci_script_cmd_stage_flash` wrapper `dfci_script_cmd_stage_flash`
- `int FadeProc_Erase(int)` func native `dfci_script_cmd_stage_flash_type` wrapper `dfci_script_cmd_stage_flash_type`
- `int CutInProc_Set(int)` func native `dfci_script_cmd_stage_flash_time` wrapper `dfci_script_cmd_stage_flash_time`
- `int CutInProc_Erase()` func native `sub_444320` wrapper `sub_444320`
- `int SetCharaFlash(int)` func native `sub_444360` wrapper `sub_444360`
- `int SetCharaColor(int)` func native `sub_444430` wrapper `sub_444430`
- `int SetStageCharaLight(int)` func native `sub_444500` wrapper `sub_444500`
- `int SetCharaDrawType(int)` func native `sub_444560` wrapper `sub_444560`
- `int PcAfterImage_Set(int)` func native `sub_4445C0` wrapper `sub_4445C0`
- `int PcAfterImage_Clear()` func native `sub_4446E0` wrapper `sub_4446E0`
- `int PcAuraEffect_Set(int)` func native `sub_444730` wrapper `sub_444730`
- `int PcAuraEffect_Clear()` func native `sub_444A50` wrapper `sub_444A50`
- `int Slowmotion_Set(int)` func native `sub_444AB0` wrapper `sub_444AB0`
- `int LinePrim_Set(int)` func native `dfci_btlsq_effobject_load` wrapper `dfci_btlsq_effobject_load`
- `int LinePrim_SetTypeParam(int)` func native `sub_444E80` wrapper `sub_444E80`
- `int Cockpit_SetView(int)` func native `sub_445090` wrapper `sub_445090`
- `int ComboView_Clear()` func native `sub_445120` wrapper `sub_445120`
- `int ComboView_Set(int)` func native `sub_445170` wrapper `sub_445170`
- `int ChangeStartCorrectTiming()` func native `sub_445210` wrapper `sub_445210`
- `int ChangeStartCorrectTimingPlus()` func native `sub_445270` wrapper `sub_445270`
- `void BGM_Set(int)` func native `sub_4452B0` wrapper `sub_4452B0`
- `int BG_GetNum()` func native `sub_445310` wrapper `sub_445310`
- `int AddSkillCount(int)` func native `sub_443A90` wrapper `sub_443A90`
- `int GetEntranceCallCount()` func native `sub_445330` wrapper `sub_445330`
- `int IsReplayEntrance()` func native `sub_445350` wrapper `sub_445350`
- `int GetMyObjectCount()` func native `sub_445370` wrapper `sub_445370`
- `instance SetGuardPlusFlag(int)` func native `sub_4453B0` wrapper `sub_4453B0`
- `int Chara_SetMessage(int, int, int, int, int)` func native `sub_445580` wrapper `sub_445580`
- `int Chara_SetMessagePC(int, int, int, int, int)` func native `sub_4455E0` wrapper `sub_4455E0`
- `instance Chara_SetMessageDelay(int)` func native `sub_445640` wrapper `sub_445640`
- `int Chara_JoinMessage(int)` func native `sub_4456A0` wrapper `sub_4456A0`
- `int Chara_ClearMessageAll()` func native `sub_445720` wrapper `sub_445720`
- `int SetAllKeyCheck(int)` func native `sub_445790` wrapper `sub_445790`

## CPointStatus (6)

- var `distance` @0 (Sqrat_Class_VarD_446CE0)
- var `distance_x` @4 (Sqrat_Class_VarD_446CE0)
- var `distance_y` @8 (Sqrat_Class_VarD_446CE0)
- var `pos_x` @12 (Sqrat_Class_VarD_446CE0)
- var `pos_y` @16 (Sqrat_Class_VarD_446CE0)
- var `angle` @20 (Sqrat_Class_VarA_446D40)

## tDDC (62)

- `int Kirifuda_Add(int)` func native `sub_43D8D0` wrapper `sub_43D8D0`
- `int Kirifuda_AddEx(int)` func native `sub_43D940` wrapper `sub_43D940`
- `int Kirifuda_GetValue()` func native `sub_43DD60` wrapper `sub_43DD60`
- `int Kirifuda_GetValueEx()` func native `sub_43DDB0` wrapper `sub_43DDB0`
- `int Kirifuda_GetMax()` func native `sub_43DE10` wrapper `sub_43DE10`
- `int Kirifuda_TimeAdd(int, int)` func native `sub_43DA10` wrapper `sub_43DA10`
- `int Kirifuda_CheckTime()` func native `sub_43DB80` wrapper `sub_43DB80`
- `int Kirifuda_CheckStatus()` func native `sub_43DBD0` wrapper `sub_43DBD0`
- `string Kirifuda_SetTimeValue(int, int)` func native `sub_43DC20` wrapper `sub_43DC20`
- `int Kirifuda_GetTime()` func native `sub_43DC70` wrapper `sub_43DC70`
- `string Kirifuda_SetInstallCount(int)` func native `sub_43DCC0` wrapper `sub_43DCC0`
- `string Kirifuda_TimeDamageMinus(int)` func native `sub_43DD10` wrapper `sub_43DD10`
- `int Kirifuda_FinalizeProc()` func native `sub_43DAB0` wrapper `sub_43DAB0`
- `int Kirifuda_TimeClear()` func native `sub_43DB30` wrapper `sub_43DB30`
- `int Kirifuda_SubMax()` func native `sub_43D9B0` wrapper `sub_43D9B0`
- `string KirifudaWatch_SetEndPalette(int, int)` func native `sub_43DE60` wrapper `sub_43DE60`
- `string KirifudaWatch_SetParam(int, int)` func native `sub_43DEB0` wrapper `sub_43DEB0`
- `int KirifudaWatch_GetParam(int)` func native `sub_43DF00` wrapper `sub_43DF00`
- `string Blast_AddPoint(int)` func native `sub_43DF50` wrapper `sub_43DF50`
- `int Blast_IsUsableMode()` func native `sub_43DFB0` wrapper `sub_43DFB0`
- `int Blast_IsUse()` func native `sub_43E010` wrapper `sub_43E010`
- `int Blast_Use(int)` func native `sub_43E070` wrapper `sub_43E070`
- `int Blast_UseEx(int)` func native `sub_43E0E0` wrapper `sub_43E0E0`
- `int Blast_SetParam(int)` func native `sub_43E2E0` wrapper `sub_43E2E0`
- `int Blast_SetFailed(int)` func native `sub_43E3D0` wrapper `sub_43E3D0`
- `int Blast_GetUseMode()` func native `sub_43E150` wrapper `sub_43E150`
- `int Blast_CheckClimaxMode()` func native `sub_43E1C0` wrapper `sub_43E1C0`
- `int Blast_EndClimaxMode()` func native `sub_43E270` wrapper `sub_43E270`
- `int Blast_SetClimaxTime(int, int)` func native `sub_43E210` wrapper `sub_43E210`
- `string Blast_AddPowerTime(int)` func native `sub_43E440` wrapper `sub_43E440`
- `int Support_IsUse()` func native `sub_43E490` wrapper `sub_43E490`
- `int Support_Use()` func native `sub_43E4F0` wrapper `sub_43E4F0`
- `int Support_SetFailed(int)` func native `sub_43E590` wrapper `sub_43E590`
- `int Support_StartRecast(int)` func native `sub_43E5F0` wrapper `sub_43E5F0`
- `string Support_SetPowerupRecastMax(int)` func native `sub_43E6A0` wrapper `sub_43E6A0`
- `int Support_GetCallCount()` func native `sub_43E6F0` wrapper `sub_43E6F0`
- `int Support_GetRecastMode()` func native `sub_43E650` wrapper `sub_43E650`
- `int Support_AddRecastValue(int)` func native `sub_43E7A0` wrapper `sub_43E7A0`
- `int Support_SetRecastCost(int)` func native `sub_43E740` wrapper `sub_43E740`
- `int Support_GetRecastCost()` func native `sub_43E800` wrapper `sub_43E800`
- `int Support_GetType()` func native `sub_43E8F0` wrapper `sub_43E8F0`
- `int Support_GetCharaNo()` func native `sub_43E8A0` wrapper `sub_43E8A0`
- `int Support_GetSelectColor()` func native `sub_43E850` wrapper `sub_43E850`
- `int Support_SetMessage(int, int, int)` func native `sub_43E980` wrapper `sub_43E980`
- `instance ExJem_Attack(int, int)` func native `sub_43EAF0` wrapper `sub_43EAF0`
- `instance Jem_AddCount(int)` func native `sub_43EB50` wrapper `sub_43EB50`
- `int Jem_GetRunFlag(int)` func native `sub_43EBA0` wrapper `sub_43EBA0`
- `int Jem_IsWorking(int)` func native `sub_43EC10` wrapper `sub_43EC10`
- `int Jem_IsWorkingEff(int)` func native `sub_43EC80` wrapper `sub_43EC80`
- `instance Jem_WorkingPlus(int, int)` func native `sub_43ECE0` wrapper `sub_43ECE0`
- `int Jem_SetMessage(int, int)` func native `sub_43EF30` wrapper `sub_43EF30`
- `int Jem_SetMessageEx(int, int, int, int)` func native `sub_43EEE0` wrapper `sub_43EEE0`
- `int HP_Add(int)` func native `sub_43EF80` wrapper `sub_43EF80`
- `int HP_SourceDamMode(int)` func native `sub_43F040` wrapper `sub_43F040`
- `instance SkillCountPlus(int)` func native `sub_43F0D0` wrapper `sub_43F0D0`
- `int AchievementSousai()` func native `sub_43F110` wrapper `sub_43F110`
- `int PowerUpSys_GetCount(int)` func native `sub_43F1D0` wrapper `sub_43F1D0`
- `int PowerUpSys_GetLast()` func native `sub_43F120` wrapper `sub_43F120`
- `int PowerUpSys_IsDouble(int)` func native `sub_43F170` wrapper `sub_43F170`
- `int SubSp_GetChara()` func native `sub_43F230` wrapper `sub_43F230`
- `int SubSp_GetType()` func native `sub_43F280` wrapper `sub_43F280`
- `int GetMiyukiPalNum()` func native `sub_8138B0` wrapper `sub_8138B0`

## BCom (3)

- `int Move_Jump(int)` func native `sub_42EC20` wrapper `sub_42EC20`
- `int Move_Next()` func native `sub_42ECB0` wrapper `sub_42ECB0`
- `int Move_End(int)` func native `sub_42ECF0` wrapper `sub_42ECF0`

## UpdateSt (2)

- `? GetELenX()` func native `None` wrapper `None`
- `? GetELenY()` func native `None` wrapper `None`

## BMvCore (17)

- `instance GetMyCharaData(instance)` func native `sub_43BD80` wrapper `sub_43BD80`
- `instance GetCaptureCharaData(instance)` func native `sub_43BDE0` wrapper `sub_43BDE0`
- `instance GetParentCharaData(instance)` func native `sub_43BE50` wrapper `sub_43BE50`
- `instance GetPlayerCharaData(instance)` func native `sub_43BEC0` wrapper `sub_43BEC0`
- `instance GetNearEnemyCharaData(instance)` func native `sub_43BF30` wrapper `sub_43BF30`
- `instance GetEnemyCharaData(instance, int)` func native `sub_43BFA0` wrapper `sub_43BFA0`
- `instance GetLastDamageCharaData(instance, int)` func native `sub_43C210` wrapper `sub_43C210`
- `instance SetLastDamageCharaData(int)` func native `sub_43C1D0` wrapper `sub_43C1D0`
- `instance GetLastHitCharaData(instance, int)` func native `sub_43C2A0` wrapper `sub_43C2A0`
- `instance GetLastCatchCharaData(instance, int)` func native `sub_43C330` wrapper `sub_43C330`
- `instance GetSupportCharaData(instance)` func native `sub_43C3C0` wrapper `sub_43C3C0`
- `instance GetMainCharaData(instance)` func native `sub_43C440` wrapper `sub_43C440`
- `instance GetNearEnemySupportData(instance)` func native `sub_43C070` wrapper `sub_43C070`
- `instance GetEnemySupportData(instance, int)` func native `sub_43C0F0` wrapper `sub_43C0F0`
- `int CallEntryBCCachePreTransfer()` func native `sub_443A80` wrapper `sub_443A80`
- `int PushCharaData(instance)` func native `sub_43C4C0` wrapper `sub_43C4C0`
- `int PopCharaData()` func native `sub_43C520` wrapper `sub_43C520`

## CCharaData (5)

- var `IsDone` @0 (Sqrat_Class_VarA_43CFB0)
- `int push()` func native `sub_43C530` wrapper `sub_43C530`
- `? isdone()` func native `None` wrapper `None`
- `int isPlayer()` func native `sub_43C570` wrapper `sub_43C570`
- var `data` @4 (Sqrat_Class_VarA_43D010)

## CFindObject (1)

- `int SetFirst()` func native `sub_43BD10` wrapper `sub_43BD10`

## CDat (2)

- `int Load(int)` func native `sub_8948A0` wrapper `sub_8948A0`
- `int SetPosition(int, int)` method native `sub_4191F0` wrapper `sub_4191F0`

## CAnime (25)

- `instance SetCDat(int)` method native `sub_45B120` wrapper `sub_45B120`
- `int AddFrame(int, float)` method native `sub_80AD70` wrapper `sub_80AD70`
- `int AddFrameEx(int, float, int)` method native `sub_80ACC0` wrapper `sub_80ACC0`
- `int AddJump(int)` func native `sub_80ACA0` wrapper `sub_80ACA0`
- `int AddModeJump(int, int, int)` method native `sub_80AB00` wrapper `sub_80AB00`
- `int AddTagJump(int)` func native `sub_80AC80` wrapper `sub_80AC80`
- `int AddModeTagJump(int, int, int)` method native `sub_80AA60` wrapper `sub_80AA60`
- `int AddTag(int)` func native `sub_80ABE0` wrapper `sub_80ABE0`
- `int AddSe(int)` func native `sub_80A990` wrapper `sub_80A990`
- `int AddModeTag(int)` func native `sub_80A920` wrapper `sub_80A920`
- `int AddEnd(int)` func native `sub_80A8B0` wrapper `sub_80A8B0`
- `? SetPrio()` func native `None` wrapper `None`
- `instance SetMode(int)` func native `sub_41CAD0` wrapper `sub_41CAD0`
- `int GetMode()` func native `sub_42DE90` wrapper `sub_42DE90`
- `instance AddMode(int)` func native `sub_4591D0` wrapper `sub_4591D0`
- `int Jump(int)` func native `sub_80A7F0` wrapper `sub_80A7F0`
- `int TagJump(int)` func native `sub_80B100` wrapper `sub_80B100`
- `int Reset()` func native `sub_419340` wrapper `sub_419340`
- `float ResetFrameTime()` func native `sub_4193B0` wrapper `sub_4193B0`
- `int IsNull()` func native `sub_4591B0` wrapper `sub_4591B0`
- `instance SetView(int)` func native `sub_41CAB0` wrapper `sub_41CAB0`
- `int GetView()` func native `sub_42DE70` wrapper `sub_42DE70`
- `int ClearFrame()` func native `sub_80A3B0` wrapper `sub_80A3B0`
- `float SetStartWait(float)` method native `sub_430DA0` wrapper `sub_430DA0`
- `int SetPosition(int, int)` method native `sub_80A240` wrapper `sub_80A240`

## CAniManager (6)

- `int Render()` func native `sub_419560` wrapper `sub_419560`
- `int Update()` func native `sub_4194F0` wrapper `sub_4194F0`
- `int Reset()` func native `sub_80A350` wrapper `sub_80A350`
- `? Finalize()` func native `None` wrapper `None`
- `int SetMode(int)` func native `sub_419770` wrapper `sub_419770`
- `instance GetCAnime(int)` func native `sub_459200` wrapper `sub_459200`

