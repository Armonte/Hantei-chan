# S8b handoff: ActCadenza -> MBAACC data follow-ups (feat/s8b-actcadenza-data)

Base: update/parity-1014 bfffbce. Reference: ActCadenza Ver.1.03A (D1). No push or merge was done.
Rules and IDA evidence: `Hantei_Docs/HANTEI_MBAC_SUPPORT.md` §4 rule 12; audit `docs/mbac_moon/CONVERTER_FIDELITY.md` §2c
(outside the repo, pre-S8b copy in `roster_v3/_audit/CONVERTER_FIDELITY.pre_s8b.md`); plan status `docs/actcadenza_parity/PLAN.md` P1.

## Done
1. **Guts from 1.03A.**
   - 06.p is decrypted exactly now: `tools/ha4/actcadenza_pk_extract.py` (PK format from mbacPC `PKArchive_Open` 0x431670 /
     `PKArchive_ReadEntry` 0x431A30) → `gamedata/mbac_v11_english/06/` (1730 files). `VECTOR.TXT` and `G_CHAOS.DAT` are
     byte-identical to the earlier recovered `patch_vs_100/*_103A`. 12 CTs differ, as the SPEC says.
   - **[CORRECTED] guts index.** `Slot_LoadCommandCT` copies CT+0x2264.. to slot+0x304, and the damage code reads
     slot+0x310+4q = CT+0x2270+4q = `_C.TXT` status floats **f1..f4**. f0 is never read. roster and roster_v2 used f0..f3
     (one HP quarter off for every character).
   - `ha4tool convert` reads the CT: `--ac-version 1.03A` (default) or `1.00`, `--ac-patch DIR` (06), `--ac-base DIR` (02).
     The 1.00 CT status block matches `_C.TXT` for all 24 characters (`roster_v3/_audit/ct100_vs_txt_convert_log.txt`).
   - The NECO/NECHAOS values are exact (no longer "as decoded"): NECO f1..f4 1.4/0.7/1.1/0.9; NECHAOS 0.7/1.4/1.1/0.9.
     They are odd, but they are what 1.03A ships.
   - GAKIHA and F_CIEL CTs are 8908 B. The converter accepts them (same offsets).
2. **GAKIHA giant reactions** (`--giant-flag 2 --giant-donor g_akiha.HA6`).
   - MBAA `Victim_ApplyPendingHits` @0x471F09: a KO hit forces 44/45 for GiantFlag 1/2. Otherwise flag 2 uses vector
     HitAni and flag 1 gets no reaction.
   - GAKIHA gets G_AKIHA's 25/26/27/29/30/44/45/350 (donor timing/AS/IF, own p24 hit pose and hurt boxes, unresolved donor
     EF spawns dropped) plus 900–908/351/354.
   - G_CHAOS (flag 1 vs Hermes): 44/45 are present, so nothing was added.
3. **Spawn leaks** (`--mbaacc-data`).
   - The whole roster has only the known set: akaakiha 130, akiha 129/130, hisui 206, shiki 175–179.
   - They are retargeted to a 1-frame invisible no-op at 990 (aniType 0, like MBAACC's own effects 216/217) and logged per
     character in `roster_v3/_audit/convert_log.txt`.
4. **ExShieldTime.**
   - `[Type_3]` has 6/6/8.
   - Sidecar: `data\actcadenza_moon.ini`, format `docs/actcadenza_parity/ACTCADENZA_MOON_SIDECAR.md`. 21/22 characters have
     air 10; ARC has none.
   - Per character, the converter writes `<c>_actcadenza_moon.ini`.
5. **Outputs** (gamedata, not git):
   - `mbac_to_cc/roster_v3/`: 50/50 OK; the 1.03A G_CHAOS.DAT comes from 06.
   - `g_chaos/cc_data_v3`: the HA6/pat/cg are from v3; `_9_c.txt` is the emitted file. The v2 file was the raw
     positional MBAC TXT, which MBAACC can't read.
   - `g_akiha_mbac/cc_data_v3`: the v2 hand-written commands plus a `[Status]` from the CT.
   - `actcadenza_moon3/` (new): the 22-character moon-3 set and `_reports/`.
   - `moon3_proto/data`: refreshed; the old copy is in `data_pre_s8b/`.
   - Vanilla MBAACC data is extracted from `0002.p.bak` to `gamedata/mbaacc_ref/vanilla_0002/`. The loose
     `C:\games\mbaacc\data` differs for Hermes/hisui/p_arc/p_ciel/ries/roa/ryougi/s_akiha .HA6, so the vanilla copy is used.
6. **P3 package list** (for cg2/PovertyCaster):
   - `docs/actcadenza_parity/P3_PACKAGE_FILES.tsv` (75 files) and `.md` (path mapping);
   - `P3_PACKAGE_FILES_NEEDS_SCHEMA2_INJECT.tsv` (G_AKIHA_MBAC, 8 files).
   - **Only `data\_type.txt` replaces a vanilla file.** It is the whole vanilla 0002.p `_type.txt` (sha256 8050798…, which
     equals the loose copy) with `[Type_3]` appended, and every vanilla byte is kept.
7. **Tests/audit.**
   - `ha4_convert_test` covers CT parsing, the command file, leak no-ops, giant slots, and AKIHA 1.03A/1.00 + GAKIHA real data.
   - convverify rows G1/G2/L1/S1 are 24/15/17/22 → 0
     (`evidence/s8b/convverify_before_after.md`).
8. **IDA:**
   - mbacPC 1.00 and 1.03A IDBs: `[S8B]` CT-layout comments on `Slot_LoadCommandCT` and `Damage_ComputeProrated`.
   - 1.03A IDB: `sub_432880` → `Str_ToUpperCopy`.
   - Both IDBs saved.

## Results
- `./build.sh` clean. ctest **38/38** (`evidence/s8b/ctest.log`); `ha4_convert_test` 167 checks, 0 failed.
- `tools/uni/uni_regress.sh`: **UNI_REGRESS_PASS**, rc 0, with ActCadenza `ha4tool roundtrip` **50/50 byte-identical**
  (`evidence/s8b/uni_regress.log`). The 1.03A `06/G_CHAOS.DAT` also round-trips byte-identically.
- Audit before/after (`evidence/s8b/convverify_before_after.md`): M rows still 0; G1 24→0, G2 15→0, L1 17→0, S1 22→0.

## Open
- P2: append the unmatched sprites to `<c>_3.cg`; parts frames (ARC 133, WARC 100, …) still point at ActCadenza `.pat`
  ids; the AFOF fix; `<c>_3.cpf`; `vector_ac.txt`; SeList.
- Moon-3 set keeps the converter's 900–908 (ActCadenza 23–26 reactions, D3) over MBAACC's own; `--no-system-patterns` for
  the opposite.
- `.pat` from this build differs from roster_v2 for unchanged DATs too (e.g. GAKIHA, 1.00 G_CHAOS at byte 131343). This is
  the merged S1 PAT writer, not S8b; worth a look by the PAT owner.
- Not verified: MBAACC applies `Guard=` with the same −1/−2/−3 base and ×1.07 (SPEC row R13).
