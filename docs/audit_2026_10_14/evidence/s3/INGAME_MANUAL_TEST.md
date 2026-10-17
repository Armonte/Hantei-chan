# S3 in-game check (UNI2 1.42): done 2026-10-14 with gamecap (per-process injection, no desktop input or grabs)

## Result
- **The game renders the edited AFRG.** `ingame/mem_tinted_p0_f0.png` shows Hyde standing (p0 f0) tinted red.
  `ingame/mem_vanilla_p0_f0.png` is the same frame before the change. Mean RGB over Hyde's torso goes from
  (89,91,118) to (89,58,89): green and blue drop, red holds, as a (255,77,77) multiply should do.
  `ingame/mem_tinted_p96_f1.png` is a control: another pattern stays normal.
- **The `___St\` loose-file route does not work on 1.42.** `ingame/override_*.png` are from a run with
  `<UNI2>\___St\data\chr000\chr000.ha6` in place, and Hyde is untinted there. Cause, from the 1.42 IDB
  (`C:\dev\ida\server\uni2_142_4ebed985.exe.i64`, renamed and commented):
  - `OpenFileViaSearchPaths` (0x4B9720) opens a loose `<slot><name>` file if one exists.
  - It then still calls `ArchiveIndex_OpenEntryByName` (0x4B9EA0) on the unprefixed name, at 0x4B9971.
  - If the packs hold that name, the archive entry is used and the loose handle is dropped.
  - So loose files can only add files, never override packed ones.
  - The earlier note was based on the stale install-folder IDB (an older exe).
- The install was restored (`___St\` removed). `ingame/install_before.txt` vs `install_after.txt` (a depth-2 listing
  with sizes and mtimes) differs only in the root folder's own mtime.

## How the memory run works (`ingame/s3_tint_mem.gcs`)
- Nothing on disk changes. gamecap boots UNI2 into training (Hyde vs Linne) and freezes P1 on p0 f0.
- It then writes, in that process's RAM only, the AFRG the edited `ingame/chr000.ha6` carries for p0 f0 layer 0
  (255,77,77; the only layer with a sprite).
- The address is `[P1+OBJ_FRAMEDATA]+0x10..0x12`:
  - `Han6_LoadFrame` 0x4C5090 passes frame+4 to `Han6_LoadFrameAF` 0x4C6060.
  - Layers are 48 bytes apart, and AFRG is at layer+12..14.
  - The peeks in `gamecap_mem.log` read 255,255,255 before and 255,77,77 after.
- So this proves the field and value render in game. It does not prove the file loads. File correctness is covered
  by the byte-exact round trip and `ingame/patdiff.txt` (only p0 differs).

## Loading the edited file itself (not done; it needs an install change)
Two ways:
- the nuuni `winmm` proxy ("d folder bypass", `nuuni d folder bypass - winmm proxy dll.7z` in the install), or
- a gamecap hook on `ArchiveIndex_OpenEntryByName` that returns "not found" for `data/chr000/chr000.ha6`, so the
  loose `___St\` file is used. This one is per process, so it is preferred.

Run: `tools/gamecap/build/gamecap.exe C:\...\evidence\s3\ingame\s3_tint_mem.gcs`
(`s3_tint.gcs -D TAG=override|vanilla` is the loose-file run.)
