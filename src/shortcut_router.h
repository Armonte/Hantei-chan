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
	authoring,       // [authoring] the Authoring workspace (its tuning history: Ctrl+Z / Ctrl+Y)
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
	previousView,
	closeView,
	reopenClosedView,
	// Navigation
	previousPattern,
	nextPattern,
	previousKeyframe,
	nextKeyframe,
	previousBox,
	nextBox,
	// Layer offset nudge (issue #30)
	nudgeLayerLeft,
	nudgeLayerRight,
	nudgeLayerUp,
	nudgeLayerDown,
	nudgeLayerLeftFast,
	nudgeLayerRightFast,
	nudgeLayerUpFast,
	nudgeLayerDownFast,
	// View toggles (issue #61)
	toggleSpawnPreview,
	// Transport (J/K/L)
	playReverse,
	togglePlayback,
	playForward,
	stepTickBackward,
	stepTickForward,
	// Position tool
	cancelGesture,
	// Rendering (wave 2): onion skin, PNG export, detached windows
	toggleOnionSkin,
	exportFramePng,
	exportSequencePng,
	detachView,
	// Stage tab: previous / next stage of the game's stage list
	previousStage,
	nextStage,
	// View toggles (sosfiro parity): F hides the editor panels, L / G the axis lines, H the hitboxes
	// (character tab) or the selected-part highlight (PAT tab)
	toggleHideUi,
	toggleAxis,
	toggleBoxes,
	togglePartHighlight,
	// PAT editor render modes (sosfiro T / U)
	toggleTextureView,
	toggleUvView,
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
	bool duringNumericInput = false; // may fire while a NUMERIC field is being typed in (issue #61 Q / E), never in a text field
	bool duringMouseDrag = false;    // may fire while a mouse drag runs in the view (Esc); everything else waits for the drop
};

// What owns the keyboard / mouse when a key arrives.
struct ShortcutInput {
	bool textInput = false;      // an ImGui text or scalar field is being typed in
	bool numericInput = false;   // ... and it is a numeric (scalar) field
	bool repeat = false;         // OS auto-repeat
	bool mouseDrag = false;      // a pan / box / UV drag is in progress in a view
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
	const ShortcutBinding* resolve(ShortcutChord chord, ShortcutContext focused, const ShortcutInput& input) const;

	// Pairs of actions whose chords collide in an overlapping context.
	std::vector<std::pair<ShortcutAction, ShortcutAction>> conflicts() const;

	// "Ctrl+Shift+S" style label for menus/tooltips.
	static std::string ChordLabel(ShortcutChord chord);
	std::string label(ShortcutAction action) const;

	// ---- Remapping (issue #9) ----
	// Set the chord of binding `index` (an action can have several). key 0
	// disables the binding.
	bool setBindingChord(size_t index, ShortcutChord chord);
	void resetDefaults();
	bool isDefault(size_t index) const;
	// Stable id of an action for settings files ("undo", "nextPattern", ...).
	static const char* ActionId(ShortcutAction action);
	// Settings lines "Key=<actionId>#<n>=<vk>,<mods>" for every binding that
	// differs from the default, and the reverse. Unknown ids are ignored.
	std::vector<std::string> serializeOverrides() const;
	void applyOverride(const std::string& line);
	// Bindings of `index` that share its chord in an overlapping context.
	std::vector<size_t> conflictsOf(size_t index) const;

private:
	std::vector<ShortcutBinding> m_bindings;
	std::vector<ShortcutChord> m_defaults;
	int slotOf(size_t index) const;
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
		ShortcutInput in;
		in.textInput = textInputActive;
		in.repeat = isRepeat;
		in.mouseDrag = m_mouseDrag;
		return m_registry.resolve(chord, m_focused, in);
	}
	ShortcutInput input(bool textInputActive, bool numericInput, bool isRepeat) const {
		ShortcutInput in;
		in.textInput = textInputActive;
		in.numericInput = textInputActive && numericInput;
		in.repeat = isRepeat;
		in.mouseDrag = m_mouseDrag;
		return in;
	}
	const ShortcutBinding* resolve(ShortcutChord chord, const ShortcutInput& in) const {
		return m_registry.resolve(chord, m_focused, in);
	}
	// A mouse drag in a view (pan, box / UV rectangle) blocks shortcuts until the button is released (sosfiro
	// main.cpp `!dragActive`): a key pressed mid-drag must not switch the pattern or tab the drag is editing.
	void setMouseDrag(bool active) { m_mouseDrag = active; }
	bool mouseDrag() const { return m_mouseDrag; }

private:
	ShortcutRegistry m_registry;
	ShortcutContext m_claim = ShortcutContext::none;
	ShortcutContext m_focused = ShortcutContext::none;
	uint64_t m_claimViewId = 0;
	uint64_t m_focusedViewId = 0;
	bool m_mouseDrag = false;
	std::vector<std::pair<ShortcutContext, Handler>> m_handlers;
};

#endif /* SHORTCUT_ROUTER_H_GUARD */
