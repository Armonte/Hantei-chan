#ifndef SHORTCUT_ROUTER_H_GUARD
#define SHORTCUT_ROUTER_H_GUARD

// Focus-aware keyboard shortcuts with one central binding table.
//
// ShortcutRegistry  - the table: action -> chord, reach and owning context.
// ShortcutRouter    - per UI frame, records which editor surface owns focus
//                     and resolves a key press to an action for that surface.
//
// Text input is never stolen: resolve() is told whether an ImGui text/scalar
// field owns the keyboard, and then only bindings explicitly marked
// `duringTextInput` (the save commands) can fire. Everything else, including
// Ctrl+Z/Ctrl+Y, stays with the field so its native edit history works.
//
// Pure standard C++; no ImGui/Win32 dependency (unit-tested standalone).

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

enum class ShortcutContext : uint8_t {
	none,
	characterView,   // HA6 editor tab (main/right/box panes + viewport)
	patEditor,       // PAT editor tab
	stageView,       // background/stage tab
	commands,        // _c.txt command workspace (owns its own undo history)
	count
};

enum class ShortcutAction : uint8_t {
	// Document history
	undo,
	redo,
	// Files / views
	save,
	saveProjectAs,
	openProject,
	newProject,
	nextView,
	closeView,
	// Navigation
	previousPattern,
	nextPattern,
	previousKeyframe,
	nextKeyframe,
	previousBox,
	nextBox,
	// Transport (J/K/L)
	playReverse,
	togglePlayback,
	playForward,
	stepTickBackward,
	stepTickForward,
	// Position tool
	cancelGesture,
	count
};

enum ShortcutModifier : uint8_t {
	shortcutNone  = 0,
	shortcutCtrl  = 1 << 0,
	shortcutShift = 1 << 1,
	shortcutAlt   = 1 << 2,
};

// Where a binding applies.
enum class ShortcutReach : uint8_t {
	focusedContext,   // only when its context owns focus
	editorViews,      // any document tab (characterView, patEditor, stageView)
	application,      // always, whatever owns focus
};

struct ShortcutChord {
	uint32_t key = 0;          // Win32 virtual-key code (letters are 'A'..'Z')
	uint8_t modifiers = shortcutNone;
	bool operator==(const ShortcutChord& o) const { return key == o.key && modifiers == o.modifiers; }
};

struct ShortcutBinding {
	ShortcutAction action;
	const char* name;
	ShortcutChord chord;
	ShortcutReach reach;
	ShortcutContext context;      // used when reach == focusedContext
	bool duringTextInput = false; // may fire while a text field owns the keyboard
	bool repeat = false;          // accepts OS auto-repeat
};

class ShortcutRegistry {
public:
	ShortcutRegistry();

	const std::vector<ShortcutBinding>& bindings() const { return m_bindings; }
	const ShortcutBinding* binding(ShortcutAction action) const;
	bool setChord(ShortcutAction action, ShortcutChord chord);

	// True if `b` is reachable from the focused context.
	static bool reaches(const ShortcutBinding& b, ShortcutContext focused);

	// Resolve a key press. Returns the first matching binding or nullptr.
	const ShortcutBinding* resolve(ShortcutChord chord, ShortcutContext focused,
	                               bool textInputActive, bool isRepeat) const;

	// Pairs of actions whose chords collide in an overlapping context.
	std::vector<std::pair<ShortcutAction, ShortcutAction>> conflicts() const;

	// "Ctrl+Shift+S" style label for menus/tooltips.
	static std::string ChordLabel(ShortcutChord chord);
	std::string label(ShortcutAction action) const;

private:
	std::vector<ShortcutBinding> m_bindings;
};

class ShortcutRouter {
public:
	using Handler = std::function<bool(ShortcutAction)>;

	// Call at the start of every UI frame; later claims in the frame win.
	void beginFrame() { m_claim = ShortcutContext::none; m_claimViewId = 0; }
	void claimFocus(ShortcutContext context, uint64_t viewId = 0) {
		m_claim = context;
		m_claimViewId = viewId;
	}
	// Call at the end of every UI frame. Key messages arrive between frames,
	// so they are routed to the owner recorded by the last complete frame.
	void endFrame() { m_focused = m_claim; m_focusedViewId = m_claimViewId; }

	ShortcutContext focused() const { return m_focused; }
	uint64_t focusedViewId() const { return m_focusedViewId; }
	bool accepts(ShortcutContext context) const { return m_focused == context; }

	// Optional per-context handler (e.g. the _c workspace routes its own
	// undo/redo). Consulted before the application's default handling.
	void setContextHandler(ShortcutContext context, Handler handler);
	bool dispatchToContext(ShortcutAction action) const;

	ShortcutRegistry& registry() { return m_registry; }
	const ShortcutRegistry& registry() const { return m_registry; }

	const ShortcutBinding* resolve(ShortcutChord chord, bool textInputActive, bool isRepeat) const {
		return m_registry.resolve(chord, m_focused, textInputActive, isRepeat);
	}

private:
	ShortcutRegistry m_registry;
	ShortcutContext m_claim = ShortcutContext::none;
	ShortcutContext m_focused = ShortcutContext::none;
	uint64_t m_claimViewId = 0;
	uint64_t m_focusedViewId = 0;
	std::vector<std::pair<ShortcutContext, Handler>> m_handlers;
};

#endif /* SHORTCUT_ROUTER_H_GUARD */
