#ifndef PCMOD_VANILLA_H_GUARD
#define PCMOD_VANILLA_H_GUARD
// Which game-relative paths a stock MBAACC install already has: loose files under the game folder plus every entry of its
// FilePacHeaderA .p archives (mbaacc_pack.h). The exporter warns when a package file REPLACES one of them; that is
// allowed, but the package must ship the whole file (the game reads it instead of the vanilla one, there is no patching).
// Win32 (the .p reader is UTF-16 at the boundary).
#include <set>
#include <string>

namespace pcmod {

struct VanillaIndex {
	std::string gameDir;               // UTF-8, "" = not found
	std::set<std::string> packed;      // normalised ('\', lowercase) paths inside the .p archives
	bool loaded = false;
	// Loads the archive listing; returns false (and leaves the index empty) when the folder has no MBAA.exe.
	bool Load(const std::string& gameDirUtf8);
	// "packed" / "loose" / "" (not a vanilla file). `normPath` is normalised.
	const char* Where(const std::string& normPath) const;
};

// C:\games\mbaacc when it holds MBAA.exe, else "".
std::string DefaultGameDir();
// SHA-256 of <gameDir>\MBAA.exe ("" when absent).
std::string DefaultExeSha256(const std::string& gameDirUtf8);

} // namespace pcmod
#endif
