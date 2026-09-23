#include "shortcut_router.h"

#include <algorithm>
#include <cstdio>

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
	for (const auto& b : m_bindings) m_defaults.push_back(b.chord);
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
	if (chord.key == 0) return "(none)";
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
	case 0x20: s += "Space"; break;
	case 0x08: s += "Backspace"; break;
	case 0x0D: s += "Enter"; break;
	case 0x2D: s += "Insert"; break;
	case 0x2E: s += "Delete"; break;
	case 0x24: s += "Home"; break;
	case 0x23: s += "End"; break;
	case 0x6B: s += "Num+"; break;
	case 0x6D: s += "Num-"; break;
	case 0x6E: s += "Num."; break;
	case 0xBA: s += ";"; break;
	case 0xBB: s += "="; break;
	case 0xBC: s += ","; break;
	case 0xBD: s += "-"; break;
	case 0xBE: s += "."; break;
	case 0xBF: s += "/"; break;
	case 0xC0: s += "`"; break;
	case 0xDB: s += "["; break;
	case 0xDC: s += "\\"; break;
	case 0xDD: s += "]"; break;
	case 0xDE: s += "'"; break;
	default:
		if (chord.key >= 0x60 && chord.key <= 0x69) s += "Num" + std::to_string(chord.key - 0x60);
		else if (chord.key >= 0x70 && chord.key <= 0x87) s += "F" + std::to_string(chord.key - 0x6F);
		else if (chord.key >= 0x21 && chord.key < 0x7F) s += (char)chord.key;
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

// ---------------------------------------------------------------------------
// Remapping
// ---------------------------------------------------------------------------

const char* ShortcutRegistry::ActionId(ShortcutAction action)
{
	switch (action) {
	case A::undo: return "undo";
	case A::redo: return "redo";
	case A::save: return "save";
	case A::saveProjectAs: return "saveProjectAs";
	case A::openProject: return "openProject";
	case A::newProject: return "newProject";
	case A::nextView: return "nextView";
	case A::previousView: return "previousView";
	case A::closeView: return "closeView";
	case A::previousPattern: return "previousPattern";
	case A::nextPattern: return "nextPattern";
	case A::previousKeyframe: return "previousKeyframe";
	case A::nextKeyframe: return "nextKeyframe";
	case A::previousBox: return "previousBox";
	case A::nextBox: return "nextBox";
	case A::nudgeLayerLeft: return "nudgeLayerLeft";
	case A::nudgeLayerRight: return "nudgeLayerRight";
	case A::nudgeLayerUp: return "nudgeLayerUp";
	case A::nudgeLayerDown: return "nudgeLayerDown";
	case A::nudgeLayerLeftFast: return "nudgeLayerLeftFast";
	case A::nudgeLayerRightFast: return "nudgeLayerRightFast";
	case A::nudgeLayerUpFast: return "nudgeLayerUpFast";
	case A::nudgeLayerDownFast: return "nudgeLayerDownFast";
	case A::toggleSpawnPreview: return "toggleSpawnPreview";
	case A::playReverse: return "playReverse";
	case A::togglePlayback: return "togglePlayback";
	case A::playForward: return "playForward";
	case A::stepTickBackward: return "stepTickBackward";
	case A::stepTickForward: return "stepTickForward";
	case A::cancelGesture: return "cancelGesture";
	default: return "";
	}
}

int ShortcutRegistry::slotOf(size_t index) const
{
	int slot = 0;
	for (size_t i = 0; i < index && i < m_bindings.size(); ++i)
		if (m_bindings[i].action == m_bindings[index].action) ++slot;
	return slot;
}

bool ShortcutRegistry::setBindingChord(size_t index, ShortcutChord chord)
{
	if (index >= m_bindings.size()) return false;
	m_bindings[index].chord = chord;
	return true;
}

void ShortcutRegistry::resetDefaults()
{
	for (size_t i = 0; i < m_bindings.size() && i < m_defaults.size(); ++i)
		m_bindings[i].chord = m_defaults[i];
}

bool ShortcutRegistry::isDefault(size_t index) const
{
	return index < m_defaults.size() && m_bindings[index].chord == m_defaults[index];
}

std::vector<std::string> ShortcutRegistry::serializeOverrides() const
{
	std::vector<std::string> out;
	for (size_t i = 0; i < m_bindings.size(); ++i) {
		if (isDefault(i)) continue;
		out.push_back(std::string("Key=") + ActionId(m_bindings[i].action) + "#" + std::to_string(slotOf(i)) + "=" +
			std::to_string(m_bindings[i].chord.key) + "," + std::to_string(m_bindings[i].chord.modifiers));
	}
	return out;
}

void ShortcutRegistry::applyOverride(const std::string& line)
{
	// Key=<id>#<slot>=<vk>,<mods>
	if (line.compare(0, 4, "Key=") != 0) return;
	const size_t hash = line.find('#', 4), eq = line.find('=', 4);
	if (hash == std::string::npos || eq == std::string::npos || eq < hash) return;
	const std::string id = line.substr(4, hash - 4);
	int slot = 0;
	unsigned vk = 0, mods = 0;
	if (std::sscanf(line.c_str() + hash + 1, "%d=%u,%u", &slot, &vk, &mods) != 3) return;
	int seen = 0;
	for (size_t i = 0; i < m_bindings.size(); ++i) {
		if (id != ActionId(m_bindings[i].action)) continue;
		if (seen++ == slot) {
			m_bindings[i].chord = ShortcutChord{vk, (uint8_t)(mods & 7)};
			return;
		}
	}
}

std::vector<size_t> ShortcutRegistry::conflictsOf(size_t index) const
{
	std::vector<size_t> out;
	if (index >= m_bindings.size() || m_bindings[index].chord.key == 0) return out;
	const auto& a = m_bindings[index];
	for (size_t j = 0; j < m_bindings.size(); ++j) {
		if (j == index) continue;
		const auto& b = m_bindings[j];
		if (!(a.chord == b.chord)) continue;
		for (int c = 0; c < (int)ShortcutContext::count; ++c) {
			const auto ctx = (ShortcutContext)c;
			if (reaches(a, ctx) && reaches(b, ctx)) { out.push_back(j); break; }
		}
	}
	return out;
}
