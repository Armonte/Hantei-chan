int __fastcall sub_590260(int a1, int *a2)
{
  int v2; // edi
  int *v3; // esi
  int v4; // eax
  int v5; // ecx
  int v6; // eax
  int v7; // eax
  int v8; // ecx
  int v9; // esi
  int v10; // ecx
  char v11; // dl
  int v12; // ecx
  int v13; // edx
  int v14; // edx
  int v15; // eax
  __int64 v16; // rax
  int v17; // eax
  int v18; // eax
  int v19; // eax
  bool v20; // zf
  int v21; // eax
  int v22; // ecx
  int v23; // edx
  int v24; // ecx
  char v25; // dl
  int v26; // eax
  int v27; // edx
  int v28; // ecx
  int v29; // eax
  int v30; // ecx
  int v31; // eax
  int v32; // edx
  int v33; // esi
  int v34; // ecx
  int v35; // eax
  int v36; // ecx
  int v37; // esi
  int v38; // ecx
  int v39; // esi
  int v40; // ecx
  int v41; // esi
  int v42; // eax
  int v43; // eax
  int v44; // ecx
  int v45; // ecx
  char v46; // al
  bool v47; // zf
  char v48; // al
  int v49; // eax
  int v50; // edx
  int v51; // eax
  int v52; // esi
  int v53; // edx
  int v54; // ecx
  int v55; // edx
  int v56; // edx
  char v57; // cl
  int v58; // ecx
  int v59; // eax
  int v60; // eax
  int v61; // eax
  int v62; // eax
  int v63; // eax
  int v64; // ecx
  int v65; // eax
  int v66; // ecx
  int v67; // ecx
  int v68; // edx
  int v69; // eax
  int v70; // eax
  int v71; // eax
  int v72; // eax
  int v73; // ecx
  int v74; // ecx
  __int64 v75; // rax
  bool v76; // zf
  int v77; // eax
  int v78; // eax
  bool v79; // zf
  int v80; // ecx
  int v81; // edx
  int v82; // edx
  int v83; // eax
  int v84; // esi
  int v85; // eax
  int *v86; // ecx
  int v88; // edx
  bool v89; // cc
  __int64 v90; // rax
  int v91; // eax
  int v92; // eax
  bool v93; // sf
  int v94; // ecx
  _DWORD *v95; // eax
  int v97; // [esp-18h] [ebp-58h]
  int v98; // [esp-18h] [ebp-58h]
  int v99; // [esp-18h] [ebp-58h]
  int v100; // [esp-18h] [ebp-58h]
  int v101; // [esp-14h] [ebp-54h]
  int v102; // [esp-14h] [ebp-54h]
  int v103; // [esp-14h] [ebp-54h]
  int v104; // [esp-14h] [ebp-54h]
  int v105; // [esp-4h] [ebp-44h]
  int v106; // [esp+Ch] [ebp-34h] BYREF
  int v107; // [esp+10h] [ebp-30h]
  int v108; // [esp+14h] [ebp-2Ch]
  int v109; // [esp+18h] [ebp-28h] BYREF
  int v110; // [esp+1Ch] [ebp-24h] BYREF
  int v111; // [esp+20h] [ebp-20h]
  int v112; // [esp+24h] [ebp-1Ch]
  int v113; // [esp+28h] [ebp-18h]
  __int64 v114; // [esp+2Ch] [ebp-14h]
  int v115; // [esp+34h] [ebp-Ch]
  int v116; // [esp+38h] [ebp-8h]

  v2 = a1;
  v3 = a2;
  v4 = a1;
  v111 = (int)a2;
  v112 = a1;
  v5 = *(_DWORD *)(a1 + 1196);
  v107 = v5;
  if ( v5 != 0 )
    v4 = v5;
  v106 = v4;
  switch ( *a2 )
  {
    case 2:
      v111 = 0;
      v6 = a2[1];
      if ( v6 != 0 )
      {
        if ( (v6 & 1) != 0
          && (*(_DWORD *)(v2 + 100) > sub_507F10(1) + 12800 || *(_DWORD *)(v2 + 100) < sub_507F10(0) - 12800) )
        {
          sub_50E620(v2);
          v111 = 1;
        }
        if ( (v3[1] & 2) != 0
          && (*(_DWORD *)(v2 + 104) < sub_507F10(2) - 12800 || *(_DWORD *)(v2 + 104) > sub_507F10(3) + 12800) )
        {
          sub_50E620(v2);
          v7 = 1;
        }
        else
        {
          v7 = v111;
        }
      }
      else
      {
        v7 = 0;
      }
      if ( v3[2] != 0 && *(_BYTE *)(v2 + 1776) == 1 )
      {
        sub_50E620(v2);
      }
      else if ( v7 == 0 )
      {
        return 1;
      }
      v8 = v3[4];
      if ( v8 != 0 )
      {
        v9 = v8 % 100;
        v107 = v8 % 10;
        if ( v8 % 100 < 100 && v8 < 100 )
        {
          if ( *(_DWORD *)(v2 + 1196) != 0 )
            v2 = *(_DWORD *)(v2 + 1196);
          if ( v2 != 0 )
          {
            v10 = v9 / 10;
            v11 = *(_BYTE *)(v2 + v9 / 10 + 833);
            if ( v11 > v107 )
              *(_BYTE *)(v2 + v10 + 833) = v11 - v107;
            else
              *(_BYTE *)(v2 + v10 + 833) = 0;
          }
        }
      }
      return 1;
    case 3:
      if ( *(_BYTE *)(v2 + 683) != 0 )
        return 1;
      v12 = *(_DWORD *)(v2 + 640);
      if ( v12 == 0 )
        return 1;
      v13 = a2[7];
      if ( v3[6] != 0 )
      {
        if ( dword_CAB0C8[37 * *(unsigned __int8 *)(v2 + 1260)] != 0 )
          v14 = dword_CAB0E0[37 * *(unsigned __int8 *)(v2 + 1260)];
        else
          v14 = 0;
        if ( v3[6] > v14 )
          return 1;
        v13 = v3[7];
      }
      if ( v13 != 0 )
      {
        v15 = dword_CAB0C8[37 * *(unsigned __int8 *)(v2 + 1260)] != 0
            ? dword_CAB0F8[37 * *(unsigned __int8 *)(v2 + 1260)]
            : 0;
        if ( v13 > v15 )
          return 1;
      }
      if ( (v12 & 0x100) != 0 && v3[2] / 100 != 0 )
        return 1;
      v16 = v3[2];
      v107 = 100;
      HIDWORD(v16) = v16 % 100;
      v17 = 0;
      switch ( HIDWORD(v16) )
      {
        case 0:
          v17 = 2;
          break;
        case 1:
          v17 = 3;
          break;
        case 2:
          v17 = 14;
          break;
        case 3:
          v17 = 15;
          break;
        case 5:
          v17 = 1;
          break;
        case 6:
          v17 = 12;
          break;
        case 7:
          v17 = 13;
          break;
        default:
          break;
      }
      if ( (v12 & 0x80u) != 0 || (v12 & v17) == 0 )
        return 1;
      v18 = v3[3];
      if ( v18 == 0 )
        goto LABEL_59;
      v19 = v18 - 1;
      if ( v19 != 0 )
      {
        if ( v19 != 1 )
          return 1;
        v20 = (v12 & 0x20) == 0;
      }
      else
      {
        v20 = (v12 & 0x10) == 0;
      }
      if ( !v20 )
      {
LABEL_59:
        v21 = v3[1];
        if ( v21 >= 10000 )
          v21 = 0;
        Object_SetPatternFrame(v21);
        if ( *(_BYTE *)(v2 + 1776) == 1 )
        {
          *(_DWORD *)(v2 + 104) = 0;
          *(_BYTE *)(v2 + 1776) = 0;
        }
        v22 = v3[4];
        if ( v22 != 0 )
        {
          v23 = v22 % v107;
          goto LABEL_65;
        }
      }
      return 1;
    case 5:
      v26 = sub_41F350((_DWORD *)v2);
      if ( v26 != 0 && *(_DWORD *)(v26 + 2504) != 0 )
        sub_56C680(v2, 0);
      return 1;
    case 6:
      v106 = 0;
      if ( sub_5920F0(&v106) != 0 )
      {
        v105 = v106;
        goto LABEL_77;
      }
      return 1;
    case 7:
      v106 = 0;
      if ( sub_5920F0(&v106) != 0 )
      {
        if ( v3[8] != 0 )
          ObjectUpdatePattern(v2);
        else
          Object_SetPatternFrame(-1);
      }
      return 1;
    case 8:
      v27 = 1;
      v108 = v3[2];
      v28 = 21;
      if ( g_BattleRandCalc + 1 < 56 )
        v27 = g_BattleRandCalc + 1;
      g_BattleRandCalc = v27;
      if ( v27 > 34 )
        v28 = -34;
      v29 = dword_C7F984[v27] - dword_C7F984[v27 + v28];
      v30 = v29 + 0x7FFFFFFF;
      if ( v29 >= 0 )
        v30 = v29;
      dword_C7F984[v27] = v30;
      ++dword_C7FA64;
      v31 = v3[1];
      v32 = v3[4];
      v111 = v31;
      v107 = v32;
      if ( v108 > v30 % 512 || v108 >= 512 )
      {
        if ( v3[3] != 0 )
        {
          if ( v107 > 0 )
          {
            v33 = 1;
            v34 = 21;
            if ( g_BattleRandCalc + 1 < 56 )
              v33 = g_BattleRandCalc + 1;
            g_BattleRandCalc = v33;
            if ( v33 > 34 )
              v34 = -34;
            v35 = dword_C7F984[v33] - dword_C7F984[v33 + v34];
            v36 = v35 + 0x7FFFFFFF;
            if ( v35 >= 0 )
              v36 = v35;
            dword_C7F984[v33] = v36;
            ++dword_C7FA64;
          }
          v105 = -1;
LABEL_77:
          Object_SetPatternFrame(v105);
        }
        else
        {
          Object_SetPatternFrame(v31);
        }
      }
      return 1;
    case 11:
      if ( sub_591A80(a2[4]) != 0 )
        sub_591740(1, v3[3]);
      return 1;
    case 14:
      if ( *(_DWORD *)(v2 + 748) != 0 )
        return 1;
      v37 = a2[3];
      v109 = 10000;
      v107 = 1000;
      v106 = v37 / 10000 + 10 * (v37 % 10000 / 1000);
      v38 = *(_DWORD *)(v111 + 8);
      v110 = v38 % 1000;
      v39 = v37 % 1000;
      v40 = v38 / 10000 != 0 ? sub_533D00(v39, v110, 1, v106) : sub_533D00(v39, v110, 2, v106);
      v106 = v40;
      v41 = v111;
      if ( (*(_BYTE *)(v111 + 20) & 1) != 0 || v40 == 0 )
        return 1;
      if ( *(_DWORD *)(v111 + 8) % v109 / 1000 != 0 )
      {
        if ( sub_507ED0(v40) != 0 )
          return 1;
        v40 = v106;
      }
      if ( *(_DWORD *)(v111 + 12) % v109 == 0 )
        sub_51E3F0(v40);
      ++*(_WORD *)(v40 + 932);
      v42 = *(_DWORD *)(v41 + 16);
      if ( v42 != 0 )
      {
        sub_5209B0(v42, 0);
        sub_5209B0(*(_DWORD *)(v41 + 16), 0);
      }
      v43 = *(_DWORD *)(v41 + 20);
      if ( (v43 & 4) != 0 )
      {
        v44 = *(_DWORD *)(v40 + 100);
        if ( v44 <= *(_DWORD *)(v2 + 100) )
        {
          if ( v44 >= *(_DWORD *)(v2 + 100) )
            return 1;
LABEL_122:
          v46 = *(_BYTE *)(v2 + 1772);
          *(_WORD *)(v2 + 1772) = 0;
          v47 = v46 == 0;
          goto LABEL_125;
        }
      }
      else
      {
        if ( (v43 & 2) == 0 )
          return 1;
        v45 = *(_DWORD *)(v40 + 100);
        if ( v45 > *(_DWORD *)(v2 + 100) )
          goto LABEL_122;
        if ( v45 >= *(_DWORD *)(v2 + 100) )
          return 1;
      }
      v48 = *(_BYTE *)(v2 + 1772);
      *(_WORD *)(v2 + 1772) = 257;
      v47 = v48 == 1;
LABEL_125:
      if ( !v47 )
      {
        TrainingMode_CopyActionSlotState(v2 + 1228);
        TrainingMode_CopyActionSlotState(v2 + 1244);
        *(_BYTE *)(v2 + 1775) = 0;
      }
      return 1;
    case 15:
      if ( *(_DWORD *)(v2 + 748) == 0 )
      {
        v49 = sub_533D00(a2[3] % 1000, a2[2], 1, a2[3] / 10000);
        v108 = v49;
        if ( v49 != 0 && sub_52A800(v2, v49) != 0 )
        {
          v50 = v108;
          *(_BYTE *)(v108 + 713) = 0;
          goto LABEL_131;
        }
      }
      return 1;
    case 17:
      if ( *(_DWORD *)(v2 + 640) != 0 && *(__int16 *)(v2 + 932) >= a2[2] )
      {
        if ( *(_BYTE *)(v2 + 1776) == 1 )
        {
          *(_DWORD *)(v2 + 104) = 0;
          *(_BYTE *)(v2 + 1776) = 0;
        }
        v52 = a2[3];
        if ( v52 != 0 && v52 < 100 )
        {
          v53 = v52 % 100;
          v108 = v52 % 100;
          v107 = v52 % 10;
          if ( v52 % 100 < 100 )
          {
            if ( v5 != 0 )
              v2 = v5;
            if ( v2 != 0 )
            {
              v24 = v53 / 10;
              v25 = *(_BYTE *)(v53 / 10 + v2 + 833);
              if ( v25 > v107 )
                goto LABEL_71;
              *(_BYTE *)(v24 + v2 + 833) = 0;
            }
          }
        }
      }
      return 1;
    case 19:
      if ( *(_DWORD *)(v2 + 748) == 0 )
      {
        if ( a2[8] != 0 )
        {
          if ( *(_BYTE *)(v2 + 683) == 0 && *(_DWORD *)(v2 + 640) != 0 )
          {
            v64 = *(_DWORD *)(v2 + 1176);
            v65 = *(_DWORD *)(v2 + 640) & 4;
            v108 = v64;
            if ( v64 != 0 && v65 != 0 )
            {
              *(_BYTE *)(v2 + 1260) = *(_BYTE *)(v64 + 1260);
              sub_520B50(*(char *)(v64 + 1772), 1);
              v66 = v2;
              if ( *(_DWORD *)(v2 + 1196) != 0 )
                v66 = *(_DWORD *)(v2 + 1196);
              *(_DWORD *)(v108 + 1220) = v66;
              if ( v3[3] != 0 )
              {
                v22 = v3[4];
                v23 = v22 % 100;
LABEL_65:
                v107 = v22 % 10;
                if ( v23 < 100 )
                {
                  if ( *(_DWORD *)(v2 + 1196) != 0 )
                    v2 = *(_DWORD *)(v2 + 1196);
                  if ( v2 != 0 )
                  {
                    v24 = v23 / 10;
                    v25 = *(_BYTE *)(v23 / 10 + v2 + 833);
                    if ( v25 > v107 )
LABEL_71:
                      *(_BYTE *)(v24 + v2 + 833) = v25 - v107;
                    else
                      *(_BYTE *)(v24 + v2 + 833) = 0;
                  }
                }
              }
            }
          }
        }
        else
        {
          v106 = sub_533D00(6, 0, 2, 0);
          if ( v106 != 0 )
          {
            v114 = 0;
            v54 = 0;
            while ( g_ActiveEffectList < v54 && g_ActiveEffectList > v54 )
            {
              while ( 1 )
              {
                v55 = dword_CCC7A4[v54++];
                if ( v55 != 0 )
                  break;
                if ( v54 >= g_ActiveEffectList )
                  goto LABEL_155;
              }
              v3 = (int *)v111;
              if ( v106 != *(_DWORD *)(v55 + 1200) )
                break;
              if ( (*(_BYTE *)(v55 + 124) & 4) != 0 )
              {
                v20 = *(_BYTE *)(v55 + 1772) == 0;
                *(_BYTE *)(v55 + 1260) = *(_BYTE *)(v2 + 1260);
                *(_BYTE *)(v55 + 1772) = v20;
                v61 = v3[1];
                if ( v61 != 0 )
                  v62 = (*(_DWORD *)(v55 + 464) * v61) >> 8;
                else
                  v62 = *(_DWORD *)(v55 + 464);
                *(_DWORD *)(v55 + 464) = -v62;
                *(_DWORD *)(v55 + 468) = -*(_DWORD *)(v55 + 468);
                *(_DWORD *)(v55 + 100) = 2 * *(_DWORD *)(v106 + 100) - *(_DWORD *)(v55 + 100);
              }
            }
LABEL_155:
            v56 = v106;
            v57 = *(_BYTE *)(v106 + 1772);
            *(_BYTE *)(v106 + 1260) = *(_BYTE *)(v2 + 1260);
            *(_BYTE *)(v56 + 1773) = v57 == 0;
            *(_BYTE *)(v56 + 1772) = v57 == 0;
            if ( (v57 == 0) != v57 )
            {
              TrainingMode_CopyActionSlotState(v56 + 1228);
              TrainingMode_CopyActionSlotState(v106 + 1244);
              v56 = v106;
              *(_BYTE *)(v106 + 1775) = 0;
            }
            v58 = *(_DWORD *)(v56 + 464);
            if ( *(_DWORD *)(v2 + 1196) != 0 )
              v2 = *(_DWORD *)(v2 + 1196);
            *(_DWORD *)(v56 + 1220) = v2;
            v59 = v3[1];
            if ( v59 != 0 )
              v60 = (v58 * v59) >> 8;
            else
              v60 = v58;
            *(_DWORD *)(v56 + 464) = -v60;
            v63 = -*(_DWORD *)(v56 + 468);
            ++*(_WORD *)(v56 + 932);
            *(_DWORD *)(v56 + 468) = v63;
          }
        }
      }
      return 1;
    case 20:
      if ( *(_DWORD *)(v2 + 748) == 0 )
      {
        v50 = sub_533D00(a2[3], 0, 1, 0);
        if ( v50 != 0 )
        {
LABEL_131:
          if ( v3[3] == 0 )
            sub_51E3F0(v50);
          ++*(_WORD *)(v50 + 932);
          v51 = v3[4];
          if ( v51 != 0 )
          {
            sub_5209B0(v51, 0);
            sub_5209B0(v3[4], 0);
          }
        }
      }
      return 1;
    case 26:
      v67 = *(unsigned __int8 *)(v2 + 1231);
      if ( *(_BYTE *)(v2 + 1772) == 1 )
      {
        switch ( *(_BYTE *)(v2 + 1231) )
        {
          case 4:
            v67 = 6;
            break;
          case 6:
            v67 = 4;
            break;
          case 7:
            v67 = 9;
            break;
          case 9:
            v67 = 7;
            break;
          default:
            break;
        }
      }
      v68 = a2[1];
      if ( v3[1] == 10 )
      {
        v69 = v67;
        if ( v67 == 6 )
        {
          *(_DWORD *)(v2 + 464) += v3[2];
          *(_DWORD *)(v2 + 472) += v3[3];
          v69 = 6;
        }
        v67 = v69;
        if ( v69 == 4 )
        {
          *(_DWORD *)(v2 + 464) -= v3[2];
          *(_DWORD *)(v2 + 472) -= v3[3];
        }
      }
      else if ( v3[1] == 11 )
      {
        switch ( v67 )
        {
          case 4:
            *(_DWORD *)(v2 + 464) -= v3[2];
            break;
          case 6:
            *(_DWORD *)(v2 + 464) += v3[2];
            break;
          case 7:
            *(_DWORD *)(v2 + 464) -= v3[2];
          case 8:
            *(_DWORD *)(v2 + 472) += v3[3];
            break;
          case 9:
            *(_DWORD *)(v2 + 464) += v3[2];
            *(_DWORD *)(v2 + 472) += v3[3];
            break;
          default:
            break;
        }
      }
      else if ( v68 == v67 )
      {
        *(_DWORD *)(v2 + 464) += v3[2];
        *(_DWORD *)(v2 + 472) += v3[3];
      }
      v70 = v3[4];
      if ( v70 != 0 )
      {
        if ( v67 == 6 || v67 == 9 )
        {
          if ( *(_DWORD *)(v2 + 464) > v70 )
            *(_DWORD *)(v2 + 464) = v70;
        }
        else if ( v67 == 4 || v67 == 7 )
        {
          v71 = -v70;
          if ( *(_DWORD *)(v2 + 464) < v71 )
            *(_DWORD *)(v2 + 464) = v71;
        }
      }
      return 1;
    case 29:
      v72 = a2[2];
      if ( v72 == -30000 )
        v72 = *(_DWORD *)(v2 + 844);
      v73 = (*(int *)(v2 + 100) >> 7) + 320;
      if ( a2[3] != 0 )
      {
        if ( v72 <= v73 )
          *(_DWORD *)(v2 + 100) = (v72 - 320) << 7;
      }
      else if ( v72 >= v73 )
      {
        *(_DWORD *)(v2 + 100) = (v72 - 320) << 7;
      }
      return 1;
    case 31:
      v108 = *(_DWORD *)(v2 + 1796);
      if ( v108 != 0 && sub_591740(1, -1) != 0 )
      {
        *(_DWORD *)(v2 + 4 * v3[2] + 844) = v3[3];
        sub_42E080(v108 + 2520);
      }
      return 1;
    case 34:
      sub_591B20(v2);
      return 1;
    case 35:
      if ( sub_591A80((unsigned __int8)BYTE1(a2[1])) != 0 )
      {
        v114 = *((_QWORD *)v3 + 1);
        v115 = v3[4];
        v116 = v3[5];
        sub_591740(4, 0);
      }
      return 1;
    case 38:
      if ( *(_BYTE *)(v2 + 683) == 0 )
      {
        v74 = *(_DWORD *)(v2 + 640);
        if ( v74 != 0 && ((v74 & 0x100) == 0 || a2[2] / 100 == 0) )
        {
          v75 = a2[2];
          v108 = 100;
          switch ( (unsigned int)(v75 % 100) )
          {
            case 0u:
              v76 = (v74 & 2) == 0;
              goto LABEL_234;
            case 1u:
              v76 = (v74 & 3) == 0;
              goto LABEL_234;
            case 2u:
              v76 = (v74 & 6) == 0;
              goto LABEL_234;
            case 3u:
              v76 = (v74 & 7) == 0;
              goto LABEL_234;
            case 5u:
              v76 = (v74 & 1) == 0;
              goto LABEL_234;
            case 6u:
              v76 = (v74 & 4) == 0;
              goto LABEL_234;
            case 7u:
              v76 = (v74 & 5) == 0;
LABEL_234:
              if ( v76 )
                return 1;
              v77 = v3[3];
              if ( v77 == 0 )
                goto LABEL_241;
              v78 = v77 - 1;
              if ( v78 != 0 )
              {
                if ( v78 != 1 )
                  return 1;
                v79 = (v74 & 0x20) == 0;
              }
              else
              {
                v79 = (v74 & 0x10) == 0;
              }
              if ( !v79 )
              {
LABEL_241:
                v80 = v3[5] % 10;
                if ( v3[5] / 10 != 0 || (v81 = v107, v107 == 0) )
                  v81 = v2;
                if ( v80 != 0 )
                {
                  if ( v80 == 1 )
                    *(_DWORD *)(v81 + 4 * v3[4] + 844) += v3[1];
                }
                else
                {
                  *(_DWORD *)(v81 + 4 * v3[4] + 844) = v3[1];
                }
              }
              break;
            default:
              return 1;
          }
        }
      }
      return 1;
    case 56:
      v82 = 0;
      LODWORD(v114) = v3[1];
      v107 = v3[2];
      v83 = v3[3];
      v84 = 0;
      v113 = v83;
      v85 = 14;
      v110 = 0;
      v109 = 14;
LABEL_249:
      v86 = nullptr;
      v106 = 0;
      if ( v82 == 2 || v84 >= v85 )
        return 1;
      while ( 1 )
      {
        v108 = v85;
        if ( v82 != 0 )
        {
          if ( v82 != 1 )
            goto LABEL_255;
          v86 = (int *)((char *)&g_EffectPool + 2152 * v84);
          if ( *((_BYTE *)v86 + 2148) == 0 )
          {
LABEL_254:
            v86 = (int *)v106;
            goto LABEL_255;
          }
        }
        else
        {
          if ( byte_10F9BA4[3140 * v84] == 0 )
            goto LABEL_255;
          v86 = (int *)((char *)&g_PlayerObjects + 3140 * v84);
        }
        if ( v86 == nullptr || v86 == (int *)v2 || *((_BYTE *)v86 + 1260) == *(_BYTE *)(v2 + 1260) )
          goto LABEL_254;
        if ( v86[376] <= 0 ? *((_BYTE *)v86 + 676) == 1 : *((_BYTE *)v86 + 1492) == 0 )
          goto LABEL_254;
        v106 = (int)v86;
LABEL_255:
        ++v84;
        if ( v82 == 0 )
        {
          if ( v84 == 14 )
          {
            v82 = 1;
            v85 = 2000;
            v110 = 1;
            v84 = 0;
            v109 = 2000;
            goto LABEL_275;
          }
LABEL_274:
          v85 = v109;
          goto LABEL_275;
        }
        if ( v82 != 1 )
          goto LABEL_274;
        if ( v84 == 2000 )
          v82 = 2;
        v85 = 0;
        v110 = v82;
        if ( v84 != 2000 )
          v85 = v108;
        v109 = v85;
LABEL_275:
        if ( v86 != nullptr )
        {
          if ( v86[2] < 240 )
          {
            v88 = *(_DWORD *)(v111 + 4);
            v89 = v86[25] < *(_DWORD *)(v2 + 100);
            v2 = v112;
            if ( !v89 )
              v88 = -*(_DWORD *)(v111 + 4);
            v108 = v88;
            v106 = v88;
            if ( v107 != 0 )
            {
              v90 = v86[25] - *(_DWORD *)(v112 + 100);
              v91 = (int)((HIDWORD(v90) ^ v90) - HIDWORD(v90)) >> 7;
              if ( v107 <= v91 )
              {
                v88 = 0;
LABEL_286:
                v106 = v88;
              }
              else
              {
                v88 = v108;
                v106 = v108;
                if ( v113 != 0 )
                {
                  v88 = (int)(float)((float)(1.0 - (float)((float)v91 / (float)v107)) * (float)v108);
                  goto LABEL_286;
                }
              }
            }
            v92 = v88 + v86[25];
            v89 = (int)v114 <= 0;
            v82 = v110;
            v108 = v92;
            v86[25] = v92;
            v85 = v109;
            if ( !v89 )
            {
              if ( v106 >= 0 )
              {
                v85 = v109;
                if ( v106 > 0 )
                {
                  v82 = v110;
                  v85 = v109;
                  if ( v108 > *(_DWORD *)(v2 + 100) )
                  {
                    v86[25] = *(_DWORD *)(v2 + 100);
                    v85 = v109;
                  }
                }
              }
              else
              {
                v85 = v109;
                if ( v108 < *(_DWORD *)(v2 + 100) )
                {
                  v86[25] = *(_DWORD *)(v2 + 100);
                  v85 = v109;
                }
              }
            }
          }
          goto LABEL_249;
        }
        if ( v84 >= v85 )
          return 1;
      }
    case 70:
      v93 = a2[3] < 0;
      v106 = v2 + 104;
      if ( v93 )
      {
        v106 = v2 + 104;
      }
      else
      {
        v109 = 0;
        v110 = 0;
        v101 = *(_DWORD *)(v2 + 472);
        v97 = *(_DWORD *)(v2 + 464);
        v111 = v2 + 100;
        if ( sub_5914D0(v97, v101, &v110, &v109, 0, 0) != 0 )
          goto LABEL_303;
      }
      v93 = v3[4] < 0;
      v94 = v2 + 100;
      v111 = v2 + 100;
      if ( !v93 )
      {
        v109 = 0;
        v102 = *(_DWORD *)(v2 + 472);
        v98 = *(_DWORD *)(v2 + 464);
        v110 = 0;
        if ( sub_5914D0(v98, v102, &v110, &v109, 1, 1) != 0 )
        {
LABEL_303:
          v95 = (_DWORD *)v106;
          *(_DWORD *)v111 += v110;
          *v95 += v109;
          return 1;
        }
        v94 = v2 + 100;
      }
      v93 = v3[1] < 0;
      v111 = v94;
      if ( !v93 )
      {
        v109 = 0;
        v103 = *(_DWORD *)(v2 + 472);
        v99 = *(_DWORD *)(v2 + 464);
        v106 = v2 + 104;
        v110 = 0;
        if ( sub_5914D0(v99, v103, &v110, &v109, 2, 2) != 0 )
          goto LABEL_303;
      }
      if ( v3[2] >= 0 )
      {
        v109 = 0;
        v110 = 0;
        v104 = *(_DWORD *)(v2 + 472);
        v100 = *(_DWORD *)(v2 + 464);
        v111 = v2 + 100;
        v106 = v2 + 104;
        if ( sub_5914D0(v100, v104, &v110, &v109, 3, 3) != 0 )
          goto LABEL_303;
      }
      return 1;
    default:
      return 1;
  }
}
