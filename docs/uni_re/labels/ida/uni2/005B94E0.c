int sub_5B94E0()
{
  int i; // ebx
  int v1; // edi
  int v2; // eax
  int v3; // eax
  int v4; // esi
  _DWORD *v5; // edx

  for ( i = 0; i < dword_C7B4E0; ++i )
  {
    v1 = dword_C7B4E4[i];
    if ( v1 != 0 && (*(int (__thiscall **)(int))(*(_DWORD *)v1 + 4))(dword_C7B4E4[i]) != 0 && *(_BYTE *)(v1 + 121) != 0 )
    {
      v2 = 0;
      if ( byte_218A071 != 0 || dword_C748A0 != 0 )
        v2 = 1;
      if ( dword_A57E94 != 0 )
        v2 = 10;
      if ( dword_C74894 == 0 && v2 == 0 && *(_BYTE *)(v1 + 120) == 0 )
      {
        v3 = *(_DWORD *)(v1 + 1620);
        if ( v3 != 0 )
        {
          v4 = 0;
          dword_B57FE8 = 0;
          if ( *(_BYTE *)(v3 + 277) != 0 )
          {
            do
            {
              if ( *(unsigned __int8 *)(v3 + 277) <= v4 )
                v5 = nullptr;
              else
                v5 = *(_DWORD **)(*(_DWORD *)(v3 + 284) + 4 * v4);
              if ( v5 != nullptr )
              {
                sub_5B8620(v1, v5);
                v3 = *(_DWORD *)(v1 + 1620);
              }
              ++v4;
            }
            while ( v4 < *(unsigned __int8 *)(v3 + 277) );
          }
          *(_BYTE *)(v1 + 121) = 0;
        }
      }
    }
  }
  return 1;
}
