# UNI2_Cha.CT: Cheat Engine table analysis (UNI2 Steam 1.42)

Source: `C:\Users\teo\Downloads\UNI2_Cha.CT` (CE table v47, 3 entries, 8017 bytes). Read as data only; nothing
from it was executed. IDB: `C:\dev\ida\server\uni2_142_4ebed985.exe.i64`. Image base confirmed **0x400000**;
`.text` 0x401000-0x92D000 (the CT hard-codes `TEXT_RVA=0x1000`, `TEXT_SIZE=0x52B764`, which matches). Input MD5
in the IDB: `67e8f8dfcc131317d8ab3e64f01fb023`.

## Headline

**The table is a character-select roster extender, not a gameplay or Squirrel patch.** All three entries touch
the character-select icon/portrait animation object `CSel_PAni` (source path in the binary:
`code\scene\charaselect\charsel_pani.cpp`). They lift three hard roster limits: the CSelAnim.ini icon count
(39 -> 100), the per-character portrait pointer array (39 inline slots -> heap array of 256), and two 32-entry
stack arrays used to rank icons by their intro `delay` (-> heap, 256 each). This is groundwork for adding new
characters to the select screen. **Nothing touches Squirrel**: no patched function is a native, none is reachable
from a `SqratWrap_`/`Sqrat_TableBase_BindFunc` registration, and none of the functions or their callers (4 levels)
appear in `docs/squirrel/api/uni2_api.json` / `uni2_raw.json` / `uni2_callseq.json`.

Format note: the entries are not plain `db`/`aobscan` AA scripts. Each is a `{$lua}` block that uses a shared Lua
helper (`UNI2CS` in `<LuaScript>`) to scan `.text` for AOBs, require uniqueness, save original bytes, then write
patches with `writeBytes`/`writeInteger`. Two entries allocate memory (`allocateMemory`) at runtime, so the patched
bytes contain **runtime heap addresses**; they are shown below as `<mem>`, `<ptrs>`, `<cave>`. `UNI2CS.charLimit =
0x100`.

All AOBs were re-run against the 1.42 IDB's `.text`: each `findUnique` pattern matches exactly once and every
chained step resolves, so the original bytes below are verified IDB bytes.

## Per-entry table

### 40000 "Character Select: Extended Stack Memory (new build)"

Anchor `8D * * * * * 89 * * * * * * * 3B * 7C * 8B` -> 0x657DE4 (uni2.exe+0x257DE4). The script reads two
disp32 values (`-0x250` = `iconSortKey`, `-0xB0` = `iconRank`, both `DWORD[32]` on the stack of
`CSel_PAni_Init`), then walks a chain of 8 further AOBs that contain those displacements. At each site it writes
the absolute heap address over disp32 and subtracts 0x80 from the ModRM byte (mod=10 `[ebp+disp32]` -> mod=00 with
base=101, i.e. `[disp32]` / `[index*4+disp32]`). Array "one" -> `<mem>`, array "two" -> `<mem+0x400>`.

| VA (uni2.exe+off) | function | original bytes / asm | patched bytes / asm |
|---|---|---|---|
| 0x657DEA (+257DEA) | CSel_PAni_Init | `89 84 8D B0 FD FF FF` mov [ebp+ecx*4-250h],eax | `89 04 8D <mem>` mov [ecx*4+mem],eax |
| 0x657E14 (+257E14) | CSel_PAni_Init | `8B B4 95 B0 FD FF FF` mov esi,[ebp+edx*4-250h] | `8B 34 95 <mem>` mov esi,[edx*4+mem] |
| 0x657E20 (+257E20) | CSel_PAni_Init | `39 B4 85 B0 FD FF FF` cmp [ebp+eax*4-250h],esi | `39 34 85 <mem>` cmp [eax*4+mem],esi |
| 0x657E09 (+257E09) | CSel_PAni_Init | `89 8C 95 50 FF FF FF` mov [ebp+edx*4-0B0h],ecx | `89 0C 95 <mem+400>` mov [edx*4+mem+400h],ecx |
| 0x657E2A (+257E2A) | CSel_PAni_Init | `89 8C 95 50 FF FF FF` (same) | `89 0C 95 <mem+400>` |
| 0x657FA2 (+257FA2) | CSel_PAni_Init | `8B 84 8D 50 FF FF FF` mov eax,[ebp+ecx*4-0B0h] | `8B 04 8D <mem+400>` mov eax,[ecx*4+mem+400h] |
| 0x658110 / 0x658124 / 0x658138 / 0x65816F | CSel_PAni_Init | `8D 85 50 FF FF FF` lea eax,[ebp-0B0h] | `8D 05 <mem+400>` lea eax,[mem+400h] |

Effect: the loop at 0x657DE0 copies each icon record's `+0x4C` field (the INI `delay` key; records are 0xB4 bytes,
built by `CSel_PAni_LoadCSelAnimIni`) into `iconSortKey[i]`, then computes `iconRank[i] = 1 + count(j: key[j] <
key[i])`. The rank is later used at 0x657FA2 (`... = iconRank[i] + 520`, a draw priority). With more than 32 icons
both stack arrays overflow into the frame; relocating them to a 0x1000-byte heap block allows 256 each. The four
`lea` sites reuse `iconRank` as a scratch `sprintf` buffer (`"%s"`), which is why they are relocated too.
Touches Squirrel: **no** (scene/UI code; no native path).

Caveat found in the IDB: the init at 0x657D60-0x657D99 (`iconRank[0] = -1; memset(&iconRank[1],0,0x7C)` and the
same for `iconSortKey`) is **not** relocated; it still clears the now-unused stack copies. Harmless only because
`allocateMemory` returns zeroed pages and the rank loop writes every entry 0..n-1 before reading.

### 40001 "Character Select: Extended Portrait Memory (new build)"

Allocates `<ptrs>` (0x1000, 1024 pointer slots) and `<cave>` (0x100), writes three caves, then redirects with
`E9 rel32` + NOP padding. `CSel_PAni` stores one portrait object pointer (`CPaniData*` from
`./grpdat/CSel/chara/cs_chr%03d.pat`) per character at `this+8 .. this+0xA4` (39 = 0x27 slots).

| VA | function | original bytes / asm | patched (site) | cave code |
|---|---|---|---|---|
| P1 0x6559EA (+2559EA) | CSel_PAni_Init | `8D 56 08 33 C0` lea edx,[esi+8]; xor eax,eax | `E9 <cave+0>` | `8D 15 <ptrs>` lea edx,[ptrs]; `33 C0`; `E9`->0x6559EF |
| P2 0x655B84 (+255B84) | CSel_PAni_Init | `8D 46 08 89 BD 10 FD FF FF` lea eax,[esi+8]; mov [ebp-2F0h],edi | `E9 <cave+20> 90 90 90 90` | `8D 05 <ptrs>` lea eax,[ptrs]; `89 BD 10 FD FF FF`; `E9`->0x655B8D |
| P3 0x654A33 (+254A33) | CSel_PAni_dtor | `8D 73 08 BF 27 00 00 00` lea esi,[ebx+8]; mov edi,27h | `E9 <cave+40> 90 90 90` | `8D 35 <ptrs>` lea esi,[ptrs]; `BF 00 01 00 00` mov edi,100h; `E9`->0x654A3B |

Effect: P1 is the constructor-time clear of the portrait slots (`rep stosd`, ecx stays 0x27, so only 39 of the
heap slots are zeroed; the rest are zero from allocation). P2 is the base pointer (`[ebp-2ECh]`) used by the
per-character load loop that fills the slots. P3 is the destructor loop that releases each slot, now over 256.
Touches Squirrel: **no**.

Caveats: `<ptrs>` is one global array, so this assumes a single live `CSel_PAni` instance. Any other code that
indexes `this+8+i*4` directly (draw/update paths) is not redirected by the table; it was not exhaustively audited
here. The inline `this+8` slots become dead storage.

### 40002 "Character Select: Icon Limit 0x64 (new build)"

| VA | function | original | patched |
|---|---|---|---|
| 0x653CC1 (+253CC1), imm8 of the instruction at 0x653CBF | CSel_PAni_LoadCSelAnimIni | `83 F8 27` cmp eax,27h (followed by `0F 82` jb 0x653920) | `83 F8 64` cmp eax,64h |

The script asserts the byte is 0x27 before writing (a real expect-bytes check). Effect: the loader reads
`./grpdat/CSel/CSelAnim.ini` sections `"000".."038"` (each with `icon`, `hiddun_icon`, `position_x/y`, `delay`,
`on_1p`, `on_2p`, ...) into 0xB4-byte records; the patch raises that to `"000".."099"`. Needs 40000 (rank arrays
of 32) and 40001 (portrait slots of 39) to avoid overflow once more than 32/39 icons exist. Touches Squirrel: **no**.

## Squirrel check (method)

Callers of the three patched functions, walked 4 levels: `CSel_PAni_Init` <- sub_64A650, sub_64A7E0,
sub_819D40 <- sub_825800 <- sub_8257E0 <- sub_824AC0 (plus vtable/data refs); `CSel_PAni_dtor` <-
`CSel_PAni_scalar_deleting_dtor` <- `CSel_PAni` vftable (0x9594FC). None of these addresses or names appear in the
UNI2 Squirrel API dumps, and no native address in `uni2_api.json` lies in 0x650000-0x660000. The patches change no
native return value, no move table, and nothing in the lexer/VM. They do change **data limits** that a script- or
data-side roster mod (more `cs_chr%03d.pat`, more CSelAnim.ini sections) would depend on.

## IDB renames made (saved)

| address | old | new |
|---|---|---|
| 0x6558F0 | sub_6558F0 | `CSel_PAni_Init` |
| 0x6538B0 | sub_6538B0 | `CSel_PAni_LoadCSelAnimIni` |
| 0x654940 | sub_654940 | `CSel_PAni_dtor` |
| 0x43CAD0 | sub_43CAD0 | `CSel_PAni_scalar_deleting_dtor` |
| CSel_PAni_Init lvars | v272 / Buffer | `iconSortKey` / `iconRank` |

Comments `UNI2_Cha.CT: <entry> patches X -> Y` were added at every patch site listed above, plus the anchor
0x657DE4, the unrelocated init 0x657D70, and function comments on `CSel_PAni_Init` / `CSel_PAni_LoadCSelAnimIni`.

## As a declared fbmod patch module

Game `uni2`, exe version `1.42 Steam`, identity = IDB input MD5 `67e8f8dfcc131317d8ab3e64f01fb023` (a SHA-256 of
the on-disk exe should be added when the module is built). Records use VA at image base 0x400000; the loader
rebases if needed. `<alloc:NAME+off>` = a module-owned allocation that the loader resolves before writing.

```yaml
module: uni2_charsel_roster_ext        # from UNI2_Cha.CT
game: uni2
exe: { version: "1.42", md5: 67e8f8dfcc131317d8ab3e64f01fb023, sha256: TODO }
allocs:
  - { name: rankmem,  size: 0x1000, zero: true }   # 2 x 256 dwords
  - { name: ptrs,     size: 0x1000, zero: true }   # portrait pointer slots
caves:
  - { name: pcave, size: 0x100 }                    # or a fixed code-cave section in the shim
patches:
  - id: charsel.icon_limit
    va: 0x653CC1
    expect: "27"
    replace: "64"
    description: CSelAnim.ini icon sections 000..038 -> 000..099
    affects: [behaviour:charsel.icon_count, data:grpdat/CSel/CSelAnim.ini]
    requires: [charsel.rank_heap, charsel.portrait_heap]
  - id: charsel.rank_heap            # 10 sub-records, one per site
    sites:
      - { va: 0x657DEA, expect: "89 84 8D B0 FD FF FF", replace: "89 04 8D <alloc:rankmem+0>" }
      - { va: 0x657E14, expect: "8B B4 95 B0 FD FF FF", replace: "8B 34 95 <alloc:rankmem+0>" }
      - { va: 0x657E20, expect: "39 B4 85 B0 FD FF FF", replace: "39 34 85 <alloc:rankmem+0>" }
      - { va: 0x657E09, expect: "89 8C 95 50 FF FF FF", replace: "89 0C 95 <alloc:rankmem+0x400>" }
      - { va: 0x657E2A, expect: "89 8C 95 50 FF FF FF", replace: "89 0C 95 <alloc:rankmem+0x400>" }
      - { va: 0x657FA2, expect: "8B 84 8D 50 FF FF FF", replace: "8B 04 8D <alloc:rankmem+0x400>" }
      - { va: 0x658110, expect: "8D 85 50 FF FF FF",    replace: "8D 05 <alloc:rankmem+0x400>" }
      - { va: 0x658124, expect: "8D 85 50 FF FF FF",    replace: "8D 05 <alloc:rankmem+0x400>" }
      - { va: 0x658138, expect: "8D 85 50 FF FF FF",    replace: "8D 05 <alloc:rankmem+0x400>" }
      - { va: 0x65816F, expect: "8D 85 50 FF FF FF",    replace: "8D 05 <alloc:rankmem+0x400>" }
    description: icon delay-rank arrays (2 x DWORD[32] on stack) -> heap, 256 each
    affects: [behaviour:charsel.icon_draw_order]
  - id: charsel.portrait_heap
    sites:
      - { va: 0x6559EA, expect: "8D 56 08 33 C0",             replace: "jmp <cave:pcave+0x00>",
          cave: "8D 15 <alloc:ptrs> 33 C0 jmp 0x6559EF" }
      - { va: 0x655B84, expect: "8D 46 08 89 BD 10 FD FF FF", replace: "jmp <cave:pcave+0x20> 90 90 90 90",
          cave: "8D 05 <alloc:ptrs> 89 BD 10 FD FF FF jmp 0x655B8D" }
      - { va: 0x654A33, expect: "8D 73 08 BF 27 00 00 00",    replace: "jmp <cave:pcave+0x40> 90 90 90",
          cave: "8D 35 <alloc:ptrs> BF 00 01 00 00 jmp 0x654A3B" }
    description: CSel_PAni portrait slots this+8[39] -> global heap[256] (single instance assumed)
    affects: [behaviour:charsel.portrait_slots, data:grpdat/CSel/chara/cs_chr%03d.pat]
conflicts:
  - any module patching CSel_PAni_Init 0x655B84..0x65817A or CSel_PAni_dtor 0x654A33
  - any module that reads CSel_PAni+8 portrait slots directly (they become dead)
squirrel: { natives: [], vm: none, script_visible: false }
sim_affecting: false        # select-screen only; safe to differ between netplay peers
```

Notes for the converter: the CT's AOB chain (anchor + "find next after") becomes a one-time resolution step at
module build time; the module ships fixed VAs plus `expect` bytes, and refuses on any mismatch or wrong exe hash.
The `-0x80` ModRM rewrite and the absolute address operands are why `rank_heap` cannot be pure static bytes: the
replace bytes are a template filled with the allocation address at apply time (or with the address of a fixed BSS
cave section in the shim).
