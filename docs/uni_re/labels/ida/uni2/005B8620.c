int __fastcall sub_5B8620(int a1, _DWORD *a2)
{
  int v4; // ebx
  int v5; // ecx
  int result; // eax
  int v7; // ebx
  bool v8; // zf
  int v9; // edx
  int v10; // edx
  int v11; // ecx
  char v12; // cl
  int v13; // ecx
  int v14; // eax
  int v15; // [esp-4h] [ebp-6Ch]
  int v16; // [esp+Ch] [ebp-5Ch]
  int v17; // [esp+10h] [ebp-58h] BYREF
  int v18; // [esp+14h] [ebp-54h]
  _DWORD v19[20]; // [esp+18h] [ebp-50h] BYREF

  if ( a2 == nullptr )
    return 0;
  switch ( *a2 )
  {
    case 1:
    case 0x65:
      v4 = sub_434290();
      if ( v4 == 0 )
        goto LABEL_29;
      if ( *a2 == 1000 )
      {
        v5 = a2[7];
        if ( *(_DWORD *)(a1 + 4 * v5 + 660) != 0 )
          goto LABEL_29;
        *(_DWORD *)(a1 + 4 * v5 + 660) = 1;
      }
      v18 = -1;
      v17 = -1;
      if ( sub_5B7DA0(v4, &v17) == 0 )
      {
LABEL_29:
        result = 1;
      }
      else
      {
        v15 = v18;
        *(_DWORD *)(v4 + 8) = 254;
        *(_BYTE *)(v4 + 4) = *(_BYTE *)(a1 + 4);
        *(_BYTE *)(v4 + 7) = *(_BYTE *)(a1 + 7);
        *(_DWORD *)(v4 + 1632) = *(_DWORD *)(a1 + 1632);
        *(_DWORD *)(v4 + 84) = **(_DWORD **)(a1 + 1632);
        Object_SetPatternFrame(v15);
        result = 1;
      }
      break;
    case 5:
      v7 = *(_DWORD *)(a1 + 1004);
      if ( v7 == 0 )
        goto LABEL_29;
      switch ( a2[1] )
      {
        case 0:
          sub_59F840(v7, a2[2]);
          result = 1;
          break;
        case 1:
          sub_42F0E0(v19);
          v8 = a2[6] == 0;
          v9 = a2[2];
          v16 = a2[4];
          v17 = a2[5];
          v19[0] = a1;
          v19[1] = v7;
          v19[2] = 0;
          v19[3] = v9;
          v19[4] = 0;
          if ( !v8 )
          {
            sub_559680(a1);
            sub_52EA80(v19);
            v9 = v19[7];
          }
          if ( dword_C7A658 == 0 )
            *(_DWORD *)(v7 + 140) -= v9;
          sub_565C70(a1);
          sub_54CA80(v16, 0);
          sub_54CA80(v10, 0);
          if ( v17 == 0 )
            goto LABEL_29;
          sub_4DAC80(v19);
          sub_5C3680(8);
          sub_4D9D70("normal_se", v17, v11);
          result = 1;
          break;
        case 2:
          v12 = *(_BYTE *)(v7 + 1604);
          *(_BYTE *)(v7 + 1605) = v12 == 0;
          *(_BYTE *)(v7 + 1604) = v12 == 0;
          if ( (v12 == 0) == v12 )
            goto LABEL_29;
          sub_42AD90(v7 + 1044);
          sub_42AD90(v7 + 1060);
          *(_BYTE *)(v7 + 1607) = 0;
          result = 1;
          break;
        case 3:
          if ( dword_C7A658 != 0 )
            goto LABEL_29;
          v13 = a2[2];
          v14 = -v13;
          if ( v13 >= 0 )
            v14 = a2[2];
          sub_57E700(v14, (unsigned int)v13 >> 31, 1);
          result = 1;
          break;
        default:
          goto LABEL_29;
      }
      return result;
    case 6:
      sub_5B5610();
      return 1;
    case 8:
    case 0x6C:
      sub_5B8F00();
      return 1;
    case 9:
      sub_5B5240();
      return 1;
    case 0xB:
    case 0x6F:
      sub_5B7750();
      goto LABEL_29;
    case 0xE:
      sub_5B73B0();
      return 1;
    default:
      return 0;
  }
  return result;
}
