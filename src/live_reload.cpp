#include "live_reload.h"
#include "misc.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace livereload {

bool ValidRelPath(const std::string &p, std::string *why)
{
	auto no = [&](const char *m) { if (why) *why = m; return false; };
	if (p.empty() || p.size() > 240) return no("empty or too long");
	if (p[0] == '/' || p[0] == '\\') return no("absolute path");
	if (p.size() > 1 && p[1] == ':') return no("drive letter");
	if (p.find('\\') != std::string::npos) return no("use forward slashes");
	if (p.find('\n') != std::string::npos || p.find('\r') != std::string::npos) return no("control character");
	size_t i = 0;
	while (i <= p.size()) { size_t j = p.find('/', i); if (j == std::string::npos) j = p.size(); const std::string seg = p.substr(i, j - i); if (seg == ".." || seg == "." || seg.empty()) return no("empty, '.' or '..' path segment"); i = j + 1; }
	return true;
}

std::string RequestLine(const std::string &relPath) { return "reload " + relPath + "\n"; }

bool ParseAck(const std::string &line, const std::string &relPath, Result &out)
{
	for (const char *kw : { "ok ", "refused ", "failed " }) {
		const std::string pre = std::string(kw) + relPath;
		if (line.compare(0, pre.size(), pre) == 0) {
			out.ok = kw[0] == 'o';
			std::string rest = line.substr(pre.size()); while (!rest.empty() && (rest[0] == ' ')) rest.erase(0, 1);
			out.status = std::string(kw, strlen(kw) - 1) + (rest.empty() ? "" : ": " + rest);
			return true;
		}
	}
	return false;
}

static bool WriteNew(const fs::path &dst, const void *data, size_t n)
{
	std::error_code ec; fs::create_directories(dst.parent_path(), ec);
	return WriteFileAtomic(dst.u8string().c_str(), data, n);
}

Result Reload(const std::string &gameDir, const std::string &relPath, const std::vector<uint8_t> &bytes, int timeoutMs)
{
	Result r; std::string why;
	if (gameDir.empty()) { r.status = "set the game folder first"; return r; }
	if (!ValidRelPath(relPath, &why)) { r.status = "bad path: " + why; return r; }
	const fs::path dir = fs::u8path(gameDir);
	std::error_code ec;
	if (!fs::is_directory(dir, ec)) { r.status = "game folder does not exist"; return r; }
	if (!WriteNew(dir / fs::u8path(relPath), bytes.data(), bytes.size())) { r.status = "could not write " + relPath; return r; }
	const fs::path ack = dir / "pchost_reload.ack", req = dir / "pchost_reload.req";
	fs::remove(ack, ec);
	if (fs::exists(req, ec)) { r.status = "the previous request has not been picked up yet (is the game running under PovertyCaster?)"; return r; }
	const std::string line = RequestLine(relPath);
	const fs::path tmp = dir / "pchost_reload.req.tmp";
	{ std::ofstream f(tmp, std::ios::binary); f.write(line.data(), (std::streamsize)line.size()); if (!f) { r.status = "could not write the request"; return r; } }
	fs::rename(tmp, req, ec);
	if (ec) { r.status = "could not publish the request: " + ec.message(); return r; }
	const auto t0 = std::chrono::steady_clock::now();
	while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() < timeoutMs) {
		std::ifstream f(ack, std::ios::binary);
		if (f) { std::string l; while (std::getline(f, l)) { while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back(); if (ParseAck(l, relPath, r)) return r; } }
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	fs::remove(req, ec);
	r.status = "no answer from the game (not running under PovertyCaster, or its adapter has no asset reload)";
	return r;
}

std::string GuessLoosePath(const std::string &basename, const std::vector<std::vector<uint8_t>> &fobs)
{
	auto low = [](std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; };
	const std::string want = low(basename);
	for (auto &d : fobs) {
		size_t i = 0;
		while (i < d.size()) {
			if (d[i] == '.' && i + 7 < d.size() && (d[i + 1] == '\\') ) {
				size_t j = i; while (j < d.size() && d[j] >= 0x20 && d[j] < 0x7F) j++;
				std::string s((const char *)&d[i], j - i);
				const std::string ls = low(s);
				if (ls.size() > want.size() && ls.compare(ls.size() - want.size(), want.size(), want) == 0 && ls[ls.size() - want.size() - 1] == '\\') {
					std::string out = s.substr(2); std::replace(out.begin(), out.end(), '\\', '/'); return out;
				}
				i = j;
			} else i++;
		}
	}
	return {};
}

} // namespace livereload
