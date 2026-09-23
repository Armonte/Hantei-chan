#include "bgm_player.h"
#include "misc.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis/stb_vorbis.c"

// ---------------------------------------------------------------------------
// bgm.txt
// ---------------------------------------------------------------------------

static std::string Trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

bool ParseBgmTxt(const std::string& path, std::vector<BgmEntry>& out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	out.clear();
	std::string line;
	BgmEntry* cur = nullptr;
	while (std::getline(f, line)) {
		line = Trim(line);
		if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
			out.push_back(BgmEntry{});
			cur = &out.back();
			cur->section = line.substr(1, line.size() - 2);
			continue;
		}
		if (!cur) continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		const std::string key = Trim(line.substr(0, eq));
		std::string value = line.substr(eq + 1), comment;
		const size_t slash = value.find("//");
		if (slash != std::string::npos) { comment = Trim(value.substr(slash + 2)); value = value.substr(0, slash); }
		value = Trim(value);
		if (key == "File") { cur->file = value; if (!comment.empty()) cur->comment = sj2utf8(comment); }
		else if (key == "IsLoop") cur->isLoop = std::atoi(value.c_str()) != 0;
		else if (key == "LoopPos") cur->loopPos = std::atof(value.c_str());
	}
	out.erase(std::remove_if(out.begin(), out.end(), [](const BgmEntry& e) { return e.file.empty(); }), out.end());
	return true;
}

long long AdvanceLoop(long long pos, long long total, long long loopStart, bool loop,
                      int frames, std::vector<long long>& indices)
{
	indices.resize(frames);
	if (loopStart < 0 || loopStart >= total) loopStart = 0;
	for (int i = 0; i < frames; ++i) {
		if (pos >= total) {
			if (loop && total > 0) pos = loopStart;
			else { indices[i] = -1; continue; }
		}
		indices[i] = pos++;
	}
	return pos;
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

BgmPlayer::BgmPlayer() = default;

BgmPlayer::~BgmPlayer()
{
	unload();
}

bool BgmPlayer::load(const std::string& oggPath, bool loop, double loopPos, std::string* error)
{
	unload();
	int channels = 0, rate = 0;
	short* data = nullptr;
	const int frames = stb_vorbis_decode_filename(oggPath.c_str(), &channels, &rate, &data);
	if (frames <= 0 || !data || channels <= 0 || rate <= 0) {
		if (data) free(data);
		if (error) *error = "Could not decode " + oggPath;
		return false;
	}
	m_pcm.assign(data, data + (size_t)frames * channels);
	free(data);
	m_channels = channels;
	m_rate = rate;
	m_totalFrames = frames;
	m_loop = loop;
	m_loopStart = std::clamp<long long>((long long)(loopPos * rate + 0.5), 0, std::max<long long>(0, frames - 1));
	m_pos = 0;
	m_loops = 0;
	m_path = oggPath;
	return true;
}

void BgmPlayer::unload()
{
	stop();
	m_pcm.clear();
	m_pcm.shrink_to_fit();
	m_totalFrames = 0;
	m_path.clear();
}

void BgmPlayer::seek(double seconds)
{
	if (!m_rate) return;
	m_pos = std::clamp<long long>((long long)(seconds * m_rate), 0, m_totalFrames);
}

double BgmPlayer::position() const
{
	return m_rate ? (double)m_pos.load() / m_rate : 0.0;
}

double BgmPlayer::length() const
{
	return m_rate ? (double)m_totalFrames / m_rate : 0.0;
}

bool BgmPlayer::play()
{
	if (m_pcm.empty() || m_playing) return m_playing;
	if (m_thread.joinable()) m_thread.join(); // previous run ended on its own
	m_quit = false;
	m_playing = true;
	m_thread = std::thread([this] { worker(); });
	return true;
}

void BgmPlayer::stop()
{
	m_quit = true;
	if (m_thread.joinable()) m_thread.join();
	m_playing = false;
}

void BgmPlayer::worker()
{
	WAVEFORMATEX fmt{};
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = (WORD)m_channels;
	fmt.nSamplesPerSec = (DWORD)m_rate;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = (WORD)(m_channels * 2);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
	HWAVEOUT dev = nullptr;
	if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
		m_playing = false;
		return;
	}
	constexpr int kBuffers = 4;
	const int frames = m_rate / 20; // 50 ms per buffer
	std::vector<std::vector<short>> bufs(kBuffers, std::vector<short>((size_t)frames * m_channels));
	WAVEHDR hdr[kBuffers]{};
	std::vector<long long> idx;
	auto fill = [&](int b) {
		const long long before = m_pos.load();
		const long long after = AdvanceLoop(before, m_totalFrames, m_loopStart, m_loop, frames, idx);
		if (after < before) m_loops++;
		m_pos = after;
		const float vol = m_volume.load();
		short* out = bufs[b].data();
		for (int i = 0; i < frames; ++i)
			for (int c = 0; c < m_channels; ++c)
				out[i * m_channels + c] = idx[i] < 0 ? 0 : (short)(m_pcm[(size_t)idx[i] * m_channels + c] * vol);
		hdr[b] = WAVEHDR{};
		hdr[b].lpData = (LPSTR)out;
		hdr[b].dwBufferLength = (DWORD)(frames * fmt.nBlockAlign);
		waveOutPrepareHeader(dev, &hdr[b], sizeof(WAVEHDR));
		waveOutWrite(dev, &hdr[b], sizeof(WAVEHDR));
	};
	for (int b = 0; b < kBuffers; ++b) fill(b);
	while (!m_quit) {
		bool any = false;
		for (int b = 0; b < kBuffers; ++b) {
			if (hdr[b].dwFlags & WHDR_DONE) {
				waveOutUnprepareHeader(dev, &hdr[b], sizeof(WAVEHDR));
				if (!m_loop && m_pos.load() >= m_totalFrames) { m_quit = true; break; }
				fill(b);
				any = true;
			}
		}
		if (!any) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	waveOutReset(dev);
	for (int b = 0; b < kBuffers; ++b)
		if (hdr[b].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(dev, &hdr[b], sizeof(WAVEHDR));
	waveOutClose(dev);
	m_playing = false;
}
