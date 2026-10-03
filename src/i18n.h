#ifndef I18N_H_GUARD
#define I18N_H_GUARD

// App-wide UI language (English / Japanese). English source strings are the keys; the Japanese table is split per area in
// src/i18n_ja_*.inc (each: lines of  {"English", "日本語"},  ). A string without a table entry shows in English.
//   TXT("Text")        translated text (use for text, tooltips, menu paths that are not widget IDs)
//   LBL("Label")      translated label with a STABLE ImGui ID: "日本語###Label" (window titles, buttons, menu items, tree nodes):
//                    switching language never resets window state / positions, and ID-based code keeps working.
// Format strings: translate the whole format string, keep the same % specifiers in the same order.
#include <string>

namespace i18n {
extern int language;                       // 0 English, 1 Japanese
const char *Tr(const char *en);            // pointer valid until the next call on the same thread slot (cached per key)
const char *Label(const char *en);         // "translated###English" (English id), or "English" when language == 0
void Load();                               // reads han2_settings.ini [han2] Language (shared with the HAN2 windows)
void Save();
// Combo boxes whose item names are English tables: the items are translated at draw time (the stored value is still the index).
// Same signatures as ImGui::Combo (items array, or one string of items separated by \0).
bool Combo(const char *label, int *current, const char *const *items, int count, int heightInItems = -1);
bool Combo(const char *label, int *current, const char *itemsSeparatedByZeros, int heightInItems = -1);
// Row layout that survives longer Japanese text: call right after an item; continues on the same line only if the next item
// (nextWidth px, e.g. ButtonWidth(label) or FieldWidth(w, label)) still fits in the window, otherwise the next item starts a new line.
// Error / status detail text built from English pieces: translates the whole line, a known prefix ("cannot open " + path), or
// the part after "name: ". Unknown technical diagnostics (byte offsets etc.) stay English.
std::string TrDetail(const std::string &s);
void SameLineFit(float nextWidth, float spacing = -1.f);   // spacing < 0: the style's ItemSpacing.x (pass 20 to match old SameLine(0,20) rows)
void TextDisabledWrapped(const char *fmt, ...); // TextDisabled that wraps at the window edge (long Japanese lines)
float RightPairX(const char *a, const char *b);        // window-local X that right-aligns two adjacent buttons
float ButtonWidth(const char *label);                 // visible width of ImGui::Button(label)
float FieldWidth(float itemWidth, const char *label); // item of itemWidth px followed by its label
}

#define TXT(s) ::i18n::Tr(s)
#define LBL(s) ::i18n::Label(s)

#endif
