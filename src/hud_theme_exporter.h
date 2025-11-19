#ifndef HUD_THEME_EXPORTER_H_GUARD
#define HUD_THEME_EXPORTER_H_GUARD

#include <filesystem>

// Writes the HUD theme profile used by the CCCaster plugin into the specified directory.
// When overwrite is false and the file already exists, the function leaves the existing file untouched.
bool ExportHudThemeProfile(const std::filesystem::path& directory, bool overwrite = false);

#endif /* HUD_THEME_EXPORTER_H_GUARD */

