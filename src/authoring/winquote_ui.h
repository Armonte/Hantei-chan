#ifndef AUTHORING_WINQUOTE_UI_H_GUARD
#define AUTHORING_WINQUOTE_UI_H_GUARD

// "Win quotes" window: browse a game's story\battle\*_win.txt and edit the quotes (winquote_model.h).

#include "winquote_model.h"

#include <string>
#include <vector>

namespace winquote {

class Window {
public:
	bool open = false;
	// gameDir: the MBAACC folder (holding story\battle) or story\battle itself.
	bool openGame(const std::string& gameDirUtf8, std::string* error);
	void draw();
	bool dirty() const { return m_file.dirty(); }

private:
	bool selectFile(size_t i);
	std::string m_dir; // story\battle
	std::vector<std::string> m_names;
	int m_current = -1;
	File m_file;
	std::string m_status, m_filter;
	bool m_statusError = false;
};

} // namespace winquote

#endif
