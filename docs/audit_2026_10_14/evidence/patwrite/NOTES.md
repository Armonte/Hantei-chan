# .pat writer: roster_v2 vs roster_v3 (ActCadenza -> MBAACC) — not a regression

Date 2026-10-14. Branch fix/pat-write (from update/parity-1014 e759fcb). No code change.

## What differs

`v2_v3_patdiff.py` (output in `v2_v3_patdiff.txt`) compares all 17 converted .pat files in
`gamedata/mbac_to_cc/roster_v2` and `roster_v3`:

- Every v3 file is exactly 20 bytes shorter. The only writer difference is one empty
  shape block that v2 wrote after the last part's `PPED` and before the first `PGST`:
  `VEST` u32 0, u32 0x10, `VNST`, `VEED` (20 bytes). v3 goes `PPED` -> `PGST` directly.
- With that block removed, 16/17 v2 files are byte-identical to v3.
- g_chaos.pat additionally differs only inside the `tex4` PGTX pixel payload
  (0x42024a..0x520248; everything outside that range is identical). That is source data:
  roster_v3 converts the ActCadenza 1.03A overlay `mbac_v11_english/06/G_CHAOS.DAT`
  (new parts texture), roster_v2 converted the 02.p one. Not a writer change.
- None of the S1 (PRAS/PRPA/flip/blend), S4+S5 (PNG/PGT2, type 22) or atomic-save changes
  show up in these files: no part records, shapes or texture headers differ.

## Source of the change

S2, commit a09185a (squash of feat/s2-pat-safety), src/parts/parts.cpp `Parts::Save`:

    if (!doneV && !useMBAACCFormat && (rawRecords.empty() || !shapes.empty()))
        encode('V', 0) ...

Before S2 (8330411) the empty VEST was dropped only on round trips of loaded MBAACC files
(rawRecords non-empty); a freshly built file (converter output, rawRecords empty) still got
one. S2 added `!useMBAACCFormat`, so freshly built MBAACC-layout files also omit it.

## Which side is correct

- Shipped MBAACC .pat files (`gamedata/mbaacc_ref/vanilla_0002/*.pat`, 19 files): zero
  `VEST`/`VNST` tags; all go `...PPED` -> `PGST` directly. v3 matches this; v2 did not.
- MBAA.exe (MBAA.exe.i64), top-level PAT loader `Pat_LoadFileAnyFormat` 0x404A40, tag loop
  at 0x404BF8..0x404C7C: reads one dword, dispatches `PPST` -> `Pat_ParsePPSTPartEntry`
  (0x404220, was File_ParsePPTPChunks), `PGST` -> `Pat_ParsePGSTTextureEntry` (0x404580,
  was DialogEntry_ParseFromFile), `P_ST` -> SpritePartition_ParseFromFile (PRST shapes),
  `_END` -> stop; any other dword is ignored and the next dword is read. There is no
  `VEST`/`VNST`/`VEED` constant anywhere in MBAA.exe.
- So MBAACC reads v2's block as five ignored dwords (`VEST`, 0, 0x10, `VNST`, `VEED`; none
  matches a dispatch tag) and loads v2 and v3 identically. v2 was harmless but carried a
  needless non-MBAACC block; v3 is the MBAACC convention and is the better output.
- Only the empty block was dropped: in every v2 file the VEST block was the empty 20-byte
  one, so no shape data was lost. MBAA.exe has no vector-shape support anyway.

## Verdict

Not a regression. The new output (v3) is correct and closer to shipped MBAACC files.
No fix and no golden test added (the step-3 fix path did not apply).

IDA: renamed 0x404220 -> Pat_ParsePPSTPartEntry, 0x404580 -> Pat_ParsePGSTTextureEntry, added
a comment on the tag loop at 0x404C21, saved the DB.
