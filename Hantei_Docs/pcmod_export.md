# pcmod package export (PovertyCaster mod manager)

Hantei-chan writes mod packages for PovertyCaster's MBAACC mod manager. The format is `pcmod.json` **schema 1**, pinned on
2026-10-14. The spec is `docs/modmanager/pcmod-manifest_schema1.md` in the main checkout. This page covers the
exporter only. For the conflict and online rules, see the spec.

## What a package is

```
<id>\pcmod.json        the manifest: id, version, class, exe hashes, requires/conflicts/overrides, and every file with
                       its SHA-256 and size
<id>\data\...          the files, at their game-relative paths
<id>-<version>.zip     the distribution archive: store-only, with a single top-level <id>/ directory
```

- **Every file has a class, decided by its path** (spec §3, revised 2026-10-14 to cover the whole MBAACC tree):

  | path | file class |
  |---|---|
  | `data\**` | **sim** |
  | `data\**.pal`, `data\_csel\**` | cosmetic (palettes and character-select art) |
  | `story\**`, `charaselect.txt` | **sim** |
  | `bg\`, `bgm\`, `dat\`, `grp\`, `se\`, `shader\`, `movie\`, `palettes\` | cosmetic |
  | `hud_theme.json`, `font.fnt`, `_sysfont.ini` | cosmetic |
  | anything else: `PackDataVersion\`, `System\`, `Information\`, other root files | **refused** (fails closed) |

  `sound\` is no longer a root (MBAACC has none). `dat\` holds title / VS screen presentation only.
  `PackDataVersion\` stays refused because it decides whether the update pack loads.
- **Package class `sim`** needs at least one sim file. It may also ship cosmetic files. Those are in its package digest
  but not in its *sim digest*, so two players may differ in a sim package's palettes or art and still play online.
- **Package class `cosmetic`** may not list a sim file. It is never compared online.
- **Paths** are normalised to `\` and compared case-insensitively. These are refused: an empty path, a leading `\`, any
  `:`, a `.` or `..` segment, an empty segment, a control character, more than 240 bytes, a segment ending in a space or
  dot, and a DOS device name (`CON`, `NUL`, `COM1`, ...).
- The writer never emits the keys reserved for schema 2 (`inject`, `palettes`, `exe_layers`). The reader refuses them,
  and it also refuses any unknown or duplicate key.

## GUI: File > Export > "Export as pcmod package..."

The same item is also under Tools > MBAACC package.

- **With a character open from its `.txt`**, the list starts with these files ticked: the descriptor, every loose file
  it references (HA6 / CG / PAT rows), `<name>_c.txt` and `<name>.pal`. Other files in that folder whose names start
  with the character's name are listed unticked.
- **Add file... / Add folder...** builds any other set. A folder laid out like the game (it has `data\`, `grp\`, ...)
  keeps its relative paths. Otherwise the game path is guessed from the last game-root folder in the file's path
  (`data`, `story`, `bg`, `bgm`, `dat`, `grp`, `se`, `shader`, `movie`, `palettes`), or from a known root file name.
  Every game path can be edited in the list.
- Each row shows its **Class** column: `sim (online)` for a file that goes into the online sim digest, `cosmetic`, or
  `refused` in red. Next to the class combo, "N of M file(s) go into the online sim digest" counts the ticked sim files.
- Each row also shows the size, the SHA-256 (hover for the full hash) and a note:
  - **REPLACES vanilla (in a .p archive / loose file)**: the MBAACC install at `C:\games\mbaacc` already has this
    path. This is allowed, but the game then reads only the packaged file, so it must be the **whole file**, not a patch.
  - **new file**: the install does not have this path.
  - In red: a refused path, a path outside every game root, or a sim file in a cosmetic package.
- **Class** defaults to Auto: `sim` if any ticked file is a sim file, otherwise `cosmetic`. You can force either.
- **MBAA.exe SHA-256** defaults to the hash of `C:\games\mbaacc\MBAA.exe` when that file exists. Enter one hash per
  line. A sim package must name at least one.
- **Requires** takes `id@x.y.z` entries (an exact version: schema 1 has no ranges). **Conflicts** and **Overrides** take
  ids. Separate entries with commas or spaces.
- **Ready: N files, package digest ...** and **Online sim digest ... (N sim file(s))** are shown live. **Export
  package** writes the folder and the zip, then reads both back and verifies them before it reports success. **Copy
  digest** copies the package digest for the catalogue entry; **Copy sim digest** copies the sim digest.
- If `<out>\<id>` already holds an earlier export (a folder with a `pcmod.json`), it is replaced. Any other non-empty
  folder with that name is refused, never overwritten.

UI script (`--ui-script`, see `tools/ui/ab_shot.sh`):
`pcmod open [chara.txt] | pcmod add <file> | pcmod addfolder <dir> | pcmod set id|version|name|author|out|exe|priority|class <v> | pcmod export | pcmod status`.

## CLI: `pcmodtool`

```
pcmodtool pack <srcdir> --id <id> --version <x.y.z> [--class sim|cosmetic] --exe <sha|MBAA.exe path>...
          [--out <dir>] [--name ..] [--author ..] [--description ..] [--priority N]
          [--requires <id>@<x.y.z>]... [--conflicts <id>]... [--overrides <id>]... [--no-zip] [--game <dir>]
pcmodtool verify <dir|zip>          exit 0 = valid, 1 = broken
pcmodtool digest <dir|zip>          "package <hex>" and "sim <hex>" (the package and package sim digests)
pcmodtool session <exe> <pkgdir>... the session sim digest (<exe> = 64-hex or a path to MBAA.exe)
```

- `<srcdir>` holds the files at their game-relative paths. A root-level `pcmod.json` or `mod.ini` is skipped.
- `--class` is detected when omitted, and `pack` prints how many files go into the online sim digest. `--exe` defaults to the hash of `C:\games\mbaacc\MBAA.exe`.
- `--game` (default `C:\games\mbaacc`, `""` turns it off) prints a `REPLACES vanilla` line for each file that the
  install already has.
- `session` applies the §4 checks: verify failures, an exe not in the package's list, an unmet `requires`, an enabled
  `conflicts`, and a path shipped twice where a sim package is involved without `overrides`. It reads `mod.ini`
  `[Config] enabled= priority=`, and the local value wins. With no sim package enabled, it prints the vanilla-set digest.

## Digests (spec §5)

- package: `"pcmod-pkg-1\n" id "\n" version "\n" class "\n"`, then for each file, sorted by its lowercased `\` path:
  `path_lower \0 sha256 \0 size "\n"`. This is the catalogue identity of the package.
- package sim: `"pcmod-sim-1\n" id "\n" version "\n"`, then the same line for each **sim-class** file only, in the same
  order. For a cosmetic package it is the digest of the header alone.
- session: `"pcmod-set-1\n" exe_sha256 "\n"`, then for each enabled sim package (priority descending, then id):
  `id \0 version \0 package_sim_digest "\n"`. This is what the handshake compares.

ctest `pcmod` and `pcmod_cli` check these against PovertyCaster's fixture (`tests/fixtures/pcmod`, content from
PovertyCaster `343706aa4`, digests re-pinned at `96889671e`), with exe = 63 zeros then `1`:

| digest | value |
|---|---|
| package | `eb3db61182ed3ee9875f68f6f668a1671ff3744340e073dffcbfc966998ddf27` |
| package sim | `16a83aa066f3a94620eed7219529cb3b6463d158e080c02190345a1d1ddc6041` |
| session (that package enabled) | `8c4fe7957fcc0be5912de92c093dcab0a47d812b6d080e6ab5517588df7341cc` |
| vanilla set | `f9956a1ae59ce26aa14e3c4d97597da9a665727f9b87539cd48432b10636c0db` |

ctest `pcmod` also rebuilds the ActCadenza moon-3 package (P3) from `tests/fixtures/pcmod_p3/P3_PACKAGE_FILES.tsv` and
checks it. It has 75 files; its one `data\*.pal` is cosmetic and stays outside the sim digest:

| P3 digest | value |
|---|---|
| package | `7721d18a3b3f8984582ca65a27ceea41992d6c9db48958a4ea6503750c4b2fe9` |
| package sim | `d6d72ad9295f0354dcabcfeffe06d06d2b031a07488460fd87c329e040026ef8` |

## Code

- `src/pcmod/pcmod_manifest.*`: the model, the strict reader, the deterministic writer, the path rules and both digests.
  It has no filesystem code.
- `src/pcmod/pcmod_sha256.*`: SHA-256 (FIPS 180-4, public domain). The tree had none before.
- `src/pcmod/pcmod_zip.*`: a store-only zip writer and reader with fixed timestamps, so the same package always gives the
  same bytes.
- `src/pcmod/pcmod_package.*`: scan, pack, verify a folder or zip, read `mod.ini`, and resolve a session.
- `src/pcmod/pcmod_vanilla.*`: the index of the install's `.p` archives (`mbaacc_pack.h`) plus loose files.
- `src/pcmod/pcmod_export_ui.*`: the window. `src/pcmod/pcmodtool.cpp`: the CLI.

These were implemented from the spec. PovertyCaster's `pc-core/include/pc/mods/Manifest.hpp` was used only to check the
results. The zip reader accepts stored entries only: a deflated archive from another tool is refused by name.
