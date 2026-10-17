#ifndef AUTHORING_WINQUOTE_MODEL_H_GUARD
#define AUTHORING_WINQUOTE_MODEL_H_GUARD

// MBAACC win quotes: story\battle\<chara>[Ex]_win.txt (CP932, CRLF). The game reads these on every result screen
// (ResultMenu_LoadWinQuoteData) and writes data\<chara>.wmt from them, so a win-quote mod ships the _win.txt, never
// the .wmt. One quote per line until END:
//   <opponent> <mode> <face>\t<voice>.adx <text>
// opponent: charaselect chara id, 255 = anyone; mode: 0 = normal, 1 = arcade fixed-CPU win; face: portrait index;
// text: "■" (CP932 81 A1) is a line break. Lines that are not quotes (comments, blanks, END and after) are kept as is,
// and so is every quote line that is not edited.

#include <cstddef>
#include <string>
#include <vector>

namespace winquote {

struct Quote {
	int opponent = 255, mode = 0, face = 0;
	std::string voice; // "Arc_W0AA04.adx"
	std::string text;  // CP932, "■" breaks
	size_t line = 0;   // index into the file's lines
};

class File {
public:
	bool load(const std::string& path);
	bool loadText(const std::string& raw);
	bool save(const std::string& path); // atomic
	const std::string& text() const { return m_text; }
	bool dirty() const { return m_text != m_saved; }

	const std::vector<Quote>& quotes() const { return m_quotes; }
	// Rewrites quote i's line ("%3d %2d %2d\t<voice> <text>", the vanilla layout).
	void set(size_t i, const Quote& q);
	// Inserts a quote after quote `after` (or before END / at the end when after is out of range).
	void add(size_t after, const Quote& q);
	void remove(size_t i);

	// Longest line of a quote in display columns (a CP932 lead byte = 2, ASCII = 1). The vanilla files mark
	// 27 full-width characters (54 columns) as the limit.
	static int widestLine(const std::string& cp932Text);
	static constexpr int kMaxColumns = 54;

private:
	void split();
	void join();
	void parse();
	std::string m_text, m_saved, m_eol = "\r\n";
	std::vector<std::string> m_lines;
	bool m_trailingEol = true;
	std::vector<Quote> m_quotes;
};

} // namespace winquote

#endif
