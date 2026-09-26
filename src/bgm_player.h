#ifndef BGM_PLAYER_H_GUARD
#define BGM_PLAYER_H_GUARD

// BGM preview (issue #66): the game's Bgm folder, its bgm.txt loop table, and
// an Ogg Vorbis player that honours the loop point (at the end of the track,
// playback continues from LoopPos, as the game does).
//
// bgm.txt (MBAACC, CP932):
//   [BGM_001]
//   File = bgmme01 //comment
//   IsLoop = 1
//   LoopPos = 05.305          seconds
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct BgmEntry {
	std::string section;   // "BGM_001"
	std::string file;      // "bgmme01" (no extension)
	std::string comment;   // UTF-8
	bool isLoop = false;
	double loopPos = 0.0;  // seconds
};

// Parses bgm.txt. Returns false if the file cannot be read.
bool ParseBgmTxt(const std::string& path, std::vector<BgmEntry>& out);

// Next `frames` sample frames starting at `pos`, wrapping to `loopStart` at
// `total` when looping (pure; used by the player and the tests). Writes the
// source frame index of every output frame to `indices` and returns the
// position after them; -1 entries mean silence past the end (no loop).
long long AdvanceLoop(long long pos, long long total, long long loopStart, bool loop,
                      int frames, std::vector<long long>& indices);

class BgmPlayer {
public:
	BgmPlayer();
	~BgmPlayer();

	// Decode an .ogg into memory. Returns false and sets *error on failure.
	bool load(const std::string& oggPath, bool loop, double loopPos, std::string* error);
	void unload();
	bool loaded() const { return !m_pcm.empty(); }

	bool play();          // opens the audio device if needed
	void stop();
	bool playing() const { return m_playing.load(); }

	void seek(double seconds);
	double position() const;          // seconds
	double length() const;            // seconds
	double loopPos() const { return m_rate ? (double)m_loopStart / m_rate : 0.0; }
	bool looping() const { return m_loop; }
	int loopsDone() const { return m_loops.load(); }
	void setVolume(float v) { m_volume = v; }

	const std::string& path() const { return m_path; }

private:
	void worker();

	std::string m_path;
	std::vector<short> m_pcm;   // interleaved
	int m_channels = 0;
	int m_rate = 0;
	long long m_totalFrames = 0;
	long long m_loopStart = 0;
	bool m_loop = false;

	std::atomic<long long> m_pos{0};
	std::atomic<bool> m_playing{false};
	std::atomic<bool> m_quit{false};
	std::atomic<int> m_loops{0};
	std::atomic<float> m_volume{0.8f};
	std::thread m_thread;
	void* m_device = nullptr;   // HWAVEOUT
};

#endif /* BGM_PLAYER_H_GUARD */
