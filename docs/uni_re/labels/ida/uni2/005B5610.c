int __usercall sub_5B5610@<eax>(_DWORD *a1@<edx>, int a2@<ecx>, int a3@<ebp>, int a4@<edi>, int a5@<esi>)
{
  int v5; // edi
  _DWORD *v6; // esi
  int v7; // edx
  int v8; // eax
  int v9; // eax
  int v10; // eax
  int v11; // eax
  int v12; // ecx
  int v13; // eax
  int v14; // ecx
  int v15; // edi
  int v16; // edx
  int v17; // edx
  int v18; // eax
  int v19; // edx
  char v20; // al
  char v21; // cl
  int v22; // esi
  int v23; // eax
  int v24; // esi
  int v25; // edx
  int v26; // esi
  int v27; // eax
  char v28; // al
  int v29; // eax
  bool v30; // zf
  char v31; // cl
  _BYTE *v32; // esi
  char v33; // cl
  int v34; // edx
  int v35; // ecx
  int v36; // eax
  int v37; // ecx
  int v38; // eax
  int v39; // edx
  int v40; // eax
  int v41; // ecx
  int v42; // eax
  int v43; // ecx
  int v44; // edx
  int v45; // ecx
  int v46; // eax
  int v47; // edx
  int v48; // ecx
  int v49; // edx
  int v50; // eax
  int v51; // ecx
  int v52; // eax
  _DWORD *v53; // ecx
  int v54; // et2
  int v55; // edx
  int v56; // ecx
  int v57; // eax
  float v58; // xmm1_4
  double v59; // xmm2_8
  float v60; // xmm1_4
  int v61; // ecx
  int v62; // eax
  double v63; // xmm1_8
  float v64; // xmm0_4
  int v65; // eax
  float v66; // xmm0_4
  int v67; // ecx
  int v68; // ecx
  int v69; // eax
  int v70; // edx
  int v71; // edx
  int v72; // eax
  int v73; // ecx
  int v74; // eax
  int v75; // ecx
  int v76; // esi
  int v77; // et2
  int v78; // edx
  int v79; // edx
  int v80; // ecx
  int v81; // eax
  int v82; // ecx
  int v83; // ecx
  char *v84; // ecx
  int v85; // eax
  int v86; // eax
  float v87; // xmm0_4
  int v88; // eax
  int v89; // edx
  BOOL v90; // ecx
  int v91; // eax
  char v92; // dl
  int v93; // eax
  char v94; // dl
  int v95; // ecx
  int v96; // esi
  int v97; // ecx
  int v98; // esi
  int v99; // ecx
  int v100; // ecx
  int v101; // eax
  int v102; // esi
  int v103; // ecx
  int v104; // edx
  int v105; // edx
  int v106; // ecx
  int v107; // esi
  int v108; // eax
  int v109; // esi
  int v110; // eax
  int v111; // eax
  int v112; // edi
  int v113; // eax
  int v114; // ecx
  int v115; // edx
  int v116; // ecx
  int v117; // eax
  int v118; // edx
  int v119; // ecx
  int v120; // edx
  int v121; // eax
  int v122; // ecx
  int v123; // eax
  _DWORD *v124; // ecx
  int v125; // edx
  bool v126; // sf
  int v127; // edx
  int v128; // eax
  int v129; // edx
  int v130; // ecx
  int v131; // eax
  int v132; // esi
  int v133; // ecx
  int v134; // eax
  int v135; // eax
  int v136; // edx
  int v137; // esi
  int v138; // ecx
  int v139; // edx
  int v140; // eax
  int v141; // eax
  int v142; // ecx
  float v143; // xmm0_4
  unsigned __int8 v144; // al
  unsigned __int8 v145; // cl
  int v146; // ecx
  __int64 v147; // rax
  int v148; // eax
  int v149; // edx
  int v150; // ecx
  int v151; // eax
  int v152; // eax
  int v153; // ecx
  int v154; // eax
  float v155; // xmm1_4
  float v156; // xmm1_4
  float v157; // xmm0_4
  int v158; // eax
  int v159; // esi
  int v160; // edx
  __int16 *v161; // eax
  int v162; // eax
  char v163; // al
  int v164; // eax
  int v165; // eax
  char v166; // dl
  int v167; // eax
  int v168; // ecx
  int v169; // eax
  int v170; // ecx
  int v171; // eax
  int v172; // ecx
  int v173; // eax
  int v174; // eax
  float v175; // xmm0_4
  int v176; // eax
  int v177; // ecx
  int v178; // eax
  int v179; // eax
  int v183; // [esp+Ch] [ebp-FCh]
  int v184; // [esp+10h] [ebp-F8h] BYREF
  int v185; // [esp+14h] [ebp-F4h]
  int v186; // [esp+18h] [ebp-F0h] BYREF
  int v187; // [esp+1Ch] [ebp-ECh] BYREF
  _DWORD *v188; // [esp+20h] [ebp-E8h]
  __int64 v189; // [esp+24h] [ebp-E4h] BYREF
  double v190; // [esp+2Ch] [ebp-DCh] BYREF
  int v191; // [esp+34h] [ebp-D4h]
  int v192; // [esp+38h] [ebp-D0h]
  int v193; // [esp+3Ch] [ebp-CCh] BYREF
  _DWORD v194[16]; // [esp+7Ch] [ebp-8Ch] BYREF
  double v195; // [esp+BCh] [ebp-4Ch] BYREF
  int v196; // [esp+C4h] [ebp-44h]
  _DWORD v197[4]; // [esp+CCh] [ebp-3Ch] BYREF
  _DWORD v198[2]; // [esp+DCh] [ebp-2Ch] BYREF
  int v199; // [esp+E4h] [ebp-24h] BYREF
  int v200; // [esp+E8h] [ebp-20h] BYREF
  double v201; // [esp+ECh] [ebp-1Ch] BYREF
  int v202; // [esp+FCh] [ebp-Ch]
  void *v203; // [esp+100h] [ebp-8h]
  void *retaddr; // [esp+108h] [ebp+0h]

  v202 = a3;
  v203 = retaddr;
  v5 = a2;
  v6 = a1;
  v192 = (int)a1;
  v7 = 1;
  if ( *(_DWORD *)(a2 + 1012) != 0 )
    a2 = *(_DWORD *)(a2 + 1012);
  v8 = v6[1];
  v191 = a2;
  if ( v8 > 300 )
  {
    if ( v8 > 311 )
    {
      if ( v8 == 1100 )
      {
        v179 = v6[2];
        if ( v179 != 0 )
        {
          if ( v179 == 1 )
          {
            *(_DWORD *)(v5 + 292) = -*(_DWORD *)(v5 + 292);
            *(_DWORD *)(v5 + 296) = -*(_DWORD *)(v5 + 296);
          }
        }
        else
        {
LABEL_121:
          *(_DWORD *)(v5 + 284) = -*(_DWORD *)(v5 + 284);
          *(_DWORD *)(v5 + 288) = -*(_DWORD *)(v5 + 288);
        }
      }
    }
    else if ( v8 == 311 )
    {
      if ( *(int *)(v5 + 8) >= 240 )
      {
        v176 = v6[4] | (v6[5] << 16);
        if ( (v6[4] & 4) != 0 )
        {
          v177 = *(_DWORD *)(v5 + 124);
          if ( (v177 & 4) != 0 )
            *(_DWORD *)(v5 + 124) = v177 & 0xFFFFFFFB;
        }
        v178 = v176 & 0x30BDF069;
        if ( v178 != 0 )
          *(_DWORD *)(v5 + 124) &= ~v178;
      }
    }
    else
    {
      v173 = v8 - 301;
      if ( v173 != 0 )
      {
        v174 = v173 - 1;
        if ( v174 != 0 )
        {
          if ( v174 == 8 && *(int *)(v5 + 8) >= 240 )
            sub_5B7C60(v5, v6[4] | (v6[5] << 16));
        }
        else
        {
          dword_9A4FB8 = v6[2] != 0;
        }
      }
      else
      {
        v30 = v6[8] == 0;
        v197[0] = 0;
        v197[1] = 0;
        if ( v30 )
        {
          v175 = (float)(int)v6[7];
          v198[0] = *(_DWORD *)(v5 + 100) + (v6[2] << 7);
          v198[1] = *(_DWORD *)(v5 + 104) + (v6[3] << 7);
          *(float *)&v199 = v175 / 10000.0;
        }
        else
        {
          *(float *)&v199 = 0.0;
          v198[0] = -12345678;
          v198[1] = -12345678;
        }
        v200 = v6[4];
        v201 = *(double *)(v6 + 5);
        v197[3] = 0;
        v197[2] = 0;
        sub_535150(v197);
      }
    }
  }
  else if ( v8 == 300 )
  {
    v168 = v6[12];
    if ( (v168 & 1) != 0 )
    {
      v169 = v6[2];
      if ( (v168 & 4) == 0 && *(_BYTE *)(v5 + 1604) == 1 )
        v169 = -v169;
      v170 = v6[13];
      if ( v170 == 0 )
      {
        *(_DWORD *)(v5 + 276) = v169;
        v170 = v6[13];
      }
      if ( v170 == 1 )
        *(_DWORD *)(v5 + 276) += v169;
    }
    if ( (v6[12] & 2) != 0 )
    {
      v171 = v6[13];
      v172 = v6[3];
      if ( v171 == 0 )
      {
        *(_DWORD *)(v5 + 280) = v172;
        v171 = v6[13];
      }
      if ( v171 == 1 )
        *(_DWORD *)(v5 + 280) += v172;
    }
  }
  else
  {
    switch ( v8 )
    {
      case 1:
        v9 = v6[12];
        if ( v9 != 0 && (v9 & 1) != 0 )
        {
          dword_C5CE04 = 0;
          dword_C5CE00 = 0;
          dword_C5CE0C = 0;
          dword_C5CE08 = 0;
          dword_C5CE14 = 0;
          dword_C5CE10 = 0;
          dword_C5CE1C = 0;
          dword_C5CE18 = 0;
          dword_C5CE24 = 0;
          dword_C5CE20 = 0;
          dword_C5CE2C = 0;
          dword_C5CE28 = 0;
          dword_C5CE34 = 0;
          dword_C5CE30 = 0;
          dword_C5CE3C = 0;
          dword_C5CE38 = 0;
          dword_C5CDFC = 0;
        }
        v10 = v6[2];
        if ( v10 != 0 )
          sub_5356D0(v10, v6[6], v6[7]);
        v11 = v6[5];
        if ( v11 != 0 )
        {
          v198[0] = -1;
          v201 = 0.0;
          v200 = 0;
          if ( (_WORD)v11 != 0 )
          {
            v198[1] = (unsigned __int16)v11;
            v199 = 4000;
            sub_4362B0(v198, 0);
          }
        }
        return 1;
      case 2:
        *(_BYTE *)(v5 + 512) = *((_BYTE *)v6 + 8);
        *(_BYTE *)(v5 + 513) = *((_BYTE *)v6 + 12);
        return 1;
      case 4:
        if ( ((int (*)(void))sub_424CA0)() == 0 && *(_BYTE *)(v12 + 120) == 0 )
        {
          v13 = sub_424A80(v12);
          v30 = v6[2] == 0;
          v14 = v191;
          HIDWORD(v190) = v13;
          if ( !v30 )
          {
            v15 = *(_DWORD *)(v191 + 144);
            v16 = *(_DWORD *)(v191 + 140);
            if ( v15 > v16 )
            {
              if ( sub_424CA0(v191) == 0 )
                v17 += v6[2];
              v18 = v17;
              if ( v17 >= v15 )
                v17 = v15;
              if ( v17 >= 0 )
              {
                v16 = v15;
                if ( v18 < v15 )
                  v16 = v18;
              }
              else
              {
                v16 = 0;
              }
              v13 = HIDWORD(v190);
              *(_DWORD *)(v14 + 140) = v16;
            }
            if ( v16 > v13 )
              *(_DWORD *)(v14 + 140) = v13;
          }
          if ( v6[3] != 0 && sub_424CA0(v14) == 0 )
            goto LABEL_32;
        }
        return 1;
      case 5:
        v19 = v6[2];
        v20 = *(_BYTE *)(v5 + 1047);
        switch ( v19 )
        {
          case 0:
            v21 = *(_BYTE *)(v5 + 1604);
            *(_BYTE *)(v5 + 1605) = v21 == 0;
            *(_BYTE *)(v5 + 1604) = v21 == 0;
            if ( (v21 == 0) != v21 )
            {
              sub_42AD90(v5 + 1044);
              sub_42AD90(v5 + 1060);
              *(_BYTE *)(v5 + 1607) = 0;
            }
            goto LABEL_67;
          case 1:
            v22 = v6[4];
            v23 = sub_530CD0(0);
            if ( v23 != 0 )
            {
              v24 = v22 << 8;
              v25 = *(_DWORD *)(v23 + 112) + *(_DWORD *)(v23 + 100) - v24;
              v26 = *(_DWORD *)(v23 + 112) + *(_DWORD *)(v23 + 100) + v24;
              if ( *(_BYTE *)(v23 + 1604) != 0 )
                v26 = v25;
              v27 = *(_DWORD *)(v5 + 112) + *(_DWORD *)(v5 + 100);
              if ( v27 <= v26 )
              {
                if ( v27 >= v26 )
                  *(_BYTE *)(v5 + 1605) = *(_BYTE *)(v5 + 1604);
                else
                  *(_BYTE *)(v5 + 1605) = 0;
              }
              else
              {
                *(_BYTE *)(v5 + 1605) = 1;
              }
            }
            v28 = *(_BYTE *)(v5 + 1605);
            *(_BYTE *)(v5 + 1606) = v28;
            v29 = sub_54CCA0(v28, 1);
            goto LABEL_66;
          case 2:
            if ( v20 == 7 || v20 == 4 )
              goto LABEL_49;
            v30 = v20 == 1;
            goto LABEL_48;
          case 3:
            if ( v20 != 9 && v20 != 6 )
            {
              v30 = v20 == 3;
LABEL_48:
              if ( !v30 )
                return 1;
            }
LABEL_49:
            v31 = *(_BYTE *)(v5 + 1604);
            *(_BYTE *)(v5 + 1604) = v31 == 0;
            *(_BYTE *)(v5 + 1605) = v31 == 0;
            if ( (v31 == 0) != v31 )
            {
              sub_42AD90(v5 + 1044);
              sub_42AD90(v5 + 1060);
              *(_BYTE *)(v5 + 1607) = 0;
            }
            if ( v6[3] == 0 )
            {
              v32 = *(_BYTE **)(v5 + 1004);
              if ( v32 != nullptr )
              {
                v33 = v32[1604];
                v32[1605] = v33 == 0;
                v32[1604] = v33 == 0;
                if ( (v33 == 0) != v33 )
                {
                  sub_42AD90(v32 + 1044);
                  sub_42AD90(v32 + 1060);
                  v32[1607] = 0;
                }
              }
            }
LABEL_67:
            *(_WORD *)(v5 + 456) = -*(_WORD *)(v5 + 456);
            sub_539DD0(v5);
            break;
          case 4:
          case 5:
            v29 = sub_54CCA0(v19 - 4, 1);
            goto LABEL_66;
          case 6:
            v34 = 1;
            v35 = 21;
            if ( dword_C48D60 + 1 < 56 )
              v34 = dword_C48D60 + 1;
            dword_C48D60 = v34;
            if ( v34 > 34 )
              v35 = -34;
            v36 = dword_C48D64[v34] - dword_C48D64[v34 + v35];
            v37 = v36 + 0x7FFFFFFF;
            if ( v36 >= 0 )
              v37 = v36;
            dword_C48D64[v34] = v37;
            ++dword_C48E44;
            v29 = sub_54CCA0(v37 % 2, 1);
LABEL_66:
            if ( v29 != 0 )
              goto LABEL_67;
            return 1;
          case 7:
            sub_54CCA0(*(char *)(a2 + 1604), 1);
            return 1;
          default:
            return 1;
        }
        return 1;
      case 6:
        v38 = v6[13];
        v39 = v5 + 284;
        if ( v38 != 0 )
        {
          v40 = v38 - 1;
          if ( v40 != 0 )
          {
            if ( v40 == 1 )
            {
              v41 = v6[2];
              HIDWORD(v189) = v41;
              if ( v41 >= 2 )
              {
                HIDWORD(v190) = v6[3];
                if ( SHIDWORD(v190) > 0 )
                {
                  v42 = v41 / 2;
                  v43 = v41 / 2 * (v41 + 1);
                  v44 = v43 + v42;
                  if ( (v189 & 0x100000000LL) == 0 )
                    v44 = v43;
                  v45 = v191;
                  *(_DWORD *)(v5 + 292) = (*(_DWORD *)(v191 + 104)
                                         + (v6[4] << 7)
                                         - HIDWORD(v190) * v44
                                         - *(_DWORD *)(v5 + 104))
                                        / SHIDWORD(v189);
                  v46 = (*(_DWORD *)(v45 + 100) - *(_DWORD *)(v5 + 100)) / SHIDWORD(v189);
                  *(_DWORD *)(v5 + 288) = 0;
                  *(_DWORD *)(v5 + 284) = v46;
                  *(_DWORD *)(v5 + 296) = HIDWORD(v190);
                }
              }
            }
          }
          else
          {
            v30 = v6[6] == 0;
            v47 = v6[5];
            v48 = 21;
            v192 = v47;
            HIDWORD(v190) = v6 + 6;
            if ( !v30 )
            {
              v49 = 1;
              if ( dword_C48D60 + 1 < 56 )
                v49 = dword_C48D60 + 1;
              v50 = 21;
              dword_C48D60 = v49;
              if ( v49 > 34 )
                v50 = -34;
              v51 = dword_C48D64[v49] - dword_C48D64[v49 + v50];
              v52 = v51 + 0x7FFFFFFF;
              if ( v51 >= 0 )
                v52 = v51;
              v53 = (_DWORD *)HIDWORD(v190);
              dword_C48D64[v49] = v52;
              ++dword_C48E44;
              v54 = v52 % *v53;
              v48 = 21;
              v192 += v54;
              v47 = v192;
            }
            if ( *(_BYTE *)(v5 + 1604) == 1 && v47 != 0 )
            {
              v47 = 10000 - v47;
              v192 = v47;
            }
            v30 = v6[3] == 0;
            HIDWORD(v189) = v6[2];
            HIDWORD(v190) = v6 + 3;
            if ( !v30 )
            {
              v55 = 1;
              if ( dword_C48D60 + 1 < 56 )
                v55 = dword_C48D60 + 1;
              dword_C48D60 = v55;
              if ( v55 > 34 )
                v48 = -34;
              v191 = v55 + v48;
              v56 = dword_C48D64[v55] - dword_C48D64[v55 + v48];
              v57 = v56 + 0x7FFFFFFF;
              if ( v56 >= 0 )
                v57 = v56;
              v48 = HIDWORD(v190);
              dword_C48D64[v55] = v57;
              ++dword_C48E44;
              HIDWORD(v189) += v57 % *(_DWORD *)v48;
              v47 = v192;
            }
            v201 = (float)((float)v47 / 10000.0) * 3.14159265 + (float)((float)v47 / 10000.0) * 3.14159265;
            v190 = *(double *)libm_sse2_sin_precise(v48).m128_u64;
            v195 = (float)SHIDWORD(v189);
            v58 = v190 * v195;
            *(_DWORD *)(v5 + 284) = (int)v58;
            v59 = *(double *)libm_sse2_cos_precise().m128_u64;
            v60 = -(v59 * v195);
            *(_DWORD *)(v5 + 292) = (int)v60;
            v61 = v6[4];
            if ( v6[7] == 1 )
            {
              v62 = HIDWORD(v189);
              if ( v61 != 0 )
                v62 = -(SHIDWORD(v189) / v61);
            }
            else
            {
              v62 = v6[4];
            }
            v63 = (float)v62;
            v64 = v190 * v63;
            v65 = (int)v64;
            v66 = -(v59 * v63);
            *(_DWORD *)(v5 + 288) = (__int16)v65;
            *(_DWORD *)(v5 + 296) = (__int16)(int)v66;
          }
        }
        else
        {
          v67 = v6[6];
          v30 = v67 == 0;
          v188 = (_DWORD *)(v39 + 8 * (v67 != 0) + 4);
          v68 = 21;
          if ( !v30 )
            v39 = v5 + 292;
          v69 = v6[3];
          HIDWORD(v190) = v39;
          v70 = v6[2];
          if ( v69 != v70 )
          {
            v71 = 1;
            if ( dword_C48D60 + 1 < 56 )
              v71 = dword_C48D60 + 1;
            v72 = 21;
            dword_C48D60 = v71;
            if ( v71 > 34 )
              v72 = -34;
            v73 = dword_C48D64[v71] - dword_C48D64[v71 + v72];
            v74 = v73 + 0x7FFFFFFF;
            if ( v73 >= 0 )
              v74 = v73;
            v75 = v192;
            dword_C48D64[v71] = v74;
            ++dword_C48E44;
            v76 = v6[2];
            v77 = v74 % (*(_DWORD *)(v75 + 12) - v76);
            v68 = 21;
            v70 = v76 + v77;
            v6 = (_DWORD *)v192;
          }
          *(_DWORD *)HIDWORD(v190) = v70;
          v78 = v6[4];
          if ( v6[5] != v78 )
          {
            v79 = 1;
            if ( dword_C48D60 + 1 < 56 )
              v79 = dword_C48D60 + 1;
            dword_C48D60 = v79;
            if ( v79 > 34 )
              v68 = -34;
            v191 = v79 + v68;
            v80 = dword_C48D64[v79] - dword_C48D64[v79 + v68];
            v81 = v80 + 0x7FFFFFFF;
            if ( v80 >= 0 )
              v81 = v80;
            v82 = v192;
            dword_C48D64[v79] = v81;
            ++dword_C48E44;
            LOWORD(v78) = v6[4] + v81 % (*(_DWORD *)(v82 + 20) - v6[4]);
            v6 = (_DWORD *)v192;
            v78 = (__int16)v78;
          }
          *v188 = v78;
          if ( v6[6] == 0 && *(_BYTE *)(v5 + 1604) == 1 )
            goto LABEL_121;
        }
        return 1;
      case 8:
        v83 = *(_DWORD *)(a2 + 992);
        HIDWORD(v189) = v83;
        if ( v83 != 0 && sub_530F70(v83, 1) != 0 )
        {
          v84 = (char *)HIDWORD(v189);
        }
        else
        {
          v84 = (char *)&g_CharaObjects;
          HIDWORD(v189) = &g_CharaObjects;
          while ( 1 )
          {
            if ( v84[2008] != 0 )
            {
              v85 = sub_530F70(v84, v7);
              v84 = (char *)HIDWORD(v189);
              if ( v85 != 0 && *(_BYTE *)(HIDWORD(v189) + 1076) != *(_BYTE *)(v5 + 1076) )
                break;
            }
            v84 += 3008;
            HIDWORD(v189) = v84;
            if ( (int)v84 >= (int)byte_106DF80 )
              return 1;
          }
        }
        if ( v84 != nullptr )
        {
          sub_5B96A0(v84, (char *)&v190 + 4, (char *)&v201 + 4);
          Matrix4_SetIdentity(v194);
          v86 = D3DXMatrixRotationZ(&v193, *((float *)&v190 + 1) * 0.017453292f, a4, a5, v183);
          D3DXMatrixMultiply(v194, v194, v86);
          v200 = 1065353216;
          v201 = 0.0;
          D3DXVec3TransformCoord(&v195, &v200, v194);
          *(_DWORD *)(v5 + 284) += (int)(float)((float)(int)v6[3] * *(float *)&v195);
          v87 = (float)(int)v6[3];
          *(_DWORD *)(v5 + 296) = 0;
          *(_DWORD *)(v5 + 288) = 0;
          *(_DWORD *)(v5 + 292) += (int)(float)(v87 * *((float *)&v195 + 1));
          if ( v6[4] != 0 )
            Object_OnFrameChanged(v5);
        }
        return 1;
      case 9:
        sub_54B840(v6[2] == 0);
        return 1;
      case 11:
        if ( *(_DWORD *)(a2 + 8) == 0 && v6[3] != 0 )
          sub_533380(v6[2]);
        return 1;
      case 12:
        v188 = (_DWORD *)v6[12];
        v88 = v6[13];
        if ( v88 != 0 )
        {
          switch ( v88 )
          {
            case 1:
              v101 = v6[2];
              v102 = v6[3];
              HIDWORD(v201) = v101 << 7;
              sub_5371C0(&g_BtlCamera);
              v103 = v192;
              v104 = HIDWORD(v201);
              *(_DWORD *)(v5 + 100) = HIDWORD(v201);
              *(_DWORD *)(v5 + 104) = v102 << 7;
              if ( (*(_BYTE *)(v103 + 16) & 1) != 0 && *(_BYTE *)(v191 + 1604) != 0 )
                *(_DWORD *)(v5 + 100) = -v104;
              break;
            case 2:
              v105 = *(_DWORD *)(v5 + 1016);
              v106 = v6[2] << 7;
              v107 = v6[3] << 7;
              if ( v105 == 0 )
                v105 = v191;
              v108 = -v106;
              if ( *(_BYTE *)(v105 + 1604) == 0 )
                v108 = v106;
              *(_DWORD *)(v5 + 100) = *(_DWORD *)(v105 + 100) + *(_DWORD *)(v105 + 112) + v108;
              *(_DWORD *)(v5 + 104) = v107 + *(_DWORD *)(v105 + 104) + *(_DWORD *)(v105 + 116);
              break;
            case 3:
              v109 = g_BtlCamera_StageW >> 1 << 7;
              HIDWORD(v201) = -128 * (g_BtlCamera_StageW >> 1);
              v110 = sub_533580(v5);
              if ( v110 == 1 )
              {
                *(_DWORD *)(v5 + 100) = v109;
              }
              else if ( v110 == 2 )
              {
                *(_DWORD *)(v5 + 100) = HIDWORD(v201);
              }
              break;
            default:
              break;
          }
        }
        else
        {
          v89 = v6[6];
          v191 = v89;
          v192 = (v89 & 0x10) != 0;
          if ( (v89 & 2) != 0 )
          {
            if ( (v89 & 4) == 0 )
              *(_DWORD *)(a2 + 100) = *(_DWORD *)(v5 + 100);
            if ( (v89 & 8) == 0 )
              *(_DWORD *)(a2 + 104) = *(_DWORD *)(v5 + 104);
          }
          else
          {
            v30 = v6[5] == 1;
            HIDWORD(v189) = 0;
            if ( !v30 )
              v5 = a2;
            if ( (v89 & 0x10) != 0 )
            {
              v90 = v192;
            }
            else
            {
              v90 = *(_BYTE *)(v5 + 1604) == 0;
              v192 = v90;
            }
            v91 = v6[2] << 7;
            HIDWORD(v190) = v6[3] << 7;
            v92 = v191;
            v196 = v91;
            if ( (v191 & 1) != 0 )
            {
              v93 = sub_530C60(v5, (char *)&v189 + 4);
              v94 = v191;
              if ( v93 != 0 )
              {
                if ( (v191 & 4) == 0 )
                  *(_DWORD *)(v5 + 100) = *(_DWORD *)(v93 + 100) + *(_DWORD *)(v93 + 112);
                if ( (v94 & 8) == 0 )
                  *(_DWORD *)(v5 + 104) = 0;
              }
              if ( (v94 & 4) == 0 )
              {
                v95 = v196;
                if ( v192 == 0 )
                  v95 = -v196;
                v187 = v95;
                if ( v188 != nullptr )
                {
                  sub_5B6EA0(&v187);
                  v95 = v187;
                  v94 = v191;
                }
                *(_DWORD *)(v5 + 100) += v95;
              }
              if ( (v94 & 8) == 0 )
                *(_DWORD *)(v5 + 104) += HIDWORD(v190);
            }
            else
            {
              HIDWORD(v189) = v191 & 4;
              if ( v6[4] != 0 )
              {
                if ( HIDWORD(v189) == 0 )
                {
                  v98 = *(_DWORD *)(v5 + 100);
                  v99 = -v91;
                  if ( v192 == 0 )
                    v99 = v91;
                  v100 = dword_C5CCA8 + v99 - v98;
                  v186 = v100;
                  if ( v188 != nullptr )
                  {
                    sub_5B6EA0(&v186);
                    v98 = *(_DWORD *)(v5 + 100);
                    v100 = v186;
                    v92 = v191;
                  }
                  *(_DWORD *)(v5 + 100) = v98 + v100;
                }
                if ( (v92 & 8) == 0 )
                  *(_DWORD *)(v5 + 104) = HIDWORD(v190);
              }
              else
              {
                v196 = *(_DWORD *)(v5 + 100);
                HIDWORD(v201) = *(_DWORD *)(v5 + 104);
                v96 = 0;
                v192 = 0;
                v185 = 0;
                if ( HIDWORD(v189) != 0 )
                  goto LABEL_169;
                v96 = v91;
                if ( !v90 )
                  v96 = -v91;
                v184 = v96;
                if ( v188 == nullptr )
                {
LABEL_169:
                  v97 = v192;
                }
                else
                {
                  sub_5B6EA0(&v184);
                  v97 = v185;
                  v96 = v184;
                  v92 = v191;
                }
                if ( (v92 & 8) == 0 )
                  v97 = HIDWORD(v190);
                *(_DWORD *)(v5 + 100) = v96 + v196;
                *(_DWORD *)(v5 + 104) = v97 + HIDWORD(v201);
              }
            }
          }
        }
        return 1;
      case 14:
        if ( dword_C7A658 == 0 )
        {
          v111 = sub_5620F0(2, v6[2], 1, 0);
          v112 = v111;
          if ( v111 != 0 )
          {
            if ( v6[3] != 0 )
            {
              v113 = sub_424A80(v111);
              v114 = v113;
              v115 = v6[3] + *(_DWORD *)(v112 + 140);
              if ( v115 < v113 )
                v114 = v6[3] + *(_DWORD *)(v112 + 140);
              if ( v114 >= 0 )
              {
                if ( v115 < v113 )
                  v113 = v6[3] + *(_DWORD *)(v112 + 140);
              }
              else
              {
                v113 = 0;
              }
              *(_DWORD *)(v112 + 140) = v113;
            }
            if ( v6[4] != 0 )
LABEL_32:
              sub_53C7C0(1);
          }
        }
        return 1;
      case 15:
        v116 = dword_9A3E0C;
        if ( v6[2] == 0 )
          v116 = 0;
        sub_4D67B0(v116);
        return 1;
      case 16:
        v117 = v6[13];
        if ( v117 != 0 )
        {
          if ( v117 == 1 )
          {
            v133 = dword_C5CCAC + (v6[3] << 7) - *(_DWORD *)(v5 + 104);
            v134 = dword_C5CCA8 + (v6[2] << 7) - *(_DWORD *)(v5 + 100);
            *(_DWORD *)(v5 + 284) = v134;
            *(_DWORD *)(v5 + 288) = 0;
            *(_DWORD *)(v5 + 292) = v133;
            *(_DWORD *)(v5 + 296) = 0;
            HIDWORD(v201) = v6[4];
            if ( SHIDWORD(v201) >= 2 )
            {
              *(_DWORD *)(v5 + 284) = v134 / SHIDWORD(v201);
              *(_DWORD *)(v5 + 292) = v133 / v6[4];
            }
          }
        }
        else
        {
          v30 = v6[3] == 0;
          v118 = v6[2];
          v119 = 21;
          v192 = v118;
          HIDWORD(v201) = v6 + 3;
          if ( !v30 )
          {
            v120 = 1;
            if ( dword_C48D60 + 1 < 56 )
              v120 = dword_C48D60 + 1;
            v121 = 21;
            dword_C48D60 = v120;
            if ( v120 > 34 )
              v121 = -34;
            v122 = dword_C48D64[v120] - dword_C48D64[v120 + v121];
            v123 = v122 + 0x7FFFFFFF;
            if ( v122 >= 0 )
              v123 = v122;
            v124 = (_DWORD *)HIDWORD(v201);
            dword_C48D64[v120] = v123;
            ++dword_C48E44;
            v125 = v123 % *v124 + v192;
            v119 = 21;
            v118 = 360
                 * (((int)(((unsigned __int64)(1240768329LL * v125) >> 32) - v125) >> 8)
                  + ((unsigned int)(((unsigned __int64)(1240768329LL * v125) >> 32) - v125) >> 31))
                 + v125;
            v192 = v118;
          }
          if ( *(_BYTE *)(v5 + 1604) == 1 )
          {
            v126 = 180 - v118 < 0;
            v127 = 180 - v118;
            v192 = v127;
            if ( v126 )
              v192 = v127 + 360;
          }
          v30 = v6[5] == 0;
          v128 = v6[4];
          HIDWORD(v190) = v128;
          if ( v30 )
          {
            v132 = v128;
          }
          else
          {
            v129 = 1;
            if ( dword_C48D60 + 1 < 56 )
              v129 = dword_C48D60 + 1;
            dword_C48D60 = v129;
            if ( v129 > 34 )
              v119 = -34;
            v191 = v129 + v119;
            v130 = dword_C48D64[v129] - dword_C48D64[v129 + v119];
            v131 = v130 + 0x7FFFFFFF;
            if ( v130 >= 0 )
              v131 = v130;
            dword_C48D64[v129] = v131;
            ++dword_C48E44;
            v132 = v131 % (v6[5] + 1) + HIDWORD(v190);
          }
          *(_DWORD *)(v5 + 292) = (int)(float)(Math_SinPrecise() * (float)v132);
          *(_DWORD *)(v5 + 284) = (int)(float)(Math_CosPrecise() * (float)v132);
        }
        return 1;
      case 19:
        v135 = *(unsigned __int8 *)(a2 + 1076);
        goto LABEL_237;
      case 21:
        dword_C5CCBC = dword_C5CCD4;
        dword_C5CCC0 = dword_C5CCD8;
        dword_C5CCC4 = dword_C5CCDC;
        dword_C5CCC8 = dword_C5CCE0;
        dword_C5CCCC = dword_C5CCE4;
        return 1;
      case 22:
        v140 = 0;
        if ( v6[3] != 0 )
          v140 = sub_419800(v6[3]);
        v141 = v6[2] + v140;
        v142 = v6[7];
        if ( v142 != 0 )
        {
          switch ( v142 )
          {
            case 1:
              *(float *)(v5 + 1600) = (float)v141 / 10000.0;
              break;
            case 2:
              v143 = (float)v141 / 10000.0;
              *(float *)(v5 + 1600) = v143;
              *(float *)(v5 + 1596) = v143;
              break;
            case 10:
              *(_DWORD *)(v5 + 1592) = v141;
              break;
            default:
              break;
          }
        }
        else
        {
          *(float *)(v5 + 1596) = (float)v141 / 10000.0;
        }
        return 1;
      case 102:
        *(_BYTE *)(v5 + 1658) += *((_BYTE *)v6 + 8);
        return 1;
      case 103:
        v144 = *(_BYTE *)(v5 + 1658);
        v145 = *((_BYTE *)v6 + 8);
        if ( v144 > v145 )
          *(_BYTE *)(v5 + 1658) = v144 - v145;
        else
          *(_BYTE *)(v5 + 1658) = 0;
        return 1;
      case 105:
        if ( v6[4] / 10 != 0 || (v146 = *(_DWORD *)(v5 + 1012)) == 0 )
          v146 = v5;
        v147 = sub_530FB0(v146);
        if ( HIDWORD(v147) != 0 )
        {
          if ( HIDWORD(v147) == 1 )
            *(_DWORD *)(v147 + 4 * v6[2] + 660) += v6[3];
        }
        else
        {
          *(_DWORD *)(v147 + 4 * v6[2] + 660) = v6[3];
        }
        return 1;
      case 106:
        v148 = v6[2];
        if ( v148 == 0 )
          return 1;
        v149 = v6[3];
        v150 = *(_DWORD *)(v5 + 124);
        if ( v149 == 0 )
          goto LABEL_278;
        if ( v149 == 1 )
        {
          *(_DWORD *)(v5 + 124) = v148 | v150;
        }
        else
        {
          if ( v149 == 2 )
          {
            v148 &= v150;
LABEL_278:
            v150 &= ~v148;
          }
          *(_DWORD *)(v5 + 124) = v150;
        }
        return 1;
      case 111:
        v151 = v6[2];
        if ( v151 != 0 )
        {
          if ( v151 == 1 )
          {
            *(_OWORD *)(v5 + 284) = *(_OWORD *)(v5 + 368);
            *(_QWORD *)(v5 + 300) = *(_QWORD *)(v5 + 384);
          }
        }
        else
        {
          *(_OWORD *)(v5 + 368) = *(_OWORD *)(v5 + 284);
          *(_QWORD *)(v5 + 384) = *(_QWORD *)(v5 + 300);
          if ( v6[3] != 0 )
          {
            *(_DWORD *)(v5 + 288) = 0;
            *(_DWORD *)(v5 + 284) = 0;
            *(_DWORD *)(v5 + 300) = 0;
            *(_DWORD *)(v5 + 296) = 0;
            *(_DWORD *)(v5 + 292) = 0;
            *(_DWORD *)(v5 + 304) = 0;
          }
          v152 = v6[4];
          if ( v152 != 0 && *(int *)(v5 + 284) <= 0 && *(int *)(v5 + 296) <= 0 )
            *(_DWORD *)(v5 + 380) = v152;
        }
        return 1;
      case 112:
        v135 = *(unsigned __int8 *)(v5 + 1076);
LABEL_237:
        v136 = v6[3];
        v137 = v6[2];
        v138 = dword_C59B04[54 * v135];
        if ( v138 <= 0 )
        {
          dword_C59B04[54 * v135] = 100;
          v138 = 100;
        }
        if ( v137 != 0 )
        {
          v139 = v136 - 1;
          if ( v139 != 0 )
          {
            if ( v139 == 1 )
            {
              v138 -= v137;
            }
            else if ( v138 >= v137 )
            {
              v138 = v137;
            }
          }
          else
          {
            v138 = v137 * v138 / 100;
          }
          dword_C59B08[54 * v135] = v137;
          if ( v138 < 1 )
            v138 = 1;
          dword_C59B04[54 * v135] = v138;
        }
        return 1;
      case 115:
        v153 = 1;
        if ( v6[2] != 0 )
          v153 = v6[2];
        v154 = *(_DWORD *)(v5 + 284) / v153;
        *(_OWORD *)(v5 + 344) = *(_OWORD *)(v5 + 284);
        *(_QWORD *)(v5 + 360) = *(_QWORD *)(v5 + 300);
        *(_DWORD *)(v5 + 348) = -v154;
        *(_DWORD *)(v5 + 356) = 0;
        *(_DWORD *)(v5 + 352) = 0;
        *(_DWORD *)(v5 + 364) = 0;
        *(_DWORD *)(v5 + 288) = 0;
        *(_DWORD *)(v5 + 284) = 0;
        *(_DWORD *)(v5 + 300) = 0;
        return 1;
      case 116:
        v155 = (float)*(int *)(v5 + 1592) / 10000.0;
        *((float *)&v190 + 1) = (float)(int)v6[2] * 0.0078125;
        if ( *(_BYTE *)(v5 + 1604) == 1 )
          v155 = 1.0 - v155;
        v201 = v155 * 3.14159265 + v155 * 3.14159265;
        v156 = *(double *)((__m128 (*)(void))libm_sse2_sin_precise)().m128_u64 * *((float *)&v190 + 1);
        *(_DWORD *)(v5 + 284) = (int)(float)(v156 * 128.0);
        v157 = -(*(double *)libm_sse2_cos_precise().m128_u64 * *((float *)&v190 + 1));
        *(_DWORD *)(v5 + 292) = (int)(float)(v157 * 128.0);
        return 1;
      case 117:
        HIDWORD(v201) = v6[2];
        v158 = v6[3];
        v189 = 0;
        if ( v158 == 0 )
          v158 = 1;
        HIDWORD(v189) = a2;
        HIDWORD(v190) = v158;
        v188 = (_DWORD *)v6[6];
        v196 = (unsigned __int8)v188 & 1;
        v159 = sub_434260(&v189);
        if ( v159 == 0 )
          return 1;
        break;
      case 153:
        *(_WORD *)(v5 + 470) = 66;
        return 1;
      case 252:
        *(_BYTE *)(a2 + 490) = *((_BYTE *)v6 + 8);
        return 1;
      case 254:
        v165 = sub_426060(v5);
        if ( v165 != 0 )
          *(_BYTE *)(v165 + 2300) |= v166;
        return 1;
      case 255:
        v167 = sub_426060(v5);
        if ( v167 != 0 )
          *(_BYTE *)(v167 + 2300) |= 2u;
        return 1;
      default:
        return 1;
    }
    while ( 1 )
    {
      v160 = v159;
      if ( *(_DWORD *)(v159 + 1012) != 0 )
        v160 = *(_DWORD *)(v159 + 1012);
      if ( v191 == v160 )
      {
        v161 = (__int16 *)sub_426470(HIDWORD(v201) + 9);
        if ( v161 != nullptr )
          break;
      }
      v159 = sub_434260(&v189);
      if ( v159 == 0 )
        return 1;
    }
    v199 = *v161;
    v200 = v161[1];
    LODWORD(v201) = v161[2];
    HIDWORD(v201) = v161[3];
    sub_426570(&v199);
    if ( v196 == 0 )
    {
      v162 = (v199 - *(_DWORD *)(v5 + 100)) / SHIDWORD(v190);
      *(_DWORD *)(v5 + 284) = v162;
      *(_DWORD *)(v5 + 300) = v162;
    }
    v163 = (char)v188;
    if ( ((unsigned __int8)v188 & 2) == 0 )
    {
      v164 = (v200 - *(_DWORD *)(v5 + 104)) / SHIDWORD(v190);
      *(_DWORD *)(v5 + 292) = v164;
      *(_DWORD *)(v5 + 304) = v164;
      v163 = (char)v188;
    }
    if ( (v163 & 4) != 0 )
      *(_DWORD *)(v5 + 1592) = (int)(float)(sub_5B97F0().m128_f32[0] * 10000.0);
  }
  return 1;
}
