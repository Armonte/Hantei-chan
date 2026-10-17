int __usercall sub_57F480@<eax>(_DWORD *a1@<edx>, int a2@<ecx>, int a3@<ebp>)
{
  int v3; // edi
  _DWORD *v4; // esi
  int v5; // edx
  int v6; // eax
  int v7; // eax
  int v8; // eax
  int v9; // ecx
  int v10; // eax
  int v11; // ecx
  int v12; // edi
  int v13; // edx
  int v14; // edx
  int v15; // eax
  int v16; // edx
  char v17; // al
  char v18; // cl
  int v19; // esi
  int v20; // eax
  int v21; // esi
  int v22; // edx
  int v23; // esi
  int v24; // eax
  char v25; // al
  int v26; // eax
  bool v27; // zf
  char v28; // cl
  _BYTE *v29; // esi
  char v30; // cl
  int v31; // edx
  int v32; // ecx
  int v33; // eax
  int v34; // ecx
  int v35; // eax
  int v36; // edx
  int v37; // eax
  int v38; // ecx
  int v39; // eax
  int v40; // ecx
  int v41; // edx
  int v42; // ecx
  int v43; // eax
  int v44; // edx
  int v45; // ecx
  int v46; // edx
  int v47; // eax
  int v48; // ecx
  int v49; // eax
  _DWORD *v50; // ecx
  int v51; // et2
  int v52; // edx
  int v53; // ecx
  int v54; // eax
  double v55; // xmm2_8
  float v56; // xmm1_4
  int v57; // ecx
  int v58; // eax
  double v59; // xmm1_8
  float v60; // xmm0_4
  int v61; // eax
  float v62; // xmm0_4
  int v63; // ecx
  int v64; // ecx
  int v65; // eax
  int v66; // edx
  int v67; // edx
  int v68; // eax
  int v69; // ecx
  int v70; // eax
  int v71; // ecx
  int v72; // esi
  int v73; // et2
  int v74; // edx
  int v75; // edx
  int v76; // ecx
  int v77; // eax
  int v78; // ecx
  int v79; // ecx
  char *v80; // ecx
  int IsCurrentPlayerObject; // eax
  int v82; // eax
  float v83; // xmm0_4
  int v84; // eax
  int v85; // edx
  BOOL v86; // ecx
  int v87; // eax
  char v88; // dl
  int v89; // eax
  char v90; // dl
  int v91; // ecx
  int v92; // esi
  int v93; // ecx
  int v94; // esi
  int v95; // ecx
  int v96; // ecx
  int v97; // eax
  int v98; // esi
  int v99; // ecx
  int v100; // edx
  int v101; // edx
  int v102; // ecx
  int v103; // esi
  int v104; // eax
  int v105; // esi
  int v106; // eax
  int v107; // eax
  int v108; // edi
  int v109; // eax
  int v110; // ecx
  int v111; // edx
  int v112; // ecx
  int v113; // eax
  int v114; // edx
  int v115; // ecx
  int v116; // edx
  int v117; // eax
  int v118; // ecx
  int v119; // eax
  _DWORD *v120; // ecx
  int v121; // edx
  bool v122; // sf
  int v123; // edx
  int v124; // eax
  int v125; // edx
  int v126; // ecx
  int v127; // eax
  int v128; // esi
  int v129; // ecx
  int v130; // eax
  int v131; // eax
  int v132; // eax
  int v133; // eax
  int v134; // ecx
  float v135; // xmm0_4
  unsigned __int8 v136; // al
  unsigned __int8 v137; // cl
  int v138; // ecx
  __int64 v139; // rax
  int v140; // eax
  int v141; // edx
  int v142; // ecx
  int v143; // eax
  int v144; // eax
  int v145; // ecx
  int v146; // eax
  float v147; // xmm1_4
  float v148; // xmm1_4
  float v149; // xmm0_4
  int v150; // eax
  int v151; // esi
  int v152; // edx
  __int16 *v153; // eax
  int v154; // eax
  char v155; // al
  int v156; // eax
  int v157; // eax
  char v158; // dl
  int v159; // eax
  int v160; // ecx
  int v161; // eax
  int v162; // ecx
  int v163; // eax
  int v164; // ecx
  int v165; // eax
  int v166; // eax
  float v167; // xmm0_4
  int v168; // eax
  int v169; // ecx
  int v170; // eax
  int v171; // eax
  int v173; // [esp+10h] [ebp-F8h] BYREF
  int v174; // [esp+14h] [ebp-F4h]
  int v175; // [esp+18h] [ebp-F0h] BYREF
  int v176; // [esp+1Ch] [ebp-ECh] BYREF
  _DWORD *v177; // [esp+20h] [ebp-E8h]
  __int64 v178; // [esp+24h] [ebp-E4h] BYREF
  double v179; // [esp+2Ch] [ebp-DCh] BYREF
  int v180; // [esp+34h] [ebp-D4h]
  int v181; // [esp+38h] [ebp-D0h]
  int v182; // [esp+3Ch] [ebp-CCh] BYREF
  _BYTE v183[64]; // [esp+7Ch] [ebp-8Ch] BYREF
  double v184; // [esp+BCh] [ebp-4Ch] BYREF
  int v185; // [esp+C4h] [ebp-44h]
  _DWORD v186[4]; // [esp+CCh] [ebp-3Ch] BYREF
  _DWORD v187[2]; // [esp+DCh] [ebp-2Ch] BYREF
  int v188; // [esp+E4h] [ebp-24h] BYREF
  int v189; // [esp+E8h] [ebp-20h] BYREF
  double v190; // [esp+ECh] [ebp-1Ch] BYREF
  int v191; // [esp+FCh] [ebp-Ch]
  void *v192; // [esp+100h] [ebp-8h]
  void *retaddr; // [esp+108h] [ebp+0h]

  v191 = a3;
  v192 = retaddr;
  v3 = a2;
  v4 = a1;
  v181 = (int)a1;
  v5 = 1;
  if ( *(_DWORD *)(a2 + 1196) != 0 )
    a2 = *(_DWORD *)(a2 + 1196);
  v6 = v4[1];
  v180 = a2;
  if ( v6 > 300 )
  {
    if ( v6 > 311 )
    {
      if ( v6 == 1100 )
      {
        v171 = v4[2];
        if ( v171 != 0 )
        {
          if ( v171 == 1 )
          {
            *(_DWORD *)(v3 + 472) = -*(_DWORD *)(v3 + 472);
            *(_DWORD *)(v3 + 476) = -*(_DWORD *)(v3 + 476);
          }
        }
        else
        {
LABEL_120:
          *(_DWORD *)(v3 + 464) = -*(_DWORD *)(v3 + 464);
          *(_DWORD *)(v3 + 468) = -*(_DWORD *)(v3 + 468);
        }
      }
    }
    else if ( v6 == 311 )
    {
      if ( *(int *)(v3 + 8) >= 240 )
      {
        v168 = v4[4] | (v4[5] << 16);
        if ( (v4[4] & 4) != 0 )
        {
          v169 = *(_DWORD *)(v3 + 124);
          if ( (v169 & 4) != 0 )
            *(_DWORD *)(v3 + 124) = v169 & 0xFFFFFFFB;
        }
        v170 = v168 & 0x30BDF069;
        if ( v170 != 0 )
          *(_DWORD *)(v3 + 124) &= ~v170;
      }
    }
    else
    {
      v165 = v6 - 301;
      if ( v165 != 0 )
      {
        v166 = v165 - 1;
        if ( v166 != 0 )
        {
          if ( v166 == 8 && *(int *)(v3 + 8) >= 240 )
            sub_581A90((_DWORD *)v3, v4[4] | (v4[5] << 16));
        }
        else
        {
          dword_A2F508 = v4[2] != 0;
        }
      }
      else
      {
        v27 = v4[8] == 0;
        v186[0] = 0;
        v186[1] = 0;
        if ( v27 )
        {
          v167 = (float)(int)v4[7];
          v187[0] = *(_DWORD *)(v3 + 100) + (v4[2] << 7);
          v187[1] = *(_DWORD *)(v3 + 104) + (v4[3] << 7);
          *(float *)&v188 = v167 / 10000.0;
        }
        else
        {
          *(float *)&v188 = 0.0;
          v187[0] = -12345678;
          v187[1] = -12345678;
        }
        v189 = v4[4];
        v190 = *(double *)(v4 + 5);
        v186[3] = 0;
        v186[2] = 0;
        sub_509A80(v186);
      }
    }
  }
  else if ( v6 == 300 )
  {
    v160 = v4[12];
    if ( (v160 & 1) != 0 )
    {
      v161 = v4[2];
      if ( (v160 & 4) == 0 && *(_BYTE *)(v3 + 1772) == 1 )
        v161 = -v161;
      v162 = v4[13];
      if ( v162 == 0 )
      {
        *(_DWORD *)(v3 + 456) = v161;
        v162 = v4[13];
      }
      if ( v162 == 1 )
        *(_DWORD *)(v3 + 456) += v161;
    }
    if ( (v4[12] & 2) != 0 )
    {
      v163 = v4[13];
      v164 = v4[3];
      if ( v163 == 0 )
      {
        *(_DWORD *)(v3 + 460) = v164;
        v163 = v4[13];
      }
      if ( v163 == 1 )
        *(_DWORD *)(v3 + 460) += v164;
    }
  }
  else
  {
    switch ( v6 )
    {
      case 1:
        v7 = v4[2];
        if ( v7 != 0 )
          sub_50A080((char *)&g_CameraElements, v7, 0, 0);
        v8 = v4[5];
        if ( v8 != 0 )
        {
          v187[0] = -1;
          v190 = 0.0;
          v189 = 0;
          if ( (_WORD)v8 != 0 )
          {
            v187[1] = (unsigned __int16)v8;
            v188 = 4000;
            sub_42E8D0((int)v187, 0);
          }
        }
        if ( v4[6] != 0 )
          sub_50A080((char *)&g_CameraElements, v4[2], 1, 0);
        return 1;
      case 2:
        *(_BYTE *)(v3 + 696) = *((_BYTE *)v4 + 8);
        *(_BYTE *)(v3 + 697) = *((_BYTE *)v4 + 12);
        return 1;
      case 4:
        if ( ((int (*)(void))sub_41DFB0)() == 0 && *(_BYTE *)(v9 + 120) == 0 )
        {
          v10 = sub_41DD90(v9);
          v27 = v4[2] == 0;
          v11 = v180;
          HIDWORD(v179) = v10;
          if ( !v27 )
          {
            v12 = *(_DWORD *)(v180 + 144);
            v13 = *(_DWORD *)(v180 + 140);
            if ( v12 > v13 )
            {
              if ( sub_41DFB0(v180) == 0 )
                v14 += v4[2];
              v15 = v14;
              if ( v14 >= v12 )
                v14 = v12;
              if ( v14 >= 0 )
              {
                v13 = v12;
                if ( v15 < v12 )
                  v13 = v15;
              }
              else
              {
                v13 = 0;
              }
              v10 = HIDWORD(v179);
              *(_DWORD *)(v11 + 140) = v13;
            }
            if ( v13 > v10 )
              *(_DWORD *)(v11 + 140) = v10;
          }
          if ( v4[3] != 0 && sub_41DFB0(v11) == 0 )
            goto LABEL_31;
        }
        return 1;
      case 5:
        v16 = v4[2];
        v17 = *(_BYTE *)(v3 + 1231);
        switch ( v16 )
        {
          case 0:
            v18 = *(_BYTE *)(v3 + 1772);
            *(_BYTE *)(v3 + 1773) = v18 == 0;
            *(_BYTE *)(v3 + 1772) = v18 == 0;
            if ( (v18 == 0) != v18 )
            {
              TrainingMode_CopyActionSlotState(v3 + 1228);
              TrainingMode_CopyActionSlotState(v3 + 1244);
              *(_BYTE *)(v3 + 1775) = 0;
            }
            goto LABEL_66;
          case 1:
            v19 = v4[4];
            v20 = sub_505720(0);
            if ( v20 != 0 )
            {
              v21 = v19 << 8;
              v22 = *(_DWORD *)(v20 + 112) + *(_DWORD *)(v20 + 100) - v21;
              v23 = *(_DWORD *)(v20 + 112) + *(_DWORD *)(v20 + 100) + v21;
              if ( *(_BYTE *)(v20 + 1772) != 0 )
                v23 = v22;
              v24 = *(_DWORD *)(v3 + 112) + *(_DWORD *)(v3 + 100);
              if ( v24 <= v23 )
              {
                if ( v24 >= v23 )
                  *(_BYTE *)(v3 + 1773) = *(_BYTE *)(v3 + 1772);
                else
                  *(_BYTE *)(v3 + 1773) = 0;
              }
              else
              {
                *(_BYTE *)(v3 + 1773) = 1;
              }
            }
            v25 = *(_BYTE *)(v3 + 1773);
            *(_BYTE *)(v3 + 1774) = v25;
            v26 = sub_520B50(v25, 1);
            goto LABEL_65;
          case 2:
            if ( v17 == 7 || v17 == 4 )
              goto LABEL_48;
            v27 = v17 == 1;
            goto LABEL_47;
          case 3:
            if ( v17 != 9 && v17 != 6 )
            {
              v27 = v17 == 3;
LABEL_47:
              if ( !v27 )
                return 1;
            }
LABEL_48:
            v28 = *(_BYTE *)(v3 + 1772);
            *(_BYTE *)(v3 + 1772) = v28 == 0;
            *(_BYTE *)(v3 + 1773) = v28 == 0;
            if ( (v28 == 0) != v28 )
            {
              TrainingMode_CopyActionSlotState(v3 + 1228);
              TrainingMode_CopyActionSlotState(v3 + 1244);
              *(_BYTE *)(v3 + 1775) = 0;
            }
            if ( v4[3] == 0 )
            {
              v29 = *(_BYTE **)(v3 + 1188);
              if ( v29 != nullptr )
              {
                v30 = v29[1772];
                v29[1773] = v30 == 0;
                v29[1772] = v30 == 0;
                if ( (v30 == 0) != v30 )
                {
                  TrainingMode_CopyActionSlotState(v29 + 1228);
                  TrainingMode_CopyActionSlotState(v29 + 1244);
                  v29[1775] = 0;
                }
              }
            }
LABEL_66:
            *(_WORD *)(v3 + 636) = -*(_WORD *)(v3 + 636);
            sub_50E550(v3);
            break;
          case 4:
          case 5:
            v26 = sub_520B50(v16 - 4, 1);
            goto LABEL_65;
          case 6:
            v31 = 1;
            v32 = 21;
            if ( g_BattleRandCalc + 1 < 56 )
              v31 = g_BattleRandCalc + 1;
            g_BattleRandCalc = v31;
            if ( v31 > 34 )
              v32 = -34;
            v33 = dword_C7F984[v31] - dword_C7F984[v31 + v32];
            v34 = v33 + 0x7FFFFFFF;
            if ( v33 >= 0 )
              v34 = v33;
            dword_C7F984[v31] = v34;
            ++dword_C7FA64;
            v26 = sub_520B50(v34 % 2, 1);
LABEL_65:
            if ( v26 != 0 )
              goto LABEL_66;
            return 1;
          case 7:
            sub_520B50(*(char *)(a2 + 1772), 1);
            return 1;
          default:
            return 1;
        }
        return 1;
      case 6:
        v35 = v4[13];
        v36 = v3 + 464;
        if ( v35 != 0 )
        {
          v37 = v35 - 1;
          if ( v37 != 0 )
          {
            if ( v37 == 1 )
            {
              v38 = v4[2];
              HIDWORD(v178) = v38;
              if ( v38 >= 2 )
              {
                HIDWORD(v179) = v4[3];
                if ( SHIDWORD(v179) > 0 )
                {
                  v39 = v38 / 2;
                  v40 = v38 / 2 * (v38 + 1);
                  v41 = v40 + v39;
                  if ( (v178 & 0x100000000LL) == 0 )
                    v41 = v40;
                  v42 = v180;
                  *(_DWORD *)(v3 + 472) = (*(_DWORD *)(v180 + 104)
                                         + (v4[4] << 7)
                                         - HIDWORD(v179) * v41
                                         - *(_DWORD *)(v3 + 104))
                                        / SHIDWORD(v178);
                  v43 = (*(_DWORD *)(v42 + 100) - *(_DWORD *)(v3 + 100)) / SHIDWORD(v178);
                  *(_DWORD *)(v3 + 468) = 0;
                  *(_DWORD *)(v3 + 464) = v43;
                  *(_DWORD *)(v3 + 476) = HIDWORD(v179);
                }
              }
            }
          }
          else
          {
            v27 = v4[6] == 0;
            v44 = v4[5];
            v45 = 21;
            v181 = v44;
            HIDWORD(v179) = v4 + 6;
            if ( !v27 )
            {
              v46 = 1;
              if ( g_BattleRandCalc + 1 < 56 )
                v46 = g_BattleRandCalc + 1;
              v47 = 21;
              g_BattleRandCalc = v46;
              if ( v46 > 34 )
                v47 = -34;
              v48 = dword_C7F984[v46] - dword_C7F984[v46 + v47];
              v49 = v48 + 0x7FFFFFFF;
              if ( v48 >= 0 )
                v49 = v48;
              v50 = (_DWORD *)HIDWORD(v179);
              dword_C7F984[v46] = v49;
              ++dword_C7FA64;
              v51 = v49 % *v50;
              v45 = 21;
              v181 += v51;
              v44 = v181;
            }
            if ( *(_BYTE *)(v3 + 1772) == 1 && v44 != 0 )
            {
              v44 = 10000 - v44;
              v181 = v44;
            }
            v27 = v4[3] == 0;
            HIDWORD(v178) = v4[2];
            HIDWORD(v179) = v4 + 3;
            if ( !v27 )
            {
              v52 = 1;
              if ( g_BattleRandCalc + 1 < 56 )
                v52 = g_BattleRandCalc + 1;
              g_BattleRandCalc = v52;
              if ( v52 > 34 )
                v45 = -34;
              v180 = v52 + v45;
              v53 = dword_C7F984[v52] - dword_C7F984[v52 + v45];
              v54 = v53 + 0x7FFFFFFF;
              if ( v53 >= 0 )
                v54 = v53;
              v45 = HIDWORD(v179);
              dword_C7F984[v52] = v54;
              ++dword_C7FA64;
              HIDWORD(v178) += v54 % *(_DWORD *)v45;
              v44 = v181;
            }
            v190 = (float)((float)v44 / 10000.0) * 3.14159265 + (float)((float)v44 / 10000.0) * 3.14159265;
            v179 = *(double *)libm_sse2_sin_precise(v45).m128_u64;
            v184 = (float)SHIDWORD(v178);
            *(_DWORD *)(v3 + 464) = (int)(float)(v184 * v179);
            v55 = *(double *)libm_sse2_cos_precise().m128_u64;
            v56 = -(v184 * v55);
            *(_DWORD *)(v3 + 472) = (int)v56;
            v57 = v4[4];
            if ( v4[7] == 1 )
            {
              v58 = HIDWORD(v178);
              if ( v57 != 0 )
                v58 = -(SHIDWORD(v178) / v57);
            }
            else
            {
              v58 = v4[4];
            }
            v59 = (float)v58;
            v60 = v59 * v179;
            v61 = (int)v60;
            v62 = -(v59 * v55);
            *(_DWORD *)(v3 + 468) = (__int16)v61;
            *(_DWORD *)(v3 + 476) = (__int16)(int)v62;
          }
        }
        else
        {
          v63 = v4[6];
          v27 = v63 == 0;
          v177 = (_DWORD *)(v36 + 8 * (v63 != 0) + 4);
          v64 = 21;
          if ( !v27 )
            v36 = v3 + 472;
          v65 = v4[3];
          HIDWORD(v179) = v36;
          v66 = v4[2];
          if ( v65 != v66 )
          {
            v67 = 1;
            if ( g_BattleRandCalc + 1 < 56 )
              v67 = g_BattleRandCalc + 1;
            v68 = 21;
            g_BattleRandCalc = v67;
            if ( v67 > 34 )
              v68 = -34;
            v69 = dword_C7F984[v67] - dword_C7F984[v67 + v68];
            v70 = v69 + 0x7FFFFFFF;
            if ( v69 >= 0 )
              v70 = v69;
            v71 = v181;
            dword_C7F984[v67] = v70;
            ++dword_C7FA64;
            v72 = v4[2];
            v73 = v70 % (*(_DWORD *)(v71 + 12) - v72);
            v64 = 21;
            v66 = v72 + v73;
            v4 = (_DWORD *)v181;
          }
          *(_DWORD *)HIDWORD(v179) = v66;
          v74 = v4[4];
          if ( v4[5] != v74 )
          {
            v75 = 1;
            if ( g_BattleRandCalc + 1 < 56 )
              v75 = g_BattleRandCalc + 1;
            g_BattleRandCalc = v75;
            if ( v75 > 34 )
              v64 = -34;
            v180 = v75 + v64;
            v76 = dword_C7F984[v75] - dword_C7F984[v75 + v64];
            v77 = v76 + 0x7FFFFFFF;
            if ( v76 >= 0 )
              v77 = v76;
            v78 = v181;
            dword_C7F984[v75] = v77;
            ++dword_C7FA64;
            LOWORD(v74) = v4[4] + v77 % (*(_DWORD *)(v78 + 20) - v4[4]);
            v4 = (_DWORD *)v181;
            v74 = (__int16)v74;
          }
          *v177 = v74;
          if ( v4[6] == 0 && *(_BYTE *)(v3 + 1772) == 1 )
            goto LABEL_120;
        }
        return 1;
      case 8:
        v79 = *(_DWORD *)(a2 + 1176);
        HIDWORD(v178) = v79;
        if ( v79 != 0 && TrainingMenu_IsCurrentPlayerObject(v79, 1) != 0 )
        {
          v80 = (char *)HIDWORD(v178);
        }
        else
        {
          v80 = (char *)&g_PlayerObjects;
          HIDWORD(v178) = &g_PlayerObjects;
          while ( 1 )
          {
            if ( v80[2148] != 0 )
            {
              IsCurrentPlayerObject = TrainingMenu_IsCurrentPlayerObject(v80, v5);
              v80 = (char *)HIDWORD(v178);
              if ( IsCurrentPlayerObject != 0 && *(_BYTE *)(HIDWORD(v178) + 1260) != *(_BYTE *)(v3 + 1260) )
                break;
            }
            v80 += 3140;
            HIDWORD(v178) = v80;
            if ( (int)v80 >= (int)byte_1103EF8 )
              return 1;
          }
        }
        if ( v80 != nullptr )
        {
          sub_583570(v80, (char *)&v179 + 4, (char *)&v190 + 4);
          Concurrency::SchedulerPolicy::~SchedulerPolicy((Concurrency::SchedulerPolicy *)v183);
          v82 = D3DXMatrixRotationZ(&v182, *((float *)&v179 + 1) * 0.017453292f);
          D3DXMatrixMultiply(v183, v183, v82);
          v189 = 1065353216;
          v190 = 0.0;
          D3DXVec3TransformCoord(&v184, &v189, v183);
          *(_DWORD *)(v3 + 464) += (int)(float)((float)(int)v4[3] * *(float *)&v184);
          v83 = (float)(int)v4[3];
          *(_DWORD *)(v3 + 476) = 0;
          *(_DWORD *)(v3 + 468) = 0;
          *(_DWORD *)(v3 + 472) += (int)(float)(v83 * *((float *)&v184 + 1));
          if ( v4[4] != 0 )
            ObjectUpdateFrame(v3);
        }
        return 1;
      case 9:
        sub_51F800(v4[2] == 0);
        return 1;
      case 11:
        if ( *(_DWORD *)(a2 + 8) == 0 && v4[3] != 0 )
          sub_507CD0(v4[2]);
        return 1;
      case 12:
        v177 = (_DWORD *)v4[12];
        v84 = v4[13];
        if ( v84 != 0 )
        {
          switch ( v84 )
          {
            case 1:
              v97 = v4[2];
              v98 = v4[3];
              HIDWORD(v190) = v97 << 7;
              sub_50BB80((int)&g_CameraElements);
              v99 = v181;
              v100 = HIDWORD(v190);
              *(_DWORD *)(v3 + 100) = HIDWORD(v190);
              *(_DWORD *)(v3 + 104) = v98 << 7;
              if ( (*(_BYTE *)(v99 + 16) & 1) != 0 && *(_BYTE *)(v180 + 1772) != 0 )
                *(_DWORD *)(v3 + 100) = -v100;
              break;
            case 2:
              v101 = *(_DWORD *)(v3 + 1200);
              v102 = v4[2] << 7;
              v103 = v4[3] << 7;
              if ( v101 == 0 )
                v101 = v180;
              v104 = -v102;
              if ( *(_BYTE *)(v101 + 1772) == 0 )
                v104 = v102;
              *(_DWORD *)(v3 + 100) = *(_DWORD *)(v101 + 100) + *(_DWORD *)(v101 + 112) + v104;
              *(_DWORD *)(v3 + 104) = v103 + *(_DWORD *)(v101 + 104) + *(_DWORD *)(v101 + 116);
              break;
            case 3:
              v105 = dword_CABCE4 >> 1 << 7;
              HIDWORD(v190) = -128 * (dword_CABCE4 >> 1);
              v106 = sub_507ED0(v3);
              if ( v106 == 1 )
              {
                *(_DWORD *)(v3 + 100) = v105;
              }
              else if ( v106 == 2 )
              {
                *(_DWORD *)(v3 + 100) = HIDWORD(v190);
              }
              break;
            default:
              break;
          }
        }
        else
        {
          v85 = v4[6];
          v180 = v85;
          v181 = (v85 & 0x10) != 0;
          if ( (v85 & 2) != 0 )
          {
            if ( (v85 & 4) == 0 )
              *(_DWORD *)(a2 + 100) = *(_DWORD *)(v3 + 100);
            if ( (v85 & 8) == 0 )
              *(_DWORD *)(a2 + 104) = *(_DWORD *)(v3 + 104);
          }
          else
          {
            v27 = v4[5] == 1;
            HIDWORD(v178) = 0;
            if ( !v27 )
              v3 = a2;
            if ( (v85 & 0x10) != 0 )
            {
              v86 = v181;
            }
            else
            {
              v86 = *(_BYTE *)(v3 + 1772) == 0;
              v181 = v86;
            }
            v87 = v4[2] << 7;
            HIDWORD(v179) = v4[3] << 7;
            v88 = v180;
            v185 = v87;
            if ( (v180 & 1) != 0 )
            {
              v89 = sub_5056B0(v3, (char *)&v178 + 4);
              v90 = v180;
              if ( v89 != 0 )
              {
                if ( (v180 & 4) == 0 )
                  *(_DWORD *)(v3 + 100) = *(_DWORD *)(v89 + 100) + *(_DWORD *)(v89 + 112);
                if ( (v90 & 8) == 0 )
                  *(_DWORD *)(v3 + 104) = 0;
              }
              if ( (v90 & 4) == 0 )
              {
                v91 = v185;
                if ( v181 == 0 )
                  v91 = -v185;
                v176 = v91;
                if ( v177 != nullptr )
                {
                  sub_580C60(&v176);
                  v91 = v176;
                  v90 = v180;
                }
                *(_DWORD *)(v3 + 100) += v91;
              }
              if ( (v90 & 8) == 0 )
                *(_DWORD *)(v3 + 104) += HIDWORD(v179);
            }
            else
            {
              HIDWORD(v178) = v180 & 4;
              if ( v4[4] != 0 )
              {
                if ( HIDWORD(v178) == 0 )
                {
                  v94 = *(_DWORD *)(v3 + 100);
                  v95 = -v87;
                  if ( v181 == 0 )
                    v95 = v87;
                  v96 = dword_CABA98 + v95 - v94;
                  v175 = v96;
                  if ( v177 != nullptr )
                  {
                    sub_580C60(&v175);
                    v94 = *(_DWORD *)(v3 + 100);
                    v96 = v175;
                    v88 = v180;
                  }
                  *(_DWORD *)(v3 + 100) = v94 + v96;
                }
                if ( (v88 & 8) == 0 )
                  *(_DWORD *)(v3 + 104) = HIDWORD(v179);
              }
              else
              {
                v185 = *(_DWORD *)(v3 + 100);
                HIDWORD(v190) = *(_DWORD *)(v3 + 104);
                v92 = 0;
                v181 = 0;
                v174 = 0;
                if ( HIDWORD(v178) != 0 )
                  goto LABEL_168;
                v92 = v87;
                if ( !v86 )
                  v92 = -v87;
                v173 = v92;
                if ( v177 == nullptr )
                {
LABEL_168:
                  v93 = v181;
                }
                else
                {
                  sub_580C60(&v173);
                  v93 = v174;
                  v92 = v173;
                  v88 = v180;
                }
                if ( (v88 & 8) == 0 )
                  v93 = HIDWORD(v179);
                *(_DWORD *)(v3 + 100) = v92 + v185;
                *(_DWORD *)(v3 + 104) = v93 + HIDWORD(v190);
              }
            }
          }
        }
        return 1;
      case 14:
        if ( dword_CC9B98 == 0 )
        {
          v107 = sub_533D00(2, v4[2], 1, 0);
          v108 = v107;
          if ( v107 != 0 )
          {
            if ( v4[3] != 0 )
            {
              v109 = sub_41DD90(v107);
              v110 = v109;
              v111 = *(_DWORD *)(v108 + 140) + v4[3];
              if ( v111 < v109 )
                v110 = *(_DWORD *)(v108 + 140) + v4[3];
              if ( v110 >= 0 )
              {
                if ( v111 < v109 )
                  v109 = *(_DWORD *)(v108 + 140) + v4[3];
              }
              else
              {
                v109 = 0;
              }
              *(_DWORD *)(v108 + 140) = v109;
            }
            if ( v4[4] != 0 )
LABEL_31:
              TrainingMenu_ApplyValueToPlayerData(1);
          }
        }
        return 1;
      case 15:
        v112 = dword_A1CD2C;
        if ( v4[2] == 0 )
          v112 = 0;
        sub_4B8AF0(v112);
        return 1;
      case 16:
        v113 = v4[13];
        if ( v113 != 0 )
        {
          if ( v113 == 1 )
          {
            v129 = dword_CABA9C + (v4[3] << 7) - *(_DWORD *)(v3 + 104);
            v130 = dword_CABA98 + (v4[2] << 7) - *(_DWORD *)(v3 + 100);
            *(_DWORD *)(v3 + 464) = v130;
            *(_DWORD *)(v3 + 468) = 0;
            *(_DWORD *)(v3 + 472) = v129;
            *(_DWORD *)(v3 + 476) = 0;
            HIDWORD(v190) = v4[4];
            if ( SHIDWORD(v190) >= 2 )
            {
              *(_DWORD *)(v3 + 464) = v130 / SHIDWORD(v190);
              *(_DWORD *)(v3 + 472) = v129 / v4[4];
            }
          }
        }
        else
        {
          v27 = v4[3] == 0;
          v114 = v4[2];
          v115 = 21;
          v181 = v114;
          HIDWORD(v190) = v4 + 3;
          if ( !v27 )
          {
            v116 = 1;
            if ( g_BattleRandCalc + 1 < 56 )
              v116 = g_BattleRandCalc + 1;
            v117 = 21;
            g_BattleRandCalc = v116;
            if ( v116 > 34 )
              v117 = -34;
            v118 = dword_C7F984[v116] - dword_C7F984[v116 + v117];
            v119 = v118 + 0x7FFFFFFF;
            if ( v118 >= 0 )
              v119 = v118;
            v120 = (_DWORD *)HIDWORD(v190);
            dword_C7F984[v116] = v119;
            ++dword_C7FA64;
            v121 = v119 % *v120 + v181;
            v115 = 21;
            v114 = 360
                 * (((int)(((unsigned __int64)(1240768329LL * v121) >> 32) - v121) >> 8)
                  + ((unsigned int)(((unsigned __int64)(1240768329LL * v121) >> 32) - v121) >> 31))
                 + v121;
            v181 = v114;
          }
          if ( *(_BYTE *)(v3 + 1772) == 1 )
          {
            v122 = 180 - v114 < 0;
            v123 = 180 - v114;
            v181 = v123;
            if ( v122 )
              v181 = v123 + 360;
          }
          v27 = v4[5] == 0;
          v124 = v4[4];
          HIDWORD(v179) = v124;
          if ( v27 )
          {
            v128 = v124;
          }
          else
          {
            v125 = 1;
            if ( g_BattleRandCalc + 1 < 56 )
              v125 = g_BattleRandCalc + 1;
            g_BattleRandCalc = v125;
            if ( v125 > 34 )
              v115 = -34;
            v180 = v125 + v115;
            v126 = dword_C7F984[v125] - dword_C7F984[v125 + v115];
            v127 = v126 + 0x7FFFFFFF;
            if ( v126 >= 0 )
              v127 = v126;
            dword_C7F984[v125] = v127;
            ++dword_C7FA64;
            v128 = v127 % (v4[5] + 1) + HIDWORD(v179);
          }
          *((float *)&v190 + 1) = (float)v181 * 0.017453292f;
          *(_DWORD *)(v3 + 472) = (int)(float)(sub_40FDF0().m128_f32[0] * (float)v128);
          *(_DWORD *)(v3 + 464) = (int)(float)(sub_40D630().m128_f32[0] * (float)v128);
        }
        return 1;
      case 19:
        v131 = *(unsigned __int8 *)(a2 + 1260);
        goto LABEL_236;
      case 21:
        dword_CABAAC = dword_CABAC4;
        dword_CABAB0 = dword_CABAC8;
        dword_CABAB4 = dword_CABACC;
        dword_CABAB8 = dword_CABAD0;
        dword_CABABC = dword_CABAD4;
        return 1;
      case 22:
        v132 = 0;
        if ( v4[3] != 0 )
          v132 = RandomSelectFromArray(v4[3]);
        v133 = v4[2] + v132;
        v134 = v4[7];
        if ( v134 != 0 )
        {
          switch ( v134 )
          {
            case 1:
              *(float *)(v3 + 1768) = (float)v133 / 10000.0;
              break;
            case 2:
              v135 = (float)v133 / 10000.0;
              *(float *)(v3 + 1768) = v135;
              *(float *)(v3 + 1764) = v135;
              break;
            case 10:
              *(_DWORD *)(v3 + 1760) = v133;
              break;
            default:
              break;
          }
        }
        else
        {
          *(float *)(v3 + 1764) = (float)v133 / 10000.0;
        }
        return 1;
      case 102:
        *(_BYTE *)(v3 + 1826) += *((_BYTE *)v4 + 8);
        return 1;
      case 103:
        v136 = *(_BYTE *)(v3 + 1826);
        v137 = *((_BYTE *)v4 + 8);
        if ( v136 > v137 )
          *(_BYTE *)(v3 + 1826) = v136 - v137;
        else
          *(_BYTE *)(v3 + 1826) = 0;
        return 1;
      case 105:
        if ( v4[4] / 10 != 0 || (v138 = *(_DWORD *)(v3 + 1196)) == 0 )
          v138 = v3;
        v139 = sub_505AC0(v138);
        if ( HIDWORD(v139) != 0 )
        {
          if ( HIDWORD(v139) == 1 )
            *(_DWORD *)(v139 + 4 * v4[2] + 844) += v4[3];
        }
        else
        {
          *(_DWORD *)(v139 + 4 * v4[2] + 844) = v4[3];
        }
        return 1;
      case 106:
        v140 = v4[2];
        if ( v140 == 0 )
          return 1;
        v141 = v4[3];
        v142 = *(_DWORD *)(v3 + 124);
        if ( v141 == 0 )
          goto LABEL_265;
        if ( v141 == 1 )
        {
          *(_DWORD *)(v3 + 124) = v140 | v142;
        }
        else
        {
          if ( v141 == 2 )
          {
            v140 &= v142;
LABEL_265:
            v142 &= ~v140;
          }
          *(_DWORD *)(v3 + 124) = v142;
        }
        return 1;
      case 111:
        v143 = v4[2];
        if ( v143 != 0 )
        {
          if ( v143 == 1 )
          {
            *(_OWORD *)(v3 + 464) = *(_OWORD *)(v3 + 548);
            *(_QWORD *)(v3 + 480) = *(_QWORD *)(v3 + 564);
          }
        }
        else
        {
          *(_OWORD *)(v3 + 548) = *(_OWORD *)(v3 + 464);
          *(_QWORD *)(v3 + 564) = *(_QWORD *)(v3 + 480);
          if ( v4[3] != 0 )
          {
            *(_DWORD *)(v3 + 468) = 0;
            *(_DWORD *)(v3 + 464) = 0;
            *(_DWORD *)(v3 + 480) = 0;
            *(_DWORD *)(v3 + 476) = 0;
            *(_DWORD *)(v3 + 472) = 0;
            *(_DWORD *)(v3 + 484) = 0;
          }
          v144 = v4[4];
          if ( v144 != 0 && *(int *)(v3 + 464) <= 0 && *(int *)(v3 + 476) <= 0 )
            *(_DWORD *)(v3 + 560) = v144;
        }
        return 1;
      case 112:
        v131 = *(unsigned __int8 *)(v3 + 1260);
LABEL_236:
        sub_527DB0((int *)&unk_CAB0C0 + 37 * v131, v4[2], v4[3], 0);
        return 1;
      case 115:
        v145 = 1;
        if ( v4[2] != 0 )
          v145 = v4[2];
        v146 = *(_DWORD *)(v3 + 464) / v145;
        *(_OWORD *)(v3 + 524) = *(_OWORD *)(v3 + 464);
        *(_QWORD *)(v3 + 540) = *(_QWORD *)(v3 + 480);
        *(_DWORD *)(v3 + 528) = -v146;
        *(_DWORD *)(v3 + 536) = 0;
        *(_DWORD *)(v3 + 532) = 0;
        *(_DWORD *)(v3 + 544) = 0;
        *(_DWORD *)(v3 + 468) = 0;
        *(_DWORD *)(v3 + 464) = 0;
        *(_DWORD *)(v3 + 480) = 0;
        return 1;
      case 116:
        v147 = (float)*(int *)(v3 + 1760) / 10000.0;
        *((float *)&v179 + 1) = (float)(int)v4[2] * 0.0078125;
        if ( *(_BYTE *)(v3 + 1772) == 1 )
          v147 = 1.0 - v147;
        v190 = v147 * 3.14159265 + v147 * 3.14159265;
        v148 = *(double *)((__m128 (*)(void))libm_sse2_sin_precise)().m128_u64 * *((float *)&v179 + 1);
        *(_DWORD *)(v3 + 464) = (int)(float)(v148 * 128.0);
        v149 = -(*(double *)libm_sse2_cos_precise().m128_u64 * *((float *)&v179 + 1));
        *(_DWORD *)(v3 + 472) = (int)(float)(v149 * 128.0);
        return 1;
      case 117:
        HIDWORD(v190) = v4[2];
        v150 = v4[3];
        v178 = 0;
        if ( v150 == 0 )
          v150 = 1;
        HIDWORD(v178) = a2;
        HIDWORD(v179) = v150;
        v177 = (_DWORD *)v4[6];
        v185 = (unsigned __int8)v177 & 1;
        v151 = sub_42CAB0(&v178);
        if ( v151 == 0 )
          return 1;
        break;
      case 153:
        *(_WORD *)(v3 + 650) = 66;
        return 1;
      case 252:
        *(_BYTE *)(a2 + 676) = *((_BYTE *)v4 + 8);
        return 1;
      case 254:
        v157 = sub_41F350((_DWORD *)v3);
        if ( v157 != 0 )
          *(_BYTE *)(v157 + 2496) |= v158;
        return 1;
      case 255:
        v159 = sub_41F350((_DWORD *)v3);
        if ( v159 != 0 )
          *(_BYTE *)(v159 + 2496) |= 2u;
        return 1;
      default:
        return 1;
    }
    while ( 1 )
    {
      v152 = v151;
      if ( *(_DWORD *)(v151 + 1196) != 0 )
        v152 = *(_DWORD *)(v151 + 1196);
      if ( v180 == v152 )
      {
        v153 = (__int16 *)sub_41F760(HIDWORD(v190) + 9);
        if ( v153 != nullptr )
          break;
      }
      v151 = sub_42CAB0(&v178);
      if ( v151 == 0 )
        return 1;
    }
    v188 = *v153;
    v189 = v153[1];
    LODWORD(v190) = v153[2];
    HIDWORD(v190) = v153[3];
    sub_41F790(&v188);
    if ( v185 == 0 )
    {
      v154 = (v188 - *(_DWORD *)(v3 + 100)) / SHIDWORD(v179);
      *(_DWORD *)(v3 + 464) = v154;
      *(_DWORD *)(v3 + 480) = v154;
    }
    v155 = (char)v177;
    if ( ((unsigned __int8)v177 & 2) == 0 )
    {
      v156 = (v189 - *(_DWORD *)(v3 + 104)) / SHIDWORD(v179);
      *(_DWORD *)(v3 + 472) = v156;
      *(_DWORD *)(v3 + 484) = v156;
      v155 = (char)v177;
    }
    if ( (v155 & 4) != 0 )
      *(_DWORD *)(v3 + 1760) = (int)(float)(sub_5836C0().m128_f32[0] * 10000.0);
  }
  return 1;
}
