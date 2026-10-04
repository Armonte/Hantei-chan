#ifndef EXTENSION_PROFILE_H_GUARD
#define EXTENSION_PROFILE_H_GUARD

// Which game executable the editor authors for.
//
//  Vanilla  - stock MBAACC. Default. Only engine-verified behaviour is offered, and the
//             command-file validator accepts everything the shipped data files contain.
//  Extended - Extended Melty / BOF patched executables (Gonptechan EX's target). Adds the
//             BOF-only IDs: IF 154-157, EF6 154, the Var6 projectile-level presets,
//             IF 14 collider filters, ExComCheck Types 2/3, and the stricter BOF _c rules
//             (unique command IDs, Flagset 1 class 0/1/2).
//
// Stored in the ImGui ini under [Extension profile][] (see RegisterExtensionProfileSettings).

enum class ExtensionProfile : int { Vanilla = 0, Extended = 1 };

ExtensionProfile GetExtensionProfile();
void SetExtensionProfile(ExtensionProfile profile);
inline bool ExtendedProfileEnabled() { return GetExtensionProfile() == ExtensionProfile::Extended; }
const char* ExtensionProfileName(ExtensionProfile profile);

struct ImGuiContext;
// Adds the ini handler. Call before ImGui::LoadIniSettingsFromDisk.
void RegisterExtensionProfileSettings(ImGuiContext& context);
// Radio items for a Preferences menu.
void DrawExtensionProfileMenu();

#endif
