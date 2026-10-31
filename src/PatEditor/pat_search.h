#ifndef PAT_SEARCH_H_GUARD
#define PAT_SEARCH_H_GUARD

// Filter box for the PAT editor's list combos (issue #113: search part sets, parts, cut-outs, shapes and textures by
// name). Only one combo popup is open at a time, so one filter serves them all.
//
//   if (ImGui::BeginCombo(...)) {
//       patsearch::FilterBox();
//       for (...) { if (!patsearch::Matches(name)) continue; ... }
//       ImGui::EndCombo();
//   }
namespace patsearch {

// Call first inside an open combo. Clears the filter and takes keyboard focus when the list opens.
void FilterBox();
// True when the filter is empty or `name` contains it (ASCII case-insensitive; other bytes compare exactly, so
// Japanese names match too).
bool Matches(const char* name);
// The pure match (header-only so tests need no ImGui).
inline bool Contains(const char* name, const char* query)
{
	auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
	if (!query || !*query) return true;
	if (!name) return false;
	for (const char* s = name; *s; ++s) {
		const char *a = s, *b = query;
		while (*a && *b && lower(*a) == lower(*b)) { ++a; ++b; }
		if (!*b) return true;
	}
	return false;
}

} // namespace patsearch

#endif /* PAT_SEARCH_H_GUARD */
