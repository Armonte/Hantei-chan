# Squirrel Script Editor: Widget Triage

Date: 2026-10-15. Scope: pick a code-editor widget for Hantei-chan's Squirrel script editor (post-MBAACC French-Bread games: UNI2, MBTL, UNIB, DFCI).
Constraints: Dear ImGui v1.93.0-docking submodule, MinGW Windows build first, vendorable under zlib/MIT, CP932 decoded to UTF-8 by us (original bytes kept by us).

## 1. Editors listed on ImGui "Useful Extensions" wiki

| Widget | Listed as |
|---|---|
| goossens/ImGuiColorTextEdit | colorizing text editor (rewrite) |
| BalazsJako/ImGuiColorTextEdit | original, 2017-2019 |
| pthom/ImGuiColorTextEdit (branch imgui_bundle) | fork used by imgui_bundle |
| nealmick/ned | editor using GL shaders (an application, not an embeddable widget) |
| Rezonality/zep | embeddable editor, optional vim keys |
| Scintilla integration (issue #108) | obsolete, no repo |
| imgui_stdlib, ImGuiTextSelect | input helpers, not editors |

santaclose/ImGuiColorTextEdit is not on the page but is the well-known maintained fork of the original, so it is included.

## 2. Comparison (GitHub data as of 2026-10-15)

| | BalazsJako | santaclose | goossens (master) | pthom (master/imgui_bundle) | Zep |
|---|---|---|---|---|---|
| Last push | 2025-11 (code idle since 2019) | 2025-10 | 2026-10-13 | 2026-10-14 (master code snapshot 2026-03) | 2026-05 |
| Licence | MIT | MIT | MIT | MIT | NOASSERTION (MIT-style, custom file) |
| Open issues | 65 | 25 | 0 | 0 | 22 |
| ImGui needed | old API, uses removed/legacy calls, patches needed for 1.93 | works with recent ImGui via small fixes | built against 1.92.8/1.92.9, no deprecated calls, so 1.93 OK | 1.92.x-era snapshot of goossens, 1.93 OK | has own display/editor abstraction, heavy |
| Text model | UTF-8 in, vector of glyph lines | same | UTF-8 in/out, vector of glyph lines | same as goossens | own buffer, gap buffer based |
| Highlighter | regex per line (slow, std::regex) | regex per line | state-machine tokenizer, multiline comments/strings | same as goossens (older) | Zep syntax classes |
| Custom language | yes (regex table) | yes | yes (Language struct, keywords sets, customTokenizer) | yes | yes, more work |
| Error markers | line markers + tooltips | line markers | AddMarker (line) + AddSquiggle (range, tooltips) | markers (squiggles newer in goossens only) | limited |
| Autocomplete | none | none | built-in popup + callback, async suggestions | built-in | none |
| Undo/redo | yes | yes | yes, multi-cursor, transactions | yes | yes |
| Find/replace | no | basic | built-in UI, can be disabled in favour of our own | built-in | yes |
| 10k-line perf | poor (regex) | poor to fair | good (incremental colorize per dirty line) | good | good |
| Vendoring size | 2 files | 3 files | 2 files (TextEditor.h/.cpp, 2.1k + 14.2k lines) | 2 files (1.4k + 9.5k lines) | large library, many files |
| Needs | C++11 | C++11 | C++17 | C++17 | C++17, lots of deps |

## 3. Text model notes (goossens, read from source)

- Internal storage is one glyph (ImWchar codepoint plus colour) per character, one vector per line. Memory is high for 10k lines but fine (about 8 bytes per glyph).
- CR is dropped on insert (`character != '\r'`), so CRLF becomes LF on SetText. GetText returns LF only. We must remember the original line ending and re-apply it on save.
- Tabs are kept as `\t` unless SetInsertSpacesOnTabs(true), which only affects typed and pasted tabs, not existing ones.
- Trailing newline: kept as a final empty line, so a file ending in `\n` yields an empty last line. Strip or re-add on save.
- Invalid UTF-8 bytes are decoded to IM_UNICODE_CODEPOINT_INVALID (U+FFFD). It is lossy, so keep the original CP932 bytes on our side and only write back text that was edited. Without IMGUI_USE_WCHAR32, codepoints above U+FFFF are also replaced; irrelevant for CP932 text.
- Language is replaceable; Lua() and AngelScript() are shipped but no Squirrel. Squirrel is C-like with `//`, `/* */`, `#` comments, `@"verbatim"` strings.

## 4. Recommendation: goossens/ImGuiColorTextEdit (master)

Reasons: most actively maintained (commits this week, zero open issues), MIT, explicit support for the 1.92 line (1.93 is compatible, uses only public ImGui plus `imgui_internal.h`), state-machine tokenizer scales to 10k lines, line markers and range squiggles with tooltips are exactly what compile errors need, autocomplete hook and disableable find/replace, only two source files to vendor. pthom's copy is an older snapshot of the same code without squiggles and docs; the original and santaclose rely on std::regex and lack autocomplete; Zep is too large and its error-marker story is weak.

Risks: the April 2026 refactor changed API (use current master, not older tutorials); C++17 required (our project already builds that way, to be confirmed against mingw toolchain); pin the commit because the API is still moving.

### Integration note

- Vendored to `third_party/ImGuiColorTextEdit/`: `TextEditor.h`, `TextEditor.cpp`, `LICENSE` (commit b12d713e77e8edddff1e69973f67085ac9cfaf74). Skip TextDiff.*, dtl.h, extras/, example/, docs/. Add TextEditor.cpp to CMake; it includes `imgui.h` and `imgui_internal.h`, so link it with the imgui target.
- Class `TextEditor`. Set text: `SetText(std::string_view utf8)`. Get text: `std::string GetText() const`.
- Config: `SetTabSize(n)`, `SetInsertSpacesOnTabs(bool)`, `SetReadOnlyEnabled(bool)`, `SetFindReplaceEnabled(bool)`, `SetPalette(TextEditor::GetDarkPalette())`.
- Error markers: `ClearMarkers(); AddMarker(line, lineNumberColor, textColor, lineNumberTooltip, textTooltip);` for gutter lines, and `ClearSquiggles(type); AddSquiggle(DocPos start, DocPos end, type, ImU32 color, tooltip)` for ranges. Lines are 0-based in DocPos; check normalizeLine for the marker line base.
- Language: fill a `static TextEditor::Language sq;` with name "Squirrel", `singleLineComment="//"`, `singleLineCommentAlt="#"`, `commentStart="/*"`, `commentEnd="*/"`, `hasDoubleQuotedStrings=true`, `hasSingleQuotedStrings=true`, `stringEscape='\\'`, and keyword/declaration/identifier sets (base on Lua() at TextEditor.cpp:12919 and Cpp). Call `SetLanguage(&sq)`. Use `customTokenizer` for `@"..."` verbatim strings if wanted.
- Render: `editor.Render("##sq", ImVec2(w,h), 0 /*ImGuiChildFlags*/)`; returns bool (true when text changed, see header). Must be called between ImGui::Begin/End. Undo: `Undo()`, `Redo()`.
- Autocomplete: `SetAutoCompleteConfig(&cfg)` with `cfg.callback` filling `SetAutoCompleteSuggestions({...})` (native Squirrel API names, entity/script function names from our docs).
