# Squirrel 2.2.4 stable: vendored copy

- Upstream: `squirrel_2.2.4_stable.tar.gz` from SourceForge (project `squirrel`, folder
  `squirrel2/squirrel 2.2.4 stable`). sha256 in `UPSTREAM_SHA256.txt`. Licence: zlib/libpng (`COPYRIGHT`).
- Only `include/`, `squirrel/`, `sqstdlib/` and the licence/readme files are kept.
- This is the exact version string embedded in UNIB 1.04.03, DFCI Ignition 2.32.00, UNI2 1.42 and MBTL 25646393
  (`_version_ = "Squirrel 2.2.4 stable"`, see `docs/squirrel/SCOPE.md`).

## Local changes (zlib licence clause 2: altered source, marked `HANTEI-CHAN PATCH`)
1. `include/squirrel.h`: `SQ_FB_COMPAT` stops `_SQ64` on 64-bit hosts so `SQInteger` stays 32-bit like the games
   (`_intsize_ 4`), and makes `SQRawObjectVal` 64-bit so raw comparisons cover whole pointers.
2. `squirrel/sqtable.h`: `hashptr` casts through `size_t` (was `SQInteger`, truncating on x64).
3. `squirrel/squtils.h`: forward-declares `sq_vm_malloc/realloc/free` before the templates (two-phase lookup in
   modern g++).
No language or VM behaviour is changed.

## French-Bread VM modification reproduced (behaviour change, only with `SQ_FB_COMPAT`)
4. `squirrel/sqlexer.cpp` `ReadString`: Shift-JIS lead bytes (0x81-0x9F, 0xE0-0xFF) inside string literals copy
   the next byte verbatim (unless it is the closing delimiter). Without this, CP932 characters whose trail byte
   is 0x5C (`表`, `能`, `ソ`, ...) are read as escapes and 7 MBTL / 18 UNI2 shipped scripts fail with
   "unrecognised escaper char". Found in both games' lexer (UNI2 1.42 0x47B9F0, MBTL 0x473430, named
   `SQLexer_ReadString_FbSjis`). The rest of the lexer matches upstream.
