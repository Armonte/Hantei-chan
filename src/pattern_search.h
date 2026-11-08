#ifndef PATTERN_SEARCH_H_GUARD
#define PATTERN_SEARCH_H_GUARD

#include "imsearch.h"
#include <vector>

// Flags for the pattern-name searches (MainPane). ImSearch's text
// highlighting indexes one past the end of the window's index buffer when a
// query ends in a match that is followed by whitespace (issue #81: "OD DA");
// with asserts enabled that aborted the app. The imsearch submodule is
// upstream code, so highlighting is turned off here instead of patching it;
// tools/patches/imsearch-highlight-index.patch is the upstream fix.
constexpr ImSearchFlags kPatternSearchFlags = ImSearchFlags_NoTextHighlighting;

// "Hide empty patterns" filter for the pattern lists (combo, search bar, search window, pattern list).
// With hideEmpty an unused slot (no frames, no name, no code name) is hidden; with alsoNamed too, every
// pattern with no frames is hidden (the ones the list marks with a circle). Display only: IDs and the
// ID field are unchanged, and the pattern in `keep` (the selected one) is always listed.
template<class Seq>
bool PatternHiddenByFilter(const Seq* s, bool hideEmpty, bool alsoNamed)
{
	if (!hideEmpty || !s || !s->frames.empty()) return false;
	return alsoNamed || (s->name.empty() && s->codeName.empty());
}

// The pattern IDs the lists show, in order.
template<class FD>
std::vector<int> VisiblePatterns(FD& fd, bool hideEmpty, bool alsoNamed, int keep)
{
	std::vector<int> rows;
	const int count = fd.get_sequence_count();
	rows.reserve(count);
	for (int n = 0; n < count; ++n)
		if (n == keep || !PatternHiddenByFilter(fd.get_sequence(n), hideEmpty, alsoNamed)) rows.push_back(n);
	return rows;
}

#endif /* PATTERN_SEARCH_H_GUARD */
