#ifndef PATTERN_SEARCH_H_GUARD
#define PATTERN_SEARCH_H_GUARD

#include "imsearch.h"

// Flags for the pattern-name searches (MainPane). ImSearch's text
// highlighting indexes one past the end of the window's index buffer when a
// query ends in a match that is followed by whitespace (issue #81: "OD DA");
// with asserts enabled that aborted the app. The imsearch submodule is
// upstream code, so highlighting is turned off here instead of patching it;
// tools/patches/imsearch-highlight-index.patch is the upstream fix.
constexpr ImSearchFlags kPatternSearchFlags = ImSearchFlags_NoTextHighlighting;

#endif /* PATTERN_SEARCH_H_GUARD */
