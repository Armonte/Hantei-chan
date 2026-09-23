#include "shortcut_router.h"

#include <algorithm>

namespace {
// Win32 virtual-key codes, spelled out so this file stays platform-free.
constexpr uint32_t vkTab = 0x09;
constexpr uint32_t vkEscape = 0x1B;
constexpr uint32_t vkLeft = 0x25;
constexpr uint32_t vkUp = 0x26;
constexpr uint32_t vkRight = 0x27;
constexpr uint32_t vkDown = 0x28;
constexpr uint32_t vkPrior = 0x21;     // Page Up
constexpr uint32_t vkNext = 0x22;      // Page Down
constexpr uint32_t vkMultiply = 0x6A;  // numpad *
constexpr uint32_t vkDivide = 0x6F;    // numpad /

constexpr uint8_t ctrl = shortcutCtrl;
constexpr uint8_t ctrlShift = shortcutCtrl | shortcutShift;

using R = ShortcutReach;
using C = ShortcutContext;
using A = ShortcutAction;
} // namespace

// The central table. Order matters only for resolve() ties, which conflicts()
// rejects for the defaults.
ShortcutRegistry::ShortcutRegistry()
	: m_bindings{
		// action                name                         chord                reach             context            text   repeat
		{A::undo,             "Undo",                         {'Z', ctrl},          R::application,   C::none,           false, true},
		{A::redo,             "Redo",                         {'Y', ctrl},          R::application,   C::none,           false, true},
		{A::save,             "Save",                         {'S', ctrl},          R::application,   C::none,           true,  false},
		{A::saveProjectAs,    "Save project as",              {'S', ctrlShift},     R::application,   C::none,           true,  false},
		{A::openProject,      "Open project",                 {'O', ctrl},          R::application,   C::none,           false, false},
		{A::newProject,       "New project",                  {'N', ctrl},          R::application,   C::none,           false, false},
		{A::nextView,         "Next tab",                     {vkTab, ctrl},        R::application,   C::none,           false, false},
		{A::previousView,     "Previous tab",                 {vkTab, ctrlShift},   R::application,   C::none,           false, false},
		// Alternate chords (browser-style). binding()/label()/setChord() use the first entry of an action.
		{A::nextView,         "Next tab",                     {vkNext, ctrl},       R::application,   C::none,           false, false},
		{A::previousView,     "Previous tab",                 {vkPrior, ctrl},      R::application,   C::none,           false, false},
		{A::closeView,        "Close tab",                    {'W', ctrl},          R::application,   C::none,           false, false},

		{A::previousPattern,  "Previous pattern",             {vkUp, 0},            R::editorViews,    C::none,           false, true},
		{A::nextPattern,      "Next pattern",                 {vkDown, 0},          R::editorViews,    C::none,           false, true},
		{A::previousKeyframe, "Previous keyframe",            {vkLeft, 0},          R::editorViews,    C::none,           false, true},
		{A::nextKeyframe,     "Next keyframe",                {vkRight, 0},         R::editorViews,    C::none,           false, true},
		{A::previousBox,      "Previous box",                 {'Z', 0},             R::editorViews,    C::none,           false, true},
		{A::nextBox,          "Next box",                     {'X', 0},             R::editorViews,    C::none,           false, true},

		{A::previousKeyframe, "Previous keyframe",            {vkDivide, 0},        R::editorViews,    C::none,           false, true},
		{A::nextKeyframe,     "Next keyframe",                {vkMultiply, 0},      R::editorViews,    C::none,           false, true},

		{A::nudgeLayerLeft,   "Move layer left 1px",          {vkLeft, ctrl},       R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerRight,  "Move layer right 1px",         {vkRight, ctrl},      R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerUp,     "Move layer up 1px",            {vkUp, ctrl},         R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerDown,   "Move layer down 1px",          {vkDown, ctrl},       R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerLeftFast,  "Move layer left 10px",      {vkLeft, ctrlShift},  R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerRightFast, "Move layer right 10px",     {vkRight, ctrlShift}, R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerUpFast,    "Move layer up 10px",        {vkUp, ctrlShift},    R::focusedContext, C::characterView, false, true},
		{A::nudgeLayerDownFast,  "Move layer down 10px",      {vkDown, ctrlShift},  R::focusedContext, C::characterView, false, true},
		{A::toggleSpawnPreview,  "Toggle spawned patterns",   {'P', 0},             R::focusedContext, C::characterView, false, false},

		{A::playReverse,      "Play reverse",                 {'J', 0},             R::focusedContext, C::characterView, false, false},
		{A::togglePlayback,   "Stop / play",                  {'K', 0},             R::focusedContext, C::characterView, false, false},
		{A::playForward,      "Play forward",                 {'L', 0},             R::focusedContext, C::characterView, false, false},
		{A::stepTickBackward, "Step one tick back",           {'J', shortcutShift}, R::focusedContext, C::characterView, false, true},
		{A::stepTickForward,  "Step one tick forward",        {'L', shortcutShift}, R::focusedContext, C::characterView, false, true},

		{A::cancelGesture,    "Cancel drag",                  {vkEscape, 0},        R::focusedContext, C::characterView, false, false},
	}
{
}

const ShortcutBinding* ShortcutRegistry::binding(ShortcutAction action) const
{
	auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
		[action](const ShortcutBinding& b) { return b.action == action; });
	return it == m_bindings.end() ? nullptr : &*it;
}

bool ShortcutRegistry::setChord(ShortcutAction action, ShortcutChord chord)
{
	auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
		[action](const ShortcutBinding& b) { return b.action == action; });
	if (it == m_bindings.end()) return false;
	it->chord = chord;
	return true;
}

bool ShortcutRegistry::reaches(const ShortcutBinding& b, ShortcutContext focused)
{
	switch (b.reach) {
	case ShortcutReach::application:
		return true;
	case ShortcutReach::editorViews:
		return focused == ShortcutContext::characterView || focused == ShortcutContext::patEditor ||
			focused == ShortcutContext::stageView;
	case ShortcutReach::focusedContext:
	default:
		return b.context == focused;
	}
}

const ShortcutBinding* ShortcutRegistry::resolve(ShortcutChord chord, ShortcutContext focused,
                                                 bool textInputActive, bool isRepeat) const
{
	if (chord.key == 0) return nullptr;
	for (const auto& b : m_bindings) {
		if (!(b.chord == chord)) continue;
		if (!reaches(b, focused)) continue;
		if (textInputActive && !b.duringTextInput) continue;
		if (isRepeat && !b.repeat) continue;
		return &b;
	}
	return nullptr;
}

std::vector<std::pair<ShortcutAction, ShortcutAction>> ShortcutRegistry::conflicts() const
{
	auto overlap = [](const ShortcutBinding& a, const ShortcutBinding& b) {
		for (int c = 0; c < (int)ShortcutContext::count; ++c) {
			const auto ctx = (ShortcutContext)c;
			if (reaches(a, ctx) && reaches(b, ctx)) return true;
		}
		return false;
	};
	std::vector<std::pair<ShortcutAction, ShortcutAction>> out;
	for (size_t i = 0; i < m_bindings.size(); ++i) {
		for (size_t j = i + 1; j < m_bindings.size(); ++j) {
			const auto& a = m_bindings[i];
			const auto& b = m_bindings[j];
			if (a.chord.key == 0 || !(a.chord == b.chord)) continue;
			if (overlap(a, b)) out.emplace_back(a.action, b.action);
		}
	}
	return out;
}

std::string ShortcutRegistry::ChordLabel(ShortcutChord chord)
{
	std::string s;
	if (chord.modifiers & shortcutCtrl) s += "Ctrl+";
	if (chord.modifiers & shortcutShift) s += "Shift+";
	if (chord.modifiers & shortcutAlt) s += "Alt+";
	switch (chord.key) {
	case vkTab: s += "Tab"; break;
	case vkEscape: s += "Esc"; break;
	case vkLeft: s += "Left"; break;
	case vkRight: s += "Right"; break;
	case vkUp: s += "Up"; break;
	case vkDown: s += "Down"; break;
	case vkPrior: s += "PgUp"; break;
	case vkNext: s += "PgDn"; break;
	case vkMultiply: s += "Num*"; break;
	case vkDivide: s += "Num/"; break;
	default:
		if (chord.key >= 0x20 && chord.key < 0x7F) s += (char)chord.key;
		else s += "Key" + std::to_string(chord.key);
		break;
	}
	return s;
}

std::string ShortcutRegistry::label(ShortcutAction action) const
{
	const auto* b = binding(action);
	return b ? ChordLabel(b->chord) : std::string();
}

void ShortcutRouter::setContextHandler(ShortcutContext context, Handler handler)
{
	for (auto& h : m_handlers) {
		if (h.first == context) { h.second = std::move(handler); return; }
	}
	m_handlers.emplace_back(context, std::move(handler));
}

bool ShortcutRouter::dispatchToContext(ShortcutAction action) const
{
	for (const auto& h : m_handlers) {
		if (h.first == m_focused && h.second) return h.second(action);
	}
	return false;
}
