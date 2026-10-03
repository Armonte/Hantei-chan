# Main menu reorganisation: old -> new

Layout: File / Edit / View / Tools / Stage / Game / Window / Help.
Source: `src/ui/main_menu_impl.h` (+ `DrawViewRenderItems`, `DrawDetachedWindowItems`, `DrawDetachPreference` in `src/ui/workspace_hosts_impl.h`).
Enable conditions, checkmarks, shortcut labels and tooltips are unchanged; every command appears exactly once.
Evidence screenshots (EN and JA): `docs/ui/evidence/`.

## New tree

```
File:   Open... | Open Recent > | Import > | Save Character / As / Merged Stack / as MOD |
        Save Parts / As | Export > | Project > | New Character / Reopen Closed Tab / Close Character | Exit
Edit:   Undo / Redo (+ tuning / stage variants) | Keyboard shortcuts | Preferences >
View:   Zoom level | Filter | Background color | Palette number | PUPS palette files | Game format | Language |
        Onion skin | Export PNG | Export current frame
Tools:  Animation player | Diff | Pattern manager | Pattern comparison | Variable references |
        Command File Editor | Open Command File | PAT editor | CG sprites | PAC browser | Create/patch PAC | MBAACC package >
Stage:  (as before, minus Stage Browser)
Game:   Game Link | Authoring > | Game view | HUD preview | BGM preview | Legacy >
Window: pane toggles | MBAC/HAN2 inspectors | loading report | Notes | Vectors Guide | Stage Browser | detached windows
Help:   About
```

## Mapping

| Old path | New path |
|---|---|
| File > New Project / Open Project... / Recent Projects > / Save Project / Save Project As... / Close Project | File > Project > (same names) |
| File > New Character, Reopen Closed Tab, Close Character | File (unchanged, below Project) |
| File > Load from .txt... | File > Import > Load from .txt... |
| File > Load chr HA6 from .txt... | File > Import > same |
| File > Load HA6... | File > Import > same |
| File > Load MBAC .DAT (Act Cadenza)... | File > Import > same |
| File > Load RBO / GOF2 character (.DT2/.DAT)... | File > Import > same |
| File > Load HA6 and Patch... | File > Import > same |
| File > Load Commands (_c.txt)... | File > Import > same |
| File > Load CG... / Load palette... / Load vector.txt... / Load Parts (.pat)... | File > Import > same |
| File > Effect.ha6 status + Reload Effect.ha6 | File > Import > (bottom) |
| File > Save Character / Save Character As... / Save Merged Stack As... / Save as MOD... | File (unchanged) |
| File > Save Parts (.pat) / Save Parts As... | File (unchanged) |
| File > Export MBAC as HA6... | File > Export > same |
| File > Export RBO / GOF2 sprites, poses and animations | File > Export > same |
| File > Exit | File > Exit |
| File > RBO / GOF2 archives (PAC)... | Tools > RBO / GOF2 archives (PAC)... |
| File > Animation player... | Tools > same |
| File > Changes against the loaded RBO / GOF2 file (diff)... | Tools > same |
| File > CG sprites of this RBO / GOF2 character... | Tools > same |
| File > Create / patch a PAC archive... | Tools > same |
| File > Edit parts ... (PAT editor) | Tools > same |
| File > Command File Editor (active character) / Open Command File... | Tools > same |
| File > MBAACC package > (validate / consolidate) | Tools > MBAACC package > |
| (new) | File > Open... (smart, header based), File > Open Recent |
| Edit > Undo/Redo/History/tuning/stage entries, Keyboard shortcuts | Edit (unchanged) |
| Preferences > Extension profile | Edit > Preferences > Extension profile |
| Preferences > Switch preset style | Edit > Preferences > Switch preset style |
| View > Native detached windows (restart) | Edit > Preferences > same (setting, `DetachableWindows` ini key unchanged) |
| Preferences > Background color / Game format / PUPS palette files / Palette number / Zoom level / Filter | View > same |
| Windows > Language / 言語 | View > Language / 言語 |
| View (old top menu) > Onion skin, Export PNG..., Export current frame | View > same |
| View (old) > Move tab to new window, detached window list, Return all tabs | Window > same |
| Windows > Hide/Show panes (Animation, Pattern Search Bar, Attack, Hitbox, PartSet, Part, Shape, Texture, Tool) | Window > same |
| Windows > Vectors Guide / Notes | Window > same |
| Windows > MBAC (HA4) Inspector / RBO / GOF2 (HAN2) Inspector / loading report | Window > same |
| Windows > Variable references | Tools > same |
| Windows > Pattern manager / Pattern comparison | Tools > same |
| Windows > BGM preview / HUD preview / colours | Game > same |
| Windows > Stage Browser | Window > Stage Browser |
| Windows > Game Link (MBAACC) | Game > Game Link (MBAACC) |
| Experimental: Authoring > Authoring workspace, Setup, Saved setups, Tuning, TAG HUD layout, Live, Game log | Game > Authoring > (same names) |
| Experimental: Authoring > Game view | Game > Game view |
| Experimental: Authoring > Legacy tag_tuning.ini panel | Game > Legacy > Legacy tag_tuning.ini panel |
| Stage > Load Stage File ... Next stage, all checkboxes | Stage (unchanged) |
| Help > About | Help > About |

## Removed duplicates (same command, one entry kept)

| Removed | Reason / kept at |
|---|---|
| Stage > Stage Browser | duplicate of Windows > Stage Browser; kept in Window (decision 1 lists it there) |
| Experimental: Authoring > Game Link console | same toggle (`gamelink::showPanel`) as Game Link; kept as Game > Game Link (MBAACC) |
| Windows > Tag / Team (experimental) | same as Authoring > Tuning (`authoring::Open("Tuning")`); kept as Game > Authoring > Tuning |

Nothing else was dropped. The top menu "Preferences", "Experimental: Authoring" and the old "Windows" no longer exist (Preferences became Edit > Preferences, the others are covered above).

## File > Open... (smart open)

`MainFrame::openAnyFile` reads the first 4 KiB and decides by magic, never by extension alone:

| Header | Route |
|---|---|
| `Hantei6DataFile` or `Hantei4\0` (MBAC .DAT) | `CharacterInstance::loadHA6` (it already tells HA6 and HA4 apart) |
| `HAN2RBO ` (RBO/GOF2 .DT2/.DAT), PAC archive, GOF1 `.p` archive, PAT magic (`0x01234567` at +4), `BMP Cutter` (.CHP) | `openHan2File` (PAC / .p open the archive browser) |
| `bgmake` (stage .DAT) | `loadStageFile` |
| `.hproj` with a JSON header | `openRecentProject` (unsaved-changes prompt) |
| text containing `[DataFile]` | `loadFromTxt` |
| anything else | "Load Error" popup naming the file; points to File > Import for vector / command / palette / CG files |

File > Import keeps every per-format loader. Drag and drop of `.ha6 .txt .hproj .p` now also uses this routing. Files opened through Open... go into File > Open Recent (new ini key `RecentFile=`, max 10; `RecentProject=` is untouched).
Test hook: `--open-any a|b|c` runs the same routine at startup.
