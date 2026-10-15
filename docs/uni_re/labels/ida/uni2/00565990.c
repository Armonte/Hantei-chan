void sub_565990()
{
  char *v0; // ebx
  int v1; // eax
  int v2; // eax
  int v3; // edi
  _DWORD *v4; // edx

  v0 = (char *)&unk_10652F9;
  do
  {
    if ( v0[1887] != 0 && *v0 != 0 )
    {
      v1 = 0;
      if ( byte_218A071 != 0 || dword_C748A0 != 0 )
        v1 = 1;
      if ( dword_A57E94 != 0 )
        v1 = 10;
      if ( dword_C74894 == 0 && v1 == 0 && *(v0 - 1) == 0 )
      {
        v2 = *(_DWORD *)(v0 + 1499);
        if ( v2 != 0 )
        {
          v3 = 0;
          dword_B57FE8 = 0;
          if ( *(_BYTE *)(v2 + 277) != 0 )
          {
            do
            {
              if ( *(unsigned __int8 *)(v2 + 277) <= v3 )
                v4 = nullptr;
              else
                v4 = *(_DWORD **)(*(_DWORD *)(v2 + 284) + 4 * v3);
              if ( v4 != nullptr )
              {
                sub_5B8620((int)(v0 - 121), v4);
                v2 = *(_DWORD *)(v0 + 1499);
              }
              ++v3;
            }
            while ( v3 < *(unsigned __int8 *)(v2 + 277) );
          }
          *v0 = 0;
        }
      }
    }
    v0 += 3008;
  }
  while ( (int)v0 < (int)byte_106DFF9 );
}
