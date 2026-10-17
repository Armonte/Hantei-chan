int __fastcall sub_5C8580(int a1, int a2)
{
  int v4; // eax
  unsigned __int8 v5; // al
  int v6; // eax
  bool v7; // zf
  int v8; // edi
  _DWORD *v9; // eax
  int v10; // ecx
  int v11; // edi
  int *v12; // edx
  int v13; // ebx
  unsigned __int8 v14; // al
  int v16; // [esp+14h] [ebp-1Ch]
  _DWORD v17[2]; // [esp+18h] [ebp-18h] BYREF
  int v18; // [esp+20h] [ebp-10h]
  int v19; // [esp+2Ch] [ebp-4h]

  v18 = a2;
  v4 = 0;
  if ( byte_218A071 != 0 || dword_C748A0 != 0 )
    v4 = 1;
  if ( dword_A57E94 != 0 )
    v4 = 10;
  if ( dword_C74894 == 0 && v4 == 0 && dword_A57E78 == 0 && sub_5CF000(a1) != 0 )
  {
    if ( a2 == 0 && *(_DWORD *)(a1 + 460) != 0 )
    {
      v5 = *(_BYTE *)(a1 + 497);
      if ( v5 == 0 || *(_BYTE *)(a1 + 498) > v5 )
      {
        if ( (*(int (__thiscall **)(int))(*(_DWORD *)a1 + 4))(a1) != 0 )
        {
          v6 = *(_DWORD *)(a1 + 1628);
          if ( v6 == 0 )
          {
            v6 = *(_DWORD *)(a1 + 1012);
            if ( v6 != 0 )
              v6 = *(_DWORD *)(v6 + 1628);
          }
          v7 = *(_DWORD *)(a1 + 1764) == 0;
          v8 = *(_DWORD *)(v6 + 2160);
          v17[1] = 0;
          v17[0] = a1;
          if ( v7 )
          {
            sub_429950(v8, v17);
            v9 = (_DWORD *)(a1 + 1696);
            v19 = 0;
            if ( *(_DWORD *)(a1 + 1716) >= 0x10u )
              v9 = (_DWORD *)*v9;
            sub_429B00(v9, "HitInterrupt");
            sub_5327F0(a1);
            v19 = -1;
            *(_DWORD *)(v16 + 8) -= 8;
          }
        }
        *(_DWORD *)(a1 + 460) &= ~0x2000u;
      }
    }
    v10 = *(_DWORD *)(a1 + 1620);
    if ( v10 != 0 )
    {
      v11 = 0;
      if ( *(_BYTE *)(v10 + 276) != 0 )
      {
        do
        {
          if ( *(unsigned __int8 *)(v10 + 276) <= v11 )
            v12 = nullptr;
          else
            v12 = *(int **)(*(_DWORD *)(v10 + 280) + 4 * v11);
          v13 = v10;
          if ( v12 != nullptr )
          {
            v14 = byte_9571A0[*v12];
            if ( v14 == 0xFF || v18 == v14 )
            {
              sub_5C6220(a1, v12);
              v13 = *(_DWORD *)(a1 + 1620);
            }
          }
          ++v11;
          v10 = v13;
        }
        while ( v11 < *(unsigned __int8 *)(v13 + 276) );
      }
    }
  }
  return 0;
}
