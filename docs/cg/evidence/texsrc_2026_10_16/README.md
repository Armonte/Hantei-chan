# CG + .pat textures tab: undo, RBO / GOF2 sheets, UNIB DXT3, palette mod-folder steps (2026-10-16), branch fix/texsrc-complete

Closes the HANDOFF list of docs/cg/evidence/cgbrowser_2026_10_16/README.md (tab undo, RBO/GOF2 sheets, UNIB DXT3).

- Undo: every replace / import in the tab is one step. .pat textures and CG images in a format the Bank model does not hold go into
  the CG manager history (texture records + parts that followed the size / the CG's stored bytes); Bank images use the existing
  commit(). French-Bread sheets (and a .pat document) go into the PAT editor's history (parts channel); the tab's Undo / Redo drive that
  history while a sheet is selected. Undo / redo re-upload the GL copy (Texture::Apply skipped already-applied textures, so before this
  a replaced .pat texture never reached the main view) and mark the character edited.
- GUI crash on Undo (first run, ui_pat_undo exit 5): the tab drew `hist.top().label` from a `canUndo` computed before the Undo press,
  so undoing the only step read back() of an empty vector. The stale `s3tc` pointer lead was refuted: after Parts::Load the pointer is
  only read by the load-time upload loop. Fix: History::topLabel() re-read after the buttons; selftest replays the exact sequence.
- RBO / GOF2: SheetTexSource (han2::edit sheet code) as the third source kind; save = the character save (han2 writers, .DAT / .DT2).
- UNIB DXT3: loader, GL upload, pat_texconv DXT3 codec (fix/texsrc-dxt3 2a61b97, merged).
- Palettes: "Save .pal to mod folder" (and the tab's "Save to mod folder") are now one undo step each (the file before is put back,
  or removed when there was none). The other palette operations were already one step each.

Evidence: ctest.log (87/87, exit 0); texsrc_pat_{0_original,1_replaced,2_undone,3_redone}.png (UNI2 chr003 .pat texture 0);
texsrc_rbo_{0_sheet,1_replaced,2_after_tab_undo}.png (RBO ACOLYTE_F lineage sample); scripts tools/ui/shots/texsrc_*.txt;
cgm_suite_unib_dxt3.log. Suite numbers: texsrc-sheet RBO 208/208, GOF2 64/64 (sheet PNG round trips byte-exact, replace -> undo -> redo
byte-exact); UNIB texsrc-pat 14/14 with 0 unsupported textures.
