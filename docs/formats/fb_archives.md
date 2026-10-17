# French-Bread archive containers (generalized layer `src/fbarc`)

One reader/writer per container family, one interface (`fbarc::Archive`: entries, plain read, streaming rebuild with replace / remove / add).
Front end: `build/fbarctool.exe ls|count|rt|extract|pack|magics|census`. The GUI browser (Tools > French Bread archives) and File > Open
route by header magic through `fbarc::Detect`. Every reader is proven by `fbarctool rt`: rebuild with no edits must equal the file byte for byte
(header words, name slots with their leftover bytes, entry order and layout are all preserved).

Credits: format RE by the castergroup tools (`montopak.py`, `p_extractor.py`, `universal_extractor.py`, MBAA extraction docs), Hantei-chan
`mbaacc_pack.*` / `han2/pac_archive.*` / `han2/gof1_archive.*`. IDA proof for PKFileInfo: `mbacPC.exe.i64` (`PKArchive_*`, struct `PKArchive`).

| Kind | magic / header | entry | key | games |
|---|---|---|---|---|
| PkFileInfo | `"PKFileInfo\0"`, 5 ignored bytes, u32 type @0x10, u32 count^key @0x14 | 72 B @0x18 | 0xE3DF59AC | MBAC (`C:\games\MB\AC\00..05,10.p`) |
| MbFilePacA | `"FilePacHeaderA"` + 52 B header, folder table, file table | 268 B folders, 44 B files | per-archive u32 @20 | Melty Blood AACC PC 1.07 / Steam `0000.p..` |
| RboPac (v1) | u32 1, u32 count^key | 68 B @8 | 0xE3DF59AC | RBO `*.PAC`, GOF2 `data0x.dat`, MB `data00.p` |
| MbOldP (v0) | u32 0, u32 count^key | 68 B @8 | 0xE3DF59AC | Melty Blood / ReAct PC `.p` |
| Gof1Pb | u32 flag(0/1), u32 count^key | 64 B @8 | 0xFA261EFB | PB2K1 `.dat`, GOF1 `gof_0N.p`, Rosa/dMp `PAC.PAC` |

## PKFileInfo (MBAC)  -- reversed from mbacPC.exe

`PKArchive_Open` 0x431670, `PKArchive_ReadEntry` 0x431A30, `PKArchive_FindEntryIndex` 0x431810, struct `PKArchive` (284 B, 15 archive slots at
`g_PKArchives` 0x7912C8, 5 more at `g_PKArchivesAlt` 0x994C00).

* header: `File_Read(16)` then `strcmp(buf,"PKFileInfo")` (bytes 11..15 are never read by the game; retail stores `00 ff` + archive-specific
  bytes, e.g. 0x0E = 04 / 20 in 03.p / 05.p, 0x0C = 02 in 10.p; kept verbatim), `type` u32 @0x10, `count ^ 0xE3DF59AC` u32 @0x14.
* entry (72 B), index i: bytes 0..62 `^= 3*(j*i+26)` (name, CP932, UPPER case; leftover bytes after the NUL are tool memory and stay in the slot),
  byte 63 plain, u32 offset @64 plain, u32 size @68 `^ 0xE3DF59AC`. Integrity test: `file size >= sum(sizes)`.
* data: contiguous after the index in index order (verified on all 7 retail archives, no gaps), names sorted ascending.
* lookup: `Path_CopyFileName` + `CharUpperA` + `strcmp`, linear. Flat: no folders.
* payload cipher (first 1977 = 0x7B9 bytes only): type 0 `^= i + upper(name)[i%len] + 3`; type 1 stored; type 2 `^= 0xCA`. Retail: 10.p (audio) type 1, rest type 0.
* validated: after the cipher every entry has the right magic (EX3 `LLIF`, OGG `OggS`, Hantei4 `.DAT`, `[DAT]` INI...).

## FilePacHeaderA (Melty Blood AACC)

52-byte header: magic[14], 2 pad bytes (kept), version u32 @16 (1), key u32 @20, data offset @24 (= 52+268*nd+44*nf), data size @28, nd @32, nf @36,
flag @40 (1), increment @44 (3), chunk @48 (0x1000). Folder record 268 B: first-file position (relative to the data offset), first-file index,
**total size of its files** (low byte = name increment; 0 with an empty name = unused sentinel), name[256] stream-XORed with the key (key byte k
`+= increment` after each use). File record 44 B: position, owner folder, size, name[32] (increment = size & 0xFF). Files are grouped by owner
(monotonic) and contiguous. Payload: first 4 KiB XOR stream (increment 3); the Steam build also the last 4 KiB of files > 8 KiB. Mode is detected by
a tail-entropy vote over the 24 largest files. Edits recompute folder records, offsets, sizes, name increments and header totals.
Constraint (unverified in the exe): a folder whose total size is a multiple of 256 gets name increment 0, the value the game's folder table uses for "unused".

## PAC v0 / v1, Gof1Pb

See `han2/pac_archive.h`, `han2/gof1_archive.h`, `docs/formats/gof1.md`. v0 (Melty Blood PC) enciphers the first 0x2173 bytes with the name key
(`^= name[i%len] + i + 3`, from montopak); v1 stores plain.
