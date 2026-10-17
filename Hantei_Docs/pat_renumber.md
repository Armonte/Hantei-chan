# Part set renumbering (.pat)

Between patches French-Bread reorganises a .pat's part set ids in whole blocks. For example, the UNI2 Zohar patch's
character select .pat moved the `1p_qcc_win` group to 179-181, left 202 as an empty `〇` gap, started `qcc_cursor` at
203-210 and `csel_intro` at 216, and put `name_chr0342p000` and `icon_Sai_000` in their own blocks. Before this tool
Hantei-chan could only append slots, so added part sets ended up far from their group. Renumbering moves the slots and
rewrites every reference, so the game draws the same thing afterwards.

Code: `src/parts/pat_renumber.{h,cpp}` (core, no GL), `src/PatEditor/pat_partset_renumber.cpp` (UI),
`src/ui/pat_editor_impl.h` (save), `src/pattool.cpp` (CLI).

## Operations

| Operation | Effect |
|---|---|
| Move a-b to t | The block leaves its place, later ids close up, and the block is inserted so that it starts at t. A target past the end pads with empty ids. |
| Place a-b at t | The block goes to t..t+len-1, which must be empty. The ids it leaves become empty gaps, and nothing else shifts. |
| Insert n at t | n empty ids at t; t and every later id shift up by n. |
| Delete a-b | The block is removed and later ids close up. This is refused while a frame still draws one of those ids. |
| Clear a-b | The ids stay as empty `〇` gaps, as French-Bread's files do. This is refused while a frame draws one of them. |
| Swap a b | The two ids trade places. |

An empty slot is a hole in the file, meaning no `P_ST` record has that id (64 of the 66 UNI2 .pat files have them). The
existing "Delete Part Set" button still keeps an empty `P_ST` stub, as it did before.

## References

- **HA6 frames.** An AF layer draws a .pat part set when its usePat flag is set (`AFGP`: flag, id; `AFGX`: layer,
  flag, id). The users of a .pat are the `[DataFile]` HA6 files of every project `.txt` in the .pat's folder whose
  `[PAniFile] File00` names the .pat. Examples are UNI2 `chrNNN_0.txt` (`_temp.ha6`, `chrNNN.ha6`), `csel_chrNNN.txt`
  and `effect.txt` (`effect_temp.ha6`, `effect.ha6` with `sys_effect.pat`). Only the 4 id bytes of those layers are
  rewritten, at the offsets the loader records (`g_ha6SpriteRefSink`, the same mechanism as the CG manager's
  `cgm_project`). Every other byte stays identical.
- **Shared files.** A file that is listed with another .pat, sits outside the folder, or is `BaseData` (UNI2
  `../BaseData.ha6` is listed by every character) is never rewritten. An operation that would change an id such a
  file draws is refused.
- **Effects, scripts and tables.** Nothing else in the data names part set ids. Effects spawn HA6 patterns. The
  Squirrel scripts (`script/*.txt`, `chrNNN_*.txt`) and the `.txt` tables name patterns, not parts: a search of the
  UNI2 data for PAT/part references finds only the `[PAniFile]` lines. Ids that game code draws directly (exe-side UI)
  cannot be seen from the data.
- **References past the end of the .pat** (they draw nothing) move like empty ids.

## Editor

Open the .pat on its own (PAT document). The part set pane has a **Renumber part sets** section with the operation,
the ids, and **Preview...**. The preview lists every user file, how many references it has and how many change, plus
any refusal. **Apply** changes the model in one frame, so it is one undo step on the PAT document's history (Ctrl+Z /
Ctrl+Y). Applying does not touch the HA6 files. The pane then shows how many references will change on save.

**Save** checks the plan again. It refuses if a refusal applies, if an affected HA6 is open in another tab, or if the
target is not the .pat's own file. Otherwise it writes the .pat, backs up each affected HA6 (auto-backup, S2), and
rewrites the HA6 files atomically.

How it works: each slot keeps the id it had in the loaded file (`PartSet::origId`, which is undo state), and
`Parts::diskIdOfOrig` records which id each of those has in the file on disk. Save writes a moved part set's
original record bytes with only its `P_ST` id changed, as one ascending block. The pending HA6 remap is
disk id -> current id, so undoing after a save and saving again moves the HA6 ids back.

## pattool

```
pattool refs <pat>
pattool renumber <pat> --move 179-181:300 --insert 202:1 [--place a-b:t] [--delete a-b] [--clear a-b] [--swap a:b] [--dry-run] [--no-backup]
pattool check <pat> --tmp <dir> [ops]
```

`renumber` applies the ops in order, prints the plan (files, references, refusals), and exits 3 when refused. It writes
the .pat and the HA6 files with backups. `check` runs on a copy of the folder and verifies three things:

- a no-op save is byte-identical;
- undoing the ops through the PAT undo channel restores a byte-identical .pat;
- after save, every reference draws a part set with the same content as before, and every byte except the id fields
  is identical.

ctest: `pat_renumber_*` on the UNI2 csel .pat files and `sys_effect.pat`.

Pixel check: `tools/pat/pat_renumber_shots.sh` copies three UNI2 csel projects (csel_chr007, 018 and 023), renumbers
one copy with `pattool renumber`, and shoots every PAT frame of pattern 0 of both copies with
`gonptechan.exe --headless --batch`. All shots must be identical. As a negative control, the renumbered .pat with the
original HA6 must draw something different.

## Appending part sets from another .pat

```
pattool append-from <dst.pat> <src.pat> -o <out.pat> --map <map.tsv> [--sets 1,5,7-9]
pattool append-check <dst.pat> <src.pat> --tmp <dir> [--sets ...]
```

`append-from` (src/parts/pat_append.h) copies source part sets (default: every non-empty one) to the end of the
destination, with the cut-outs, shapes and textures they draw, every inner reference (ppId, cut-out texture and
shapeIndex) renumbered. The new part sets get ids >= the destination's part set count, in ascending source order;
`map.tsv` lists `src_partset_id<TAB>new_id`, for remapping the HA6 frames that drew the source ids (pattool does not
rewrite HA6 files here). The destination keeps every id and writes its own records with their original bytes. A source
cut-out / shape / texture identical to one in the destination is reused. Textures are re-encoded for the destination:
MBAACC gets 32-bit PGTX (PGTP = width when the destination's textures carry it; ActCadenza files from `ha4tool convert`
have none), UNI2 / MBTL get lossless A8R8G8B8 PGT2. MBAACC has no shapes: a non-plane source shape is reported.

Use: the ActCadenza moon-3 set of MBAACC, whose HA6 frames draw ActCadenza part set ids while `<c>_3.txt` loads
MBAACC's `<c>.pat`.

`append-check` saves to `<dir>/out.pat`, reloads it, and checks that every destination record is byte-identical and
that each appended part set has the same parts, cut-outs, shapes and texture pixels as in the source. ctest:
`pat_append_*` (arc, warc, hisui subset into MBAACC; an MBAACC .pat into a UNI2 csel .pat).
