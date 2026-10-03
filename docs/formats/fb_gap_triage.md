# Gap triage: the "none" rows of the format matrix (2026-10-03)

Analysis only (no builds, no game runs). Facts come from `docs/formats/evidence/fb_census.tsv` (every archive entry on this machine), header probes of
real files, PovertyCaster `docs/lineage/*` + `docs/games/dmp.md`, `C:\dev\mbaa\extraction\docs`, `montopak.py`, and the IDB inventory in `C:\dev\ida\server`.
Matrix: `docs/formats/fb_format_matrix.md` (none 47 / read 17 / edit 8 / byte-exact 36 / in-game 0).

## 0. What changed the estimate (measured today)

* **MB (2002) character = the GOF1 container, same cipher keys.** `MB/MeltyBlood/data03.p` AKAAKIHA.DAT, header decrypted with the GOF1 header key:
  signature `備前長船`, version **0x12 (18, as GOF1)**, patternAreaEnd 0x6138A, partsSize 0, **cgSize 0xB08398 (11.5 MB embedded CG blob; GOF1 ships 0)**.
  So `framedata_gof1` + `gof1::DecryptDat` are the base; the delta is the CG blob (and whatever GOF1 omitted).
* **ReAct character = HA4 plain.** `MB/R/01.p`: 23 `.DAT`, magic `Hantei4\0` (= MBAC's), `02.p` = 44 stage files `bgma` (= MBAACC stage .DAT family, already handled).
  No stage-2 cipher at all. The existing HA4 loader probably opens them unchanged (version/size differences untested: run `ha4tool` over the 23 files first, hours not days).
* **PB2K1 character = a third container**: `時は来た` (c3f8edbf encrypted), 14 chars + `01p`/`02p` in `pb/01.dat`. Header decrypts to version 0x10000, check word 0x0F,
  then `0x14: 0x245A6`, `0x18: 0x14EA50`, `0x1C: 0x41C` ... (header 0x41C, offsets[250]). GOF1-like skeleton but different counts; needs a fresh parser.
* **dMp / Rosa have NO character DAT.** dMp `GameData.PAC` (132 entries): 58 `.FOB` (CHARA01..12 = 77-83 KB each, ENEMY000.. = 31 KB), 68 `.IMG`, 6 `DEMO0N.DAT`
  (2,592,104 B each, same size: demo/replay input streams). Rosa `PAC.PAC`: 6 FOB, 100 IMG, 9 WAV. The "character" is a script (`.FOB` VM) + IMG sheets, like RBO's `.FOB` + `.IMG`.
* **EX3 is solved at the block level for every title** (new this session, `fbarctool ex3rt`): 4,684 EX3 files from MBAC, MB, ReAct, PB2K1, GOF1, Lilian all
  parse, re-serialize byte-identically and decode to a BMP. See section 4.

## 1. Per-title facts

Common: archives and EX3 are done (M1/M2 + above). "Frame" = animation frame record.

| | MB (2002) | Re-ACT (2004) | PB2K1 (2001) | dMp (2003) | Rosa (2002/2005) | Lilian (2005) | QoH99 (1999) | QoH98 (1998, extra) |
|---|---|---|---|---|---|---|---|---|
| Genre | fighter | fighter | tag fighter | 6P swarm action | 1P shooter | 1P shooter | fighter | fighter |
| Install | `C:\games\MB\MeltyBlood`, `MB\R` | `C:\games\MB\R` | `C:\games\pb` | `frenchbread\dmp_1020`, `drill_milky_punch`, aquat1c copies | `benibara_rendan`, `yamayuri_rendan\omake` | `yamayuri_rendan\files\data` | `C:\games\qoh` | `C:\games\qoh98` |
| Containers | `.p` PAC v0/v1 (E3DF59AC) | same | `.dat` PB (FA261EFB) | `.PAC` PB v1 | `PAC.PAC` PB v1 (2002) / v0 (2005 repack, filename-keyed data XOR) | `.p` PAC v0/v1 | none (loose files) | none |
| Char files | 18 `.DAT` (data03.p) + `.CPF` 20 + `.WMT` 18 + `.CT` 20 | 23 `.DAT` Hantei4 (+27 CPF, 24 CT, 24 WMT) | 14 `.DAT` + 01p/02p | 12 `CHARAxx.FOB`, 6 `ENEMY`.. FOB, 6 `DEMO.DAT` | 6 `.FOB` | scripts are `.TXT` (276, `_00_0_<name>.TXT` ~46 KB each) | 37 `.chr` (33.4 MB, SZUKI MASAMI cipher) + 30 `.Fob` | 14 `.dat` (579 KB-1 MB), plain |
| Cipher | stage 1: v0 name-keyed 0x2173 B; stage 2: GOF1 3 keys (header `Memoryえらー..`, pattern `Meンどう..`, blob `hiまで..`) | stage 1 only (`Hantei4` already plain in the .p) | stage 1: FA26 v0 name-keyed 9696 B; stage 2 same 3 keys | stage 1 FA26 v1 (plain); IMG body obfuscated | v1 plain / v0 filename XOR 9696; IMG obfuscated (2002) | stage 1 only | per-file keyed on the character name (`AYAKAAYAKAAYAKAA..` repeating) = "SZUKI MASAMI" derivation; script `chr_decrypt_with_szuki.py` | none |
| Frame | 116 B (AF 40 + AS 40 + 36 index) | 216 B (HA4: AF 44 + AS 56 + 58 x i16) | not measured; header differs from GOF1 | n/a (script) | n/a | n/a | 96 B (PovertyCaster lineage doc) | 8-B anim entries + 260-B state (`DAT_FORMAT_COMPLETE.md`) |
| Known docs | gof1.md (same container), mbr_to_mb.md | mbac_to_mbr.md, HA4_SECTION.md | `PB_CHAR_DAT_FORMAT_SOLVED.md`, qoh99_to_pb2k1.md | docs/games/dmp.md, fb_titles_inventory | fb_titles_inventory | fb_titles_inventory (not a fighter) | `qoh/_archive` (hundreds of `chr_*.py` decryptor attempts), Qoh99 adapter `Qoh99Hitboxes.hpp` | `qoh98/docs/DAT_FORMAT_COMPLETE.md` (IDA `LoadAndParseCharacterDATFile` 0x44C290) |
| PovertyCaster code that parses it | `pc-adapters/mb`, `melty_legacy/LegacyTitle_Mb` (runtime memory, not files) | `pc-adapters/mbr` | `pc-adapters/pb2k1` | `pc-adapters/dmp` (typed entity + script-VM thread/bank state, so the FOB VM layout is known at runtime) | none | none | `pc-adapters/qoh99` (hitboxes, effect slots, build file) | none |
| IDB | `mb.exe.i64` | `mbr.exe.i64` | `pb2k1.exe.i64`, `pb2001.exe.i64`, `PBEx.exe.i64` (trainer) | `dmp_1020\DMP.EXE.i64` | **none** (Rosa has no IDB; fb_titles_inventory: static only) | **none** | `qoh99.exe.i64`, `qoh99_dec.exe.i64` (December build, richly named), `qoh99se_c`, `QohHack` | `Qoh98.exe.i64` |
| Naming state | renamed (FAMILY.md: mb, mbr, gof, pb2k1, rbo "renames saved") but NOT typed for the character loader; `LoadCharacterDat`-type funcs unverified | same | same; qoh99_to_pb2k1 TSV has 86 matched rows | partially (adapter comments, 0x430670 Rng, Script_UnloadBanksFrom ...) | n/a | n/a | richest of the group (ProcessBattleLoop etc.) | the DAT loader is named + documented |

## 2. Closeness to existing Hantei-chan models

| Title | Model to reuse | What is new | Verdict |
|---|---|---|---|
| ReAct | HA4 (`framedata_ha4`, `ha4tool`, 50/50 byte-exact on MBAC) | check header version / fields that differ between ReAct and Act Cadenza (216-B frames are shared) | **closest**: expect read+edit nearly free, rt after a diff of two header words |
| MB | GOF1 (`framedata_gof1`, `gof1::Load/Serialize/Encrypt`, gof1rt 455 pass) | embedded CG blob (EX3-derived BMP set, cgSize 11.5 MB), 18 chars not 8, maybe extra AT/IF/EF records | **close**: parser reuse; CG blob needs a viewer/serializer |
| PB2K1 | GOF1 skeleton (offset table + pattern records) | its own header (0x41C, 250-entry table), possibly 96/116-B frames, sections sizes at +0x14/+0x18 | **medium**: new parser, GOF1 model |
| dMp | HAN2 `.FOB` (`fob_file.cpp`, `fobdis`), `.IMG` (`img_file.cpp`, "obfuscated body" per PovertyCaster) | check the FOB dialect (same VM strings per fb_titles_inventory; opcode set may be older), IMG header `00000000 04000000 ..` | **medium**: the data is a script, not frames; "editor" = script + sheet viewer |
| Rosa | same as dMp (same lib, one step earlier) | identical FOB, IMG 2002 (obfuscated) vs 2005 (plain, +16 B header) | **medium-small** after dMp |
| Lilian | none (not a French-Bread fighter engine): `.TXT` story scripts, `.BGC` (FCHIP) / `.MAP` (FMAP) background chips, `.EX3` | BG module shared with dMp per fb_titles_inventory; Hantei-chan has `background/bg_pat` (HA6 stages) which is not the same | **new**: low value for a fighter authoring tool |
| QoH99 | none (EX3 era but CHR container + 96-B frames) | a cipher no one has finished (hundreds of attempts in `_archive`), custom frame/box/hit records | **new, hardest** |
| QoH98 | none | simple, unencrypted, fully documented | **small but orphaned** |

## 3. EX3 (LLIF) variants found (item 4)

All coded as byte-pair blocks: pair table of groups `[c][entries]` to index 256, u16 big-endian symbol count, symbols; blocks repeat to end of file.
The GOF1 reader assumed a 64-byte header; the Melty line has three header sizes with the decoded-byte count in the LAST u32 of the header. Now one reader:
`han2::ParseEx3Auto` (tries 64/68/72, accepts only if blocks tile the file AND the decode length equals the size word), `DecodeEx3`, `SerializeEx3`.

| Header | size word / data | Files in the census | Titles |
|---|---|---|---|
| 64 B | @0x3C / 0x40 | 931 | PB2K1 (699), GOF1 (232) |
| 68 B | @0x40 / 0x44 | 1,579 | Melty Blood 2002 (346), ReAct (843), Lilian (390) |
| 72 B | @0x44 / 0x48 | 2,622 | MBAC (2,622); the MBAACC PC archives use DDS/PNG/BMP instead |

Result: 4,684 + the MBAC remainder parse, re-serialize byte-identically, decode to a file starting `BM` whose length equals the header word. `fbarctool ex3rt`.
Not done: the editor-side image model (BMP -> GL texture / PNG, palette handling for 8-bit PB `_M` files), and a **re-encoder** (compress a modified BMP). The
byte-exact path never needs one (it keeps the block structure); an editor that lets people replace a sprite does. The extraction tree has ~40 `ex3_*` compressor
experiments (`C:\dev\mbaa\extraction\tools`); the status doc says an exact-match recompressor was never achieved, which only matters for "replace image".

## 4. Effort (working days, one person; read = viewable, edit = loads in the editor and saves, rt = byte-exact on every shipped file, proof = in-game)

| Title | read | edit | rt | in-game proof | notes |
|---|---|---|---|---|---|
| EX3 viewer + replace image | 1 | 2 | (done) | - | prerequisite for every Melty-line character view; replace needs a compressor (+3 if exact-match is wanted) |
| ReAct (HA4 variant) | 0.5 | 1 | 1 | 1 | needs ReAct runs (`mbr.exe`), `pc-adapters/mbr` helps launching |
| MB (GOF1 container + CG) | 1 | 2 | 2 | 1 | CG blob is the unknown; `mb.exe.i64` loader names to verify |
| PB2K1 | 2 | 3 | 2 | 1 | IDB `pb2k1.exe` + `PB_CHAR_DAT_FORMAT_SOLVED.md` give the header; frame layout unproven; typed IDB structs 2 |
| dMp (FOB/IMG) | 1 | 2 | 1 | 1.5 | FOB reader exists; IMG obfuscation and DEMO.DAT are new; IDB exists; PovertyCaster DMP adapter allows a live loop |
| Rosa | 0.5 | 1 | 0.5 | 1 | after dMp; no IDB (static only) so typing is by analogy + `rosa_fh.exe` disasm |
| Lilian | 1 | 2 | 1 | 1 | `.TXT` is plain text; BGC/MAP need RE; no IDB (needs one: 1 day) |
| QoH99 | 3-8 | 4-8 | 3 | 1 | cipher status unknown (see risks) |
| QoH98 | 1 | 2 | 1 | 1 | trivial format; no one plays it |
| Typed IDB structs, all of the above | - | - | - | - | +0.5-1 per title (rule: no `unk`, `unused_<off>` with proof) |

Total if everything: roughly 35-45 days. A sensible first tranche (EX3 view, ReAct, MB, PB2K1) is about 14 days to rt, 18 with in-game proofs.

## 5. Risks and unknowns

1. **ReAct/MB header deltas are assumed small**; a field present only in the older engines (PB/MB `drawMode` 8 origin shift, AS flags) may be unnamed in the IDB.
2. **PB2K1 frame size is unmeasured.** qoh99_to_pb2k1.md says QoH99 has 96-B frames and PB2K1's entities are 5864 B; the character DAT frame may be 96/104/116 B.
3. **MB CG blob** might be an LLIF/EX3 bundle or raw BMPs; a viewer/serializer is needed for round trip of the DAT (it is byte-exact if kept opaque).
4. **QoH99 cipher**: `chr_decrypt_with_szuki.py` is in `_archive/defunct`, i.e. an abandoned attempt; `*.chr_decrypted.bin` files exist next to 2 chars, so SOME decryption worked at runtime (memory dump?). Without a proven static cipher QoH99 is a RE project, not a format task. The `.Fob` (30 files, `01 00 00 00 31 00 eb 00 ...`) and `.Img` (340) are also unmapped.
5. **dMp/Rosa IMG obfuscation** is described only as "body looks obfuscated" in fb_titles_inventory; the 2005 Rosa repack proves a plain variant exists (diffable).
6. **Game acceptance of edited, unencrypted data**: unknown per title (see Q6). The PB/MB/GOF1 stage-2 cipher is keyed per section from fixed strings, so re-encrypting identically is easy; an "unencrypted mode" only works if the loader skips the XOR, which it does not for `備前長船` (the loader always decrypts).
7. **Shared-IDA hazard**: another agent switched the active IDB under me today; every IDA call must pass `instance_id`.
8. **Disk/IO**: archives are 100 MB-2.7 GB on a 9p mount; suites take minutes. Round trips stay streaming.
9. Lilian and Rosa are not fighters; a "character editor" has nothing to edit but scripts and sprite sheets.

## 6. DECISION QUESTIONS for the user

**Q1. Order of work.**
 a) EX3 viewer -> ReAct -> MB -> PB2K1 -> dMp -> Rosa -> QoH99/Lilian (recommended: closest-first, each step reuses the previous one, ~18 days for the first four with proofs).
 b) dMp first (PovertyCaster is actively shipping dMp netplay, so a live-reload loop has immediate users).
 c) QoH99 first (Aquat1c's game, hardest, most uncertain).
 Recommendation: (a), then (b) right after MB, because dMp has a working in-game test harness.

**Q2. dMp integration with the PovertyCaster DMP adapter.**
 a) Standalone Hantei-chan editor, user copies files by hand.
 b) Editor writes a patched `GAMEDATA.PAC` (hard-linked game dir) and PovertyCaster launches it: "edit -> Launch" button using the existing `tools/dmp_*` harness (recommended).
 c) True live reload into a running game (the adapter already knows the script-bank load/unload seam `Script_UnloadBanksFrom` 0x4130A0, so reloading a FOB bank is plausible but is a rollback/netplay-state risk).
 Recommendation: (b) now, (c) only as an offline-training feature later.

**Q3. Rosa and Lilian (non-fighters).**
 a) Skip both: nobody authors them and Hantei-chan is a fighter-authoring tool.
 b) Read-only viewers/extractors (FOB disassembly, IMG sheets, EX3 images, BG chips) so the files are not opaque, no editing (recommended: ~2-3 days total, comes almost free from dMp/EX3 work).
 c) Full editors.
 Recommendation: (b). Rosa rides on dMp; Lilian's BGC/MAP only if you care about its BGs.

**Q4. QoH99 depth.**
 a) Archive/extract only (done) and stop.
 b) Spend 1-2 days to establish whether the `.chr` cipher is actually solved (run `chr_decrypt_correct*.py` against the 37 chars; check the `*_decrypted.bin` against them), then decide (recommended).
 c) Commit to full RE and editor (5-10 days, uncertain).
 Recommendation: (b) first; if the cipher is solved and the model fits a GOF1-like layout, proceed, otherwise park it.
 Sub-question: should QoH98 (documented, unencrypted, nobody plays it) be included? Recommendation: only as a by-product, low priority.

**Q5. Round-trip bar for encrypted formats.**
 a) Byte-exact re-encryption of every shipped file (what the suite enforces today; recommended, since it proves the model is complete).
 b) Allow semantically equal output (editor normalizes padding/junk bytes).
 Recommendation: (a) for the suite, and let the editor preserve unknown/leftover bytes verbatim (as the archive layer already does).

**Q6. "Modded, unencrypted" output.** The games' loaders always run the XOR stages (MB/GOF1: `SpriteDataSlot_LoadCharacterDat` decrypts unconditionally and
 refuses version != 18; ReAct/MBAC `Hantei4` files are plain already; QoH98 is plain). So an unencrypted mode would only help ReAct/MBAC-style titles that already work that way.
 Options: a) always write the shipped (encrypted) form (recommended); b) additionally offer plain export for diffing/version control as a separate "Export decrypted" command, never as the save format.
 Recommendation: (a) for Save, (b) as an Export.

**Q7. Image replacement.** Replacing a sprite needs an EX3 compressor. The byte-pair coder is simple but the original tool's output was never reproduced bit-exactly.
 a) Skip: view/export only for now.
 b) Write our own compressor that produces VALID (not identical) EX3 and verify in-game (recommended, ~2 days; the games accept any valid stream because the decoder is table-driven).
 c) Chase bit-exact reproduction of the original tool (open-ended).
 Recommendation: (b).

**Q8. IDB hygiene scope.** The rule says rename/type everything you touch. For titles with IDBs (mb, mbr, pb2k1, DMP, qoh99) I would type only the character-load path and the structs used by the editor. For Rosa/Lilian, create IDBs (1 day each) only if Q3 = (c).
 Recommendation: the minimal path above, unless you want full typed coverage per title.

## User decisions 2026-10-03 (final)

1. Order: EX3 viewer, ReAct, MB, PB2K1, dMp, Rosa, Lilian, QoH99.
2. dMp: LIVE RELOAD NOW. Edits show up in the running dMp under PovertyCaster. Generic "asset hot-reload" seam on the PovertyCaster side (IPC; pchost receives
   `reload <archive/entry>`; the DMP adapter re-runs the game's own loader for that entry at a safe point, or declares the session non-netplay). Design first:
   `docs/formats/dmp_live_reload.md` here + a matching doc in PovertyCaster (worktree off origin/main, `pc_worktree_init.sh`, gate `PC_JOBS=2 nice -n 10 bash tools/pc_precommit.sh`; tell main before pushing).
3. Rosa and Lilian: FULL editors at fighter depth (scripts, sprites, maps / BG chips, story scripts), byte-exact, proven in-game.
4. QoH99: FULL support now. Solve the SZUKI MASAMI cipher from the qoh99 IDB (check pc-adapters/qoh99 and /mnt/c/dev/qoh first), full editor + byte-exact for all 37 .chr, .Fob, .Img. QoH98 too.
5. Encrypted formats: untouched load -> save is byte-exact; edits are re-encrypted with the game's own scheme.
6. Optional "modded, unencrypted" save mode only where the game is proven in-game to accept plaintext; otherwise the option is disabled with the reason shown.
7. Images: match French-Bread's compressor EXACTLY (EX3, IMG, any compressed sprite format): re-encoding the original pixels reproduces the shipped bytes bit for bit. Find the encoder in exes/tools; else infer from the data and prove on every shipped image.
8. IDA: full typing, no unk anywhere it touches, including runtime character structs, as for RBO/GOF.
