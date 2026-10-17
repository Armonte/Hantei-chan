# Japanese UI review (click-through)

Method: `tools/i18n_shoot/` drives the real `gonptechan.exe` (Language=1) with scripted mouse input; each shot is the application window only
(`PrintWindow` of the app window plus its own popup windows, never the desktop; a screen copy is refused when a foreign topmost window overlaps).
Every image was viewed after capture. Content: MBAA `akiha.HA6`, RBO `AMBERNITE.DAT` (no .DAT CG, so the loading report warns), GOF2 `AMIY.DT2`
from `data02.dat`, GOF1 `gof_03.p::AKIKO.DAT`, RBO `DATA01.PAC` and GOF2 `data02.dat` in the archive browser. Nothing was copied from the game installs.
Checker: `python3 tools/i18n_check.py` reports missing=0 (it now also verifies printf specifiers match between English and Japanese rows);
`tools/i18n_literals.py` lists untranslated literals in helper/std::string code paths.

## Coverage (99 screenshots)

| File | View | Status |
|---|---|---|
| `i18n_ja_authoring_tab_hud.png` | Authoring workspace, HUD tab (fixture tag tree) | clean |
| `i18n_ja_authoring_tab_live.png` | Authoring workspace, LIVE tab | clean |
| `i18n_ja_authoring_tab_log.png` | Authoring workspace, LOG tab | clean |
| `i18n_ja_authoring_tab_setup.png` | Authoring workspace, SETUP tab (fixture tag tree) | clean |
| `i18n_ja_authoring_tab_setups.png` | Authoring workspace, SETUPS tab (fixture tag tree) | clean |
| `i18n_ja_authoring_tab_tuning.png` | Authoring workspace, TUNING tab (fixture tag tree) | clean |
| `i18n_ja_dd_pattern_combo.png` | Pattern drop-down (pattern names are game data, already Japanese) | clean |
| `i18n_ja_lp_animation_data.png` | MBAA left pane: animation data | clean |
| `i18n_ja_lp_pattern_data.png` | MBAA left pane: pattern data | clean |
| `i18n_ja_lp_state_data.png` | MBAA left pane: state data | clean |
| `i18n_ja_lp_tools.png` | MBAA left pane: tools | clean |
| `i18n_ja_main_gof1.png` | Main window, GOF1 AKIKO.DAT | clean |
| `i18n_ja_main_gof1_startup_dialogs.png` | GOF1 startup: inspector open | clean |
| `i18n_ja_main_gof2.png` | Main window, GOF2 AMIY.DT2 (from data02.dat) | clean |
| `i18n_ja_main_gof2_startup_dialogs.png` | GOF2 startup: loading report (no .DAT warning) + inspector | clean |
| `i18n_ja_main_mbaa.png` | Main window, MBAA HA6 (akiha), 1900x1000 | clean |
| `i18n_ja_main_mbaa_default_size.png` | Main window at the default 1560x950 size (box pane wraps its buttons) | clean |
| `i18n_ja_main_rbo.png` | Main window, RBO AMBERNITE.DAT | clean |
| `i18n_ja_main_rbo_startup_dialogs.png` | RBO startup: loading report + HAN2 inspector auto-open | clean |
| `i18n_ja_menu_authoring.png` | Menu: authoring | clean |
| `i18n_ja_menu_edit.png` | Menu: edit | clean |
| `i18n_ja_menu_file.png` | Menu: file | clean |
| `i18n_ja_menu_file_package.png` | Menu: file package | clean |
| `i18n_ja_menu_help.png` | Menu: help | clean |
| `i18n_ja_menu_prefs.png` | Menu: prefs | clean |
| `i18n_ja_menu_prefs_bgcolor.png` | Menu: prefs bgcolor | clean |
| `i18n_ja_menu_prefs_extension.png` | Menu: prefs extension | clean |
| `i18n_ja_menu_prefs_filter.png` | Menu: prefs filter | clean |
| `i18n_ja_menu_prefs_gameformat.png` | Menu: prefs gameformat | clean |
| `i18n_ja_menu_prefs_style.png` | Menu: prefs style | clean |
| `i18n_ja_menu_prefs_zoom.png` | Menu: prefs zoom | clean |
| `i18n_ja_menu_stage.png` | Menu: stage | clean |
| `i18n_ja_menu_stage_open_game.png` | Menu: stage open game | clean |
| `i18n_ja_menu_view.png` | Menu: view | clean |
| `i18n_ja_menu_view_onion.png` | Menu: view onion | clean |
| `i18n_ja_menu_windows.png` | Menu: windows | clean |
| `i18n_ja_popup_load_error_gof1_entry.png` | Load error popup, missing GOF1 entry (translated detail) | clean |
| `i18n_ja_popup_load_error_not_character.png` | Load error popup, file cannot be opened | clean |
| `i18n_ja_popup_loading_error_cmdfile.png` | Loading Error modal (command file) shown while About was open | clean |
| `i18n_ja_popup_unsaved_changes.png` | Unsaved-changes modal on closing a modified character | clean |
| `i18n_ja_rbo_lp_animation_data.png` | RBO left pane: animation data | clean |
| `i18n_ja_rbo_lp_pattern_data.png` | RBO left pane: pattern data | clean |
| `i18n_ja_rbo_lp_state_data.png` | RBO left pane: state data | clean |
| `i18n_ja_rbo_lp_tools.png` | RBO left pane: tools | clean |
| `i18n_ja_rbo_rp_effect_flagsets.png` | RBO right pane: effect flagsets | clean |
| `i18n_ja_rbo_rp_effect_open.png` | RBO right pane: effect open | clean |
| `i18n_ja_rbo_rp_effect_type1_spawn.png` | RBO right pane: effect type1 spawn | clean |
| `i18n_ja_rbo_rp_effect_type7.png` | RBO right pane: effect type7 | clean |
| `i18n_ja_rbo_rp_effect_type8_actor.png` | RBO right pane: effect type8 actor | clean |
| `i18n_ja_rbo_rp_effect_type_combo.png` | RBO right pane: effect type combo | clean |
| `i18n_ja_rbo_rp_effects.png` | RBO right pane: effects | clean |
| `i18n_ja_rp_attack_data.png` | MBAA right pane: attack data | clean |
| `i18n_ja_rp_condition_added.png` | MBAA right pane: condition added | clean |
| `i18n_ja_rp_condition_type2.png` | MBAA right pane: condition type2 | clean |
| `i18n_ja_rp_condition_type_combo.png` | MBAA right pane: condition type combo | clean |
| `i18n_ja_rp_effect_open.png` | MBAA right pane: effect open | clean |
| `i18n_ja_rp_effect_type1_spawn.png` | MBAA right pane: effect type1 spawn | clean |
| `i18n_ja_rp_effect_type2.png` | MBAA right pane: effect type2 | clean |
| `i18n_ja_rp_effect_type3.png` | MBAA right pane: effect type3 | clean |
| `i18n_ja_rp_effect_type5.png` | MBAA right pane: effect type5 | clean |
| `i18n_ja_rp_effect_type_combo.png` | MBAA right pane: effect type combo | clean |
| `i18n_ja_rp_spawn_visualisation.png` | MBAA right pane: spawn visualisation | clean |
| `i18n_ja_win_about.png` | Window/panel: about | clean |
| `i18n_ja_win_anim_player.png` | Window/panel: anim player | clean |
| `i18n_ja_win_archive_browser.png` | Archive browser, DATA01.PAC of RBO | clean |
| `i18n_ja_win_archive_browser_gof2.png` | Archive browser, GOF2 data02.dat unfiltered | clean |
| `i18n_ja_win_archive_browser_filtered.png` | Archive browser, GOF2 data02.dat filtered to AMIY.DT2 | clean |
| `i18n_ja_win_bg_inspector_scrolled.png` | Background Inspector, scrolled to object frame fields | clean |
| `i18n_ja_win_bgm_preview.png` | Window/panel: bgm preview | clean |
| `i18n_ja_win_cg.png` | Window/panel: cg | clean |
| `i18n_ja_win_cmd_editor.png` | Command file editor, Commands tab | clean |
| `i18n_ja_win_cmd_editor_excomcheck_row.png` | Command file editor: excomcheck row | clean |
| `i18n_ja_win_cmd_editor_tab_excomchecks.png` | Command file editor: tab excomchecks | clean |
| `i18n_ja_win_cmd_editor_tab_other.png` | Command file editor: tab other | clean |
| `i18n_ja_win_cmd_editor_tab_problems.png` | Command file editor: tab problems | clean |
| `i18n_ja_win_cmd_editor_tab_tagdata.png` | Command file editor: tab tagdata | clean |
| `i18n_ja_win_diff.png` | Window/panel: diff | clean |
| `i18n_ja_win_game_link.png` | Window/panel: game link | clean |
| `i18n_ja_win_game_view.png` | Game view window | clean |
| `i18n_ja_win_game_view_menu_game.png` | Game view menu: game | clean |
| `i18n_ja_win_game_view_menu_input.png` | Game view menu: input | clean |
| `i18n_ja_win_game_view_menu_view.png` | Game view menu: view | clean |
| `i18n_ja_win_han2_inspector.png` | Window/panel: han2 inspector | clean |
| `i18n_ja_win_hud_colours.png` | Window/panel: hud colours | clean |
| `i18n_ja_win_hud_gauge_sheets.png` | Window/panel: hud gauge sheets | clean |
| `i18n_ja_win_hud_portraits.png` | Window/panel: hud portraits | clean |
| `i18n_ja_win_keyboard_shortcuts.png` | Window/panel: keyboard shortcuts | clean |
| `i18n_ja_win_load_report.png` | Window/panel: load report | clean |
| `i18n_ja_win_notes_memo.png` | Window/panel: notes memo | clean |
| `i18n_ja_win_pac_create.png` | Window/panel: pac create | clean |
| `i18n_ja_win_pat_editor.png` | Window/panel: pat editor | clean |
| `i18n_ja_win_pattern_compare.png` | Window/panel: pattern compare | clean |
| `i18n_ja_win_pattern_manager.png` | Window/panel: pattern manager | clean |
| `i18n_ja_win_stage_browser.png` | Window/panel: stage browser | clean |
| `i18n_ja_win_stage_browser_detail.png` | Window/panel: stage browser detail | clean |
| `i18n_ja_win_stage_open_bg_inspector.png` | Stage opened: Background Inspector + Stage Browser | clean |
| `i18n_ja_win_tag_legacy_panel.png` | Legacy tag_tuning.ini (Tag / Team) panel | clean |
| `i18n_ja_win_variable_ref.png` | Window/panel: variable ref | clean |
| `i18n_ja_win_vector_guide.png` | Window/panel: vector guide | clean |

Menus covered: File (+MBAACC package submenu), Edit, Preferences (all 6 submenus), Stage (+Open game stage), View (+Onion skin), Windows, Experimental: Authoring, Help.
Not shot separately (reason): PAT editor panes appear together in `win_pat_editor` (docked: part set, part, shape, texture, tool panes); the box pane is in every main-window shot.

## Defects found and fixed

1. Box pane buttons wrapped/clipped: button rows now use `i18n::SameLineFit` (wrap to the next line when a Japanese label does not fit).
2. Pattern combo pushed the ID field out of the left pane: combo width is computed from what the label, ID field and search button need.
3. Pattern data row (PSTS / Level / Flag) and state-data (Max X speed), attack-data (Circuit break time, Cust HS/BS), effect/condition rows (Manual, Delete, Copy all / Paste all / Add copy): same wrapping fix.
4. Right-aligned Copy/Paste button pairs in the PAT editor panes used fixed pixel offsets and overlapped: `i18n::RightPairX` right-aligns from the measured button widths.
5. Disabled-text hints that ran off the window (pattern manager, HUD preview, left pane tools): `i18n::TextDisabledWrapped`.
6. HUD preview colour rows overflowed: colour edit widths now scale with the window.
7. Authoring Setup: assist combos showed the clipped word "チューニン"; label changed to 設定値 and the five combos shrink to fit the column.
8. English leaking into Japanese: SHIPPED/local overlay, layer names (shipped/local), log filter hint and empty-log hints, "Sprite dims", "as player N", lever group headings in the legacy Tag panel (SeparatorText), warning counts, command-file editor (window title, tabs, search hint, error/warning severity, ExCom type names/summaries/parameter help, diagnostics, status messages, save/discard modal titles), keyboard-shortcut action names (28 rows), `archive ` prefix in the inspector, error-popup detail lines (`i18n::TrDetail` for "Could not open ...", "<name> is not in <archive>" etc.).
9. Error popups used English technical text: common ones are translated through `TrDetail`; parser diagnostics with byte offsets stay English on purpose.
10. Vector guide window called `End()` too few times when no vector.txt was loaded (ImGui assertion risk): fixed.
11. Background Inspector default width widened (540) so Japanese labels fit.
12. Format-string reorders in four Japanese rows now keep the English specifier order (checker enforces it).
13. Duplicate-looking "Revert" and "Undo" both read 元に戻す: Revert is now 保存状態に戻す.

## Deliberately left English

- Game/format/file names, field names generated from game structures in the HAN2 inspector (spriteId +0x0, G2FLIP_NONE ...), numbers, hex, key chords (Ctrl+Z), file paths.
- The `EN` / `English` language items and `Language / 言語` (bilingual by design).
- Debug diagnostics block of the Background Inspector (bgCamera pan, obj[0] ... values; developer output) except its section labels.
- `Command file: ` title is Japanese, but the file name is data. ExComCheck / PSTS / HS / BS / AT / AS abbreviations are game terms.
- Technical parser diagnostics in Load/Save errors that quote byte offsets, area numbers or section names (the lead phrase is translated when known).
- Pattern and move names inside characters are game data (already Japanese for MBAA, English or Japanese as authored for RBO/GOF).

## Residual issues

- The command-file editor `Other sections` tab shows the raw file text (CP932 comments, mojibake in the half-width kana header comment is original data).
- Authoring Live and Log tabs have no game attached in these shots (no game launched), so their live tables are empty by design.
- Tooltips are not captured (they vanish when the pointer is parked); their texts are covered by the table checker.
