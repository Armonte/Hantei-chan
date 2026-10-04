# Effect recolour config: shared spec (Hantei-chan authoring <-> PovertyCaster runtime)

Decision (2026-10-03): **runtime-only recolour, lossless, no game data is modified.** Hantei-chan is the authoring and preview tool; the PovertyCaster
runtime draws the recolour with a pixel shader (technique `TecRecolor`) at effect-quad draw time. Neither side writes sprites or palettes.

## Single source of truth

The grammar, the match rule, the colour maths and the CPU reference of the shader are one header in PovertyCaster:
`pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp` (branch `feat/fx-recolor-runtime`). Hantei-chan vendors it **byte-identical** as `src/cgm/fxr_spec.hpp`
(namespace `mbaacc::fxr`) and calls `parseIni`, `writeIni`, `findRule`, `pack` and `applyCpu` directly, so the editor's load/save and its live preview
are the runtime's own code, not a re-implementation.

* Keep them in sync: `tools/fxrecolor/fxr_sync.sh check` (exit 1 on drift), `fxr_sync.sh pull` to take the runtime's version. Change the format in the runtime
  header first, pull, then adapt the editor.
* File: `<game>\fxrecolor\<char>.ini`, `<char>` = the lowercase roster stem (`akiha`, `arc`, ...). A file replaces the built-in default set of that character.
* One `[rule <id>]` section per rule, first match wins (file order). Keys: `patterns`, `sprites`, `slots` (id lists, `a-b` ranges, empty or `*` = any), `bank`
  (`any|char|effect`), `blend`, `kind` (`lumramp|rainbow|hsv`), `by` (`max|luma`), `vrange`, `ramp` (stops `#rrggbb | accent | accent.dark | accent.light`, optional
  `@pos`), `accent_idx`, `hsv`, `slot.N.accent`, `slot.N.ramp`, `enabled`. Authoritative text: the header comment.
* `pattern` = HA6 pattern index of the object that draws the quad; `sprite` = CG image id. Both are what Hantei-chan's usage index reports.

## What Hantei-chan adds (not in the file)

* Classification (`src/cgm/fxr_core.*`): an *effect sprite* is an image of type != 0 (the engine draws type 0 through the slot palette, everything else with its
  own colours); an *effect pattern* draws at least one such sprite. Score = share of its layers that are fixed-colour; shared sprites are flagged (per-pattern rules).
* Per-pattern granularity is the `patterns` list of a rule (+ `sprites`); `AssignPatterns` moves patterns between rules and disables a rule whose list it emptied
  (an empty list would mean "any").
* The slot palette: Hantei-chan stores palettes as memory RGBA (`0xAABBGGRR`); `ToRuntimePalette` converts to the `0xAARRGGBB` that `autoAccent` documents.
  **Open question for the runtime:** `MbaaccSim_FxRecolor.cpp` copies `CG+0x10` raw into the same `autoAccent` input. The engine's mode-2/0 upload reads that
  palette as bytes R,G,B,A (`CG_UploadCellPalettized` writes `BYTE2` first = B), i.e. memory RGBA too, so unless something converts it first the runtime's accent has
  R and B swapped. Verify with a known skin colour (Akiha slot 0 entry 1 = f8e0d0).

**Runtime auto accent scans unused palette entries.** `autoAccent()` with no `accent_idx` looks at all 255 entries; Akiha's palette keeps unused
indices at pure green, which it then picks (the editor showed #00ff00 for slot 0). The editor's auto-suggest therefore writes `accent_idx = N`, where N is
the body palette index (used by type-0 sprites) that is vivid across slots *and* differs between them (`SuggestAccentIndices`); each slot's accent is then the
colour that slot gives index N. New rules get this index by default. Suggest a runtime-side guard too (ignore indices no body sprite uses) so rules without
`accent_idx` do not pick junk.

## Starter ruleset

`docs/cg/effect_recolor_data/dgv_akiha.ini`: DGV's nine Akiha colour groups (folders 00 01 02 10 20 21 30 80 90 of his "Better Akiha v2" project) as `lumramp`
rules keyed by sprite id, ramps fitted to his indexed output from the original pixels (`tools/fxrecolor/dgv_groups.py`). Fit against his data
(`dgv_akiha_oracle.csv`, `tools/fxrecolor/dgv_oracle.py`, which runs the real shader maths through `cgmtool fxr shade`): groups 00/01/02/10/20/21/30 within ~1-4/255
mean error (94-100 % of pixels within 24/255); 80 (multi-hue) and 90 (single sprite) are approximate. Credit: DGV, "Better Akiha".

## CLI

`cgmtool fxr classify|check|shade|apply|dgv` (see `src/cgm/fxr_cli.cpp`). `shade` is the oracle hook: RGBA on stdin -> recoloured RGBA on stdout.
