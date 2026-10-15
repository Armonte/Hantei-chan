#ifndef HMOD_VANILLA_H_GUARD
#define HMOD_VANILLA_H_GUARD
// Hash index of a game's vanilla archives (AUTHORING_DESIGN.md section 2.2): one row per EFFECTIVE entry (a later archive
// overrides an earlier one), with path, archive, size and SHA-256. Built through the fbarc readers (the same code as
// fbarctool: MBAACC .p with both ciphers, UNI2 d\ folder, MBTL.exe + data*.bin), cached as a TSV shared between projects.
// Win32-independent; paths are UTF-8.
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hmod {

struct VanillaEntry {
	std::string path;      // normalised: '\' separators, ASCII lowercase ("data\akiha_1_r.ha6")
	std::string pack;      // archive file name ("0003.p") or "d" / "MBTL.exe"
	uint64_t size = 0;
	std::string sha256;
};

// Normalised, hash-key form of a game-relative path ('\', lowercase, no leading ".\" or "\").
std::string NormKey(const std::string& rel);

class VanillaIndex {
public:
	// `archives`: archive files / folders in override order (later wins). MBAACC: 0000.p ... 0008.p (+ 0002.p.bak).
	// `progress(done, total, current)` is called per archive entry (may be null); returning false cancels.
	bool Build(const std::vector<std::string>& archives, const std::string& game, std::string& err,
	           const std::function<bool(size_t, size_t, const std::string&)>& progress = {});
	// Test and tooling hook: add a row without an archive behind it (ReadBytes then fails for it).
	void AddRow(const VanillaEntry& e);
	// Same, with the bytes kept in memory (ReadBytes serves them): synthetic baselines for tests and tools.
	void AddBytes(const std::string& path, const std::string& pack, std::vector<uint8_t> bytes);

	bool Save(const std::string& tsvPath, std::string& err) const;
	bool Load(const std::string& tsvPath, std::string& err);

	const VanillaEntry* Find(const std::string& normPath) const;
	// Plain bytes of a vanilla file (re-opens the archive it came from; needs Build() or SetArchives()).
	bool ReadBytes(const std::string& normPath, std::vector<uint8_t>& out, std::string* err = nullptr) const;
	// Archive locations for ReadBytes after Load().
	void SetArchives(const std::vector<std::string>& archives) { m_archives = archives; }

	// Loads the shared cache for this install if present (archives located from `gameDir`); false + err when it must be built first.
	bool LoadCached(const std::string& game, const std::string& gameDir, std::string& err);
	// Builds from the install and writes the shared cache; returns the cache path in `cachePath`.
	bool BuildCached(const std::string& game, const std::string& gameDir, std::string& cachePath, std::string& err,
	                 const std::function<bool(size_t, size_t, const std::string&)>& progress = {});

	size_t size() const { return m_rows.size(); }
	const std::string& game() const { return m_game; }
	// Hash over the archive headers (name, size) so a game update changes it: first 8 hex of SHA-256.
	const std::string& packset8() const { return m_packset; }
	const std::map<std::string, VanillaEntry>& rows() const { return m_rows; }
	bool empty() const { return m_rows.empty(); }

	// Default shared cache location: %LOCALAPPDATA%\Hantei-chan\vanilla\<game>-<packset8>.tsv (temp dir when unset).
	static std::string CachePath(const std::string& game, const std::string& packset8);
	static std::string ComputePackset8(const std::vector<std::string>& archives);
	// Archives of an install in override order: MBAACC "*.p" (+ "*.p.bak" without a sibling .p); UNI2 "<root>\d"; MBTL "<root>\MBTL.exe".
	static std::vector<std::string> FindArchives(const std::string& game, const std::string& gameDir);

private:
	std::map<std::string, VanillaEntry> m_rows;
	std::vector<std::string> m_archives;
	std::string m_game, m_packset;
	std::map<std::string, std::vector<uint8_t>> m_mem;
	struct OpenCache;
	mutable std::shared_ptr<OpenCache> m_cache;   // opened archives + path->entry maps, for repeated ReadBytes
};

} // namespace hmod
#endif
