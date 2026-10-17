// See pcmod_vanilla.h.
#include "pcmod_vanilla.h"
#include "pcmod_manifest.h"
#include "pcmod_package.h"
#include "../mbaacc_pack.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace pcmod {

static const char* kDefaultGame = "C:\\games\\mbaacc";

std::string DefaultGameDir()
{
	std::error_code ec;
	return fs::exists(fs::u8path(std::string(kDefaultGame) + "\\MBAA.exe"), ec) ? kDefaultGame : "";
}

std::string DefaultExeSha256(const std::string& gameDir)
{
	if (gameDir.empty()) return {};
	return Sha256OfFile((fs::u8path(gameDir) / "MBAA.exe").u8string());
}

bool VanillaIndex::Load(const std::string& dir)
{
	*this = VanillaIndex{};
	std::error_code ec;
	if (dir.empty() || !fs::exists(fs::u8path(dir) / "MBAA.exe", ec)) return false;
	gameDir = dir;
	for (const std::wstring& pack : mbpack::FindPacks(fs::u8path(dir).wstring())) {
		mbpack::Archive a;
		std::string err;
		if (!a.open(pack, err)) continue;
		for (const mbpack::Entry& e : a.entries()) {
			std::string norm, why;
			if (NormalisePath(e.relativePath(), norm, why)) packed.insert(norm);
		}
	}
	loaded = true;
	return true;
}

const char* VanillaIndex::Where(const std::string& normPath) const
{
	if (!loaded) return "";
	// A loose file in the game folder is what the game reads first, so it wins over the same path inside a .p archive:
	// report "loose" even when the archive also holds the path (p_arc.* is both).
	std::error_code ec;
	if (fs::is_regular_file(fs::u8path(gameDir) / fs::u8path(normPath), ec)) return "loose";
	if (packed.count(normPath)) return "packed";
	return "";
}

} // namespace pcmod
