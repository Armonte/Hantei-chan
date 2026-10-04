#ifndef TAG_WIDGETS_H_GUARD
#define TAG_WIDGETS_H_GUARD
// ImGui widgets for the TAG tuning levers, shared by the Tag / Team panel (tag_panel.cpp) and the Authoring workspace
// (authoring/authoring_window.cpp): the value widget (clamped to the lever's range), the tooltip with the guide's notes,
// and small classifiers. Must be called on the UI thread inside an ImGui frame.
#include "tag_levers.h"

#include <string>

namespace tagui {

std::string RangeText(const tagtune::Lever& l);
// Extra notes from TAG_TUNING_GUIDE.md for the levers whose one-line help is not enough (nullptr = none).
const char* GuideNote(const char* key);
// The hover tooltip for the last item: key, group, help, guide note, default / range, `where: styleValue`.
void LeverTooltip(const tagtune::Lever& l, int32_t styleValue, const char* where);
// One value widget. Returns true when v changed (always clamped into range).
bool ValueWidget(const tagtune::Lever& l, int32_t& v, float width);
bool IsSlotAction(const tagtune::Lever& l);
bool IsSlotEntry(const tagtune::Lever& l);

} // namespace tagui

#endif
