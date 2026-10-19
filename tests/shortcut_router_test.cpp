// Standalone tests for ShortcutRegistry / ShortcutRouter. Exit code 0 = pass.
#include "shortcut_router.h"

#include <cstdio>
#include <string>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static ShortcutAction act(const ShortcutBinding* b) { return b ? b->action : ShortcutAction::count; }

int main()
{
	ShortcutRouter router;

	// Focus is latched at endFrame and cleared by a frame without claims.
	CHECK(router.focused() == ShortcutContext::none);
	router.beginFrame();
	router.claimFocus(ShortcutContext::characterView, 3);
	CHECK(router.focused() == ShortcutContext::none);   // not yet latched
	router.endFrame();
	CHECK(router.accepts(ShortcutContext::characterView) && router.focusedViewId() == 3);
	router.beginFrame();
	router.claimFocus(ShortcutContext::characterView, 3);
	router.claimFocus(ShortcutContext::commands);        // later claim wins
	router.endFrame();
	CHECK(router.accepts(ShortcutContext::commands) && router.focusedViewId() == 0);
	router.beginFrame();
	router.endFrame();
	CHECK(router.focused() == ShortcutContext::none);

	const ShortcutRegistry& reg = router.registry();

	// Defaults are conflict-free.
	CHECK(reg.conflicts().empty());

	// J/K/L only in a character view.
	CHECK(act(reg.resolve({'J', 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::playReverse);
	CHECK(act(reg.resolve({'K', 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::togglePlayback);
	CHECK(act(reg.resolve({'L', 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::playForward);
	CHECK(act(reg.resolve({'L', shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::stepTickForward);
	// Wave 2 rendering bindings: view-scoped, never while typing.
	CHECK(act(reg.resolve({'O', 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::toggleOnionSkin);
	CHECK(act(reg.resolve({'O', shortcutCtrl}, ShortcutContext::characterView, false, false)) == ShortcutAction::openProject);
	CHECK(act(reg.resolve({'P', shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::exportFramePng);
	CHECK(act(reg.resolve({'P', shortcutCtrl}, ShortcutContext::characterView, false, false)) == ShortcutAction::exportSequencePng);
	CHECK(act(reg.resolve({'P', shortcutShift}, ShortcutContext::characterView, true, false)) == ShortcutAction::count);
	CHECK(act(reg.resolve({'P', 0}, ShortcutContext::characterView, true, false)) == ShortcutAction::count);
	CHECK(act(reg.resolve({'O', 0}, ShortcutContext::stageView, false, false)) == ShortcutAction::count);
	CHECK(act(reg.resolve({'D', shortcutCtrl | shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::detachView);
	CHECK(reg.resolve({'J', 0}, ShortcutContext::commands, false, false) == nullptr);
	CHECK(reg.resolve({'J', 0}, ShortcutContext::stageView, false, false) == nullptr);

	// Navigation reaches every document tab but not the command workspace.
	CHECK(act(reg.resolve({0x28, 0}, ShortcutContext::patEditor, false, false)) == ShortcutAction::nextPattern);
	CHECK(reg.resolve({0x28, 0}, ShortcutContext::commands, false, false) == nullptr);

	// Application reach crosses contexts.
	CHECK(act(reg.resolve({'Z', shortcutCtrl}, ShortcutContext::commands, false, false)) == ShortcutAction::undo);
	CHECK(act(reg.resolve({'Y', shortcutCtrl}, ShortcutContext::none, false, false)) == ShortcutAction::redo);

	// Text input keeps its keys: no Ctrl+Z, no plain letters, but Save still works.
	CHECK(reg.resolve({'Z', shortcutCtrl}, ShortcutContext::characterView, true, false) == nullptr);
	CHECK(reg.resolve({'K', 0}, ShortcutContext::characterView, true, false) == nullptr);
	CHECK(act(reg.resolve({'S', shortcutCtrl}, ShortcutContext::characterView, true, false)) == ShortcutAction::save);

	// Auto-repeat: one-shot actions ignore it, stepping accepts it.
	CHECK(reg.resolve({'K', 0}, ShortcutContext::characterView, false, true) == nullptr);
	CHECK(act(reg.resolve({'J', shortcutShift}, ShortcutContext::characterView, false, true)) == ShortcutAction::stepTickBackward);
	CHECK(act(reg.resolve({'Z', shortcutCtrl}, ShortcutContext::characterView, false, true)) == ShortcutAction::undo);

	// Rebinding into an overlapping chord is reported.
	ShortcutRegistry custom;
	// Issue #30/#61 bindings.
	CHECK(act(reg.resolve({0x25, shortcutCtrl}, ShortcutContext::characterView, false, true)) == ShortcutAction::nudgeLayerLeft);
	CHECK(act(reg.resolve({0x28, shortcutCtrl | shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::nudgeLayerDownFast);
	// S2: Ctrl+arrows also move the selected part in a PAT tab (separate rows, same action)
	CHECK(act(reg.resolve({0x25, shortcutCtrl}, ShortcutContext::patEditor, false, false)) == ShortcutAction::nudgeLayerLeft);
	CHECK(act(reg.resolve({0x28, 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::nextPattern);
	CHECK(act(reg.resolve({'P', 0}, ShortcutContext::characterView, false, false)) == ShortcutAction::toggleSpawnPreview);
	CHECK(reg.resolve({'P', 0}, ShortcutContext::characterView, true, false) == nullptr);
	CHECK(act(reg.resolve({0x22, shortcutCtrl}, ShortcutContext::none, false, false)) == ShortcutAction::nextView);
	CHECK(act(reg.resolve({0x21, shortcutCtrl}, ShortcutContext::none, false, false)) == ShortcutAction::previousView);
	CHECK(act(reg.resolve({0x09, shortcutCtrl | shortcutShift}, ShortcutContext::none, false, false)) == ShortcutAction::previousView);
	CHECK(act(reg.resolve({0x6A, 0}, ShortcutContext::characterView, false, true)) == ShortcutAction::nextKeyframe);
	CHECK(act(reg.resolve({0x6F, 0}, ShortcutContext::characterView, false, true)) == ShortcutAction::previousKeyframe);
	CHECK(reg.label(ShortcutAction::nextView) == "Ctrl+Tab");
	CHECK(act(reg.resolve({'T', shortcutCtrl | shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::reopenClosedView);
	CHECK(ShortcutRegistry::ChordLabel({0x21, shortcutCtrl}) == "Ctrl+PgUp");
	// Remapping and persistence (issue #9).
	{
		ShortcutRegistry r;
		CHECK(r.serializeOverrides().empty());
		size_t nextTabAlt = 0; int seen = 0;
		for (size_t k = 0; k < r.bindings().size(); ++k)
			if (r.bindings()[k].action == ShortcutAction::nextView && seen++ == 1) nextTabAlt = k;
		CHECK(r.setBindingChord(nextTabAlt, {'N', shortcutAlt}));
		CHECK(!r.isDefault(nextTabAlt));
		auto lines = r.serializeOverrides();
		CHECK(lines.size() == 1 && lines[0] == "Key=nextView#1=78,4");
		ShortcutRegistry r2;
		r2.applyOverride(lines[0]);
		r2.applyOverride("Key=noSuchAction#0=65,0");
		r2.applyOverride("garbage");
		CHECK(r2.serializeOverrides() == lines);
		CHECK(act(r2.resolve({'N', shortcutAlt}, ShortcutContext::none, false, false)) == ShortcutAction::nextView);
		CHECK(r2.resolve({0x22, shortcutCtrl}, ShortcutContext::none, false, false) == nullptr);
		// Disable a binding.
		size_t undoIdx = 0;
		for (size_t k = 0; k < r2.bindings().size(); ++k) if (r2.bindings()[k].action == ShortcutAction::undo) undoIdx = k;
		r2.setBindingChord(undoIdx, {});
		CHECK(r2.resolve({'Z', shortcutCtrl}, ShortcutContext::characterView, false, false) == nullptr);
		// Conflict detection per binding.
		r2.setBindingChord(undoIdx, {'Y', shortcutCtrl});
		CHECK(!r2.conflictsOf(undoIdx).empty());
		r2.resetDefaults();
		CHECK(r2.serializeOverrides().empty());
		CHECK(ShortcutRegistry::ChordLabel({0x70, 0}) == "F1");
		CHECK(ShortcutRegistry::ChordLabel({0x61, shortcutCtrl}) == "Ctrl+Num1");
		CHECK(ShortcutRegistry::ChordLabel({}) == "(none)");
		for (int a = 0; a < (int)ShortcutAction::count; ++a)
			CHECK(ShortcutRegistry::ActionId((ShortcutAction)a)[0] != 0);
		// Wave 2 actions are remappable and persist like the rest.
		ShortcutRegistry r3;
		r3.applyOverride("Key=detachView#0=77,3");   // Ctrl+Shift+M
		CHECK(act(r3.resolve({'M', shortcutCtrl | shortcutShift}, ShortcutContext::characterView, false, false)) == ShortcutAction::detachView);
		CHECK(r3.serializeOverrides().size() == 1 && r3.serializeOverrides()[0] == "Key=detachView#0=77,3");
		r3.applyOverride("Key=toggleOnionSkin#0=0,0");  // cleared
		CHECK(r3.resolve({'O', 0}, ShortcutContext::characterView, false, false) == nullptr);
	}
	custom.setChord(ShortcutAction::togglePlayback, {'X', 0});
	CHECK(custom.conflicts().size() == 1);
	CHECK(ShortcutRegistry::ChordLabel({'S', shortcutCtrl | shortcutShift}) == "Ctrl+Shift+S");
	CHECK(custom.label(ShortcutAction::undo) == "Ctrl+Z");

	// Context handlers only see actions while their context owns focus.
	int handled = 0;
	router.setContextHandler(ShortcutContext::commands, [&](ShortcutAction a) {
		if (a == ShortcutAction::undo) { ++handled; return true; }
		return false;
	});
	router.beginFrame(); router.claimFocus(ShortcutContext::characterView); router.endFrame();
	CHECK(!router.dispatchToContext(ShortcutAction::undo) && handled == 0);
	router.beginFrame(); router.claimFocus(ShortcutContext::commands); router.endFrame();
	CHECK(router.dispatchToContext(ShortcutAction::undo) && handled == 1);
	CHECK(!router.dispatchToContext(ShortcutAction::redo));

	// ---- S2 additions (PAT editor parity, issues #9 / #61) ----
	{
		ShortcutRegistry r;
		CHECK(r.conflicts().empty());
		using C = ShortcutContext;
		ShortcutInput none;
		ShortcutInput number; number.textInput = true; number.numericInput = true;
		ShortcutInput text; text.textInput = true;
		ShortcutInput drag; drag.mouseDrag = true;
		// #61 Q / E: keyframe step in any document tab, also while typing a NUMBER, never while typing a name.
		CHECK(act(r.resolve({'Q', 0}, C::characterView, none)) == ShortcutAction::previousKeyframe);
		CHECK(act(r.resolve({'E', 0}, C::characterView, none)) == ShortcutAction::nextKeyframe);
		CHECK(act(r.resolve({'E', 0}, C::patEditor, number)) == ShortcutAction::nextKeyframe);
		CHECK(r.resolve({'E', 0}, C::characterView, text) == nullptr);
		CHECK(r.resolve({'Q', 0}, C::commands, none) == nullptr);
		// Num* / Num/ keep firing in any field (they were duringTextInput before).
		CHECK(act(r.resolve({0x6A, 0}, C::characterView, text)) == ShortcutAction::nextKeyframe);
		// Old two-flag resolve() never treats a field as numeric.
		CHECK(r.resolve({'E', 0}, C::characterView, true, false) == nullptr);
		// PAT tab: arrows navigate (part set / part), Ctrl+arrows move the part, Esc cancels a UV drag.
		CHECK(act(r.resolve({0x26, 0}, C::patEditor, none)) == ShortcutAction::previousPattern);
		CHECK(act(r.resolve({0x27, 0}, C::patEditor, none)) == ShortcutAction::nextKeyframe);
		CHECK(act(r.resolve({0x25, shortcutCtrl}, C::patEditor, none)) == ShortcutAction::nudgeLayerLeft);
		CHECK(act(r.resolve({0x28, shortcutCtrl | shortcutShift}, C::patEditor, none)) == ShortcutAction::nudgeLayerDownFast);
		CHECK(act(r.resolve({0x1B, 0}, C::patEditor, none)) == ShortcutAction::cancelGesture);
		CHECK(act(r.resolve({'P', shortcutShift}, C::patEditor, none)) == ShortcutAction::exportFramePng);
		// F / L / H: L is play forward in a character tab, the axis toggle in a PAT tab (G in a character tab).
		CHECK(act(r.resolve({'F', 0}, C::characterView, none)) == ShortcutAction::toggleHideUi);
		CHECK(act(r.resolve({'F', 0}, C::stageView, none)) == ShortcutAction::toggleHideUi);
		CHECK(act(r.resolve({'L', 0}, C::characterView, none)) == ShortcutAction::playForward);
		CHECK(act(r.resolve({'L', 0}, C::patEditor, none)) == ShortcutAction::toggleAxis);
		CHECK(act(r.resolve({'G', 0}, C::characterView, none)) == ShortcutAction::toggleAxis);
		CHECK(act(r.resolve({'H', 0}, C::characterView, none)) == ShortcutAction::toggleBoxes);
		CHECK(act(r.resolve({'H', 0}, C::patEditor, none)) == ShortcutAction::togglePartHighlight);
		CHECK(act(r.resolve({'T', 0}, C::patEditor, none)) == ShortcutAction::toggleTextureView);
		CHECK(act(r.resolve({'U', 0}, C::patEditor, none)) == ShortcutAction::toggleUvView);
		CHECK(r.resolve({'T', 0}, C::characterView, none) == nullptr);
		// Shortcuts wait while a mouse drag runs; only Esc (cancel the drag) gets through.
		CHECK(r.resolve({0x28, 0}, C::characterView, drag) == nullptr);
		CHECK(r.resolve({'Z', shortcutCtrl}, C::patEditor, drag) == nullptr);
		CHECK(r.resolve({'S', shortcutCtrl}, C::characterView, drag) == nullptr);
		CHECK(act(r.resolve({0x1B, 0}, C::characterView, drag)) == ShortcutAction::cancelGesture);
		ShortcutRouter rr;
		rr.beginFrame(); rr.claimFocus(C::characterView); rr.endFrame();
		CHECK(act(rr.resolve({0x28, 0}, false, false)) == ShortcutAction::nextPattern);
		rr.setMouseDrag(true);
		CHECK(rr.resolve({0x28, 0}, false, false) == nullptr);
		CHECK(rr.resolve({0x28, 0}, rr.input(false, false, false)) == nullptr);
		rr.setMouseDrag(false);
		CHECK(act(rr.resolve({'E', 0}, rr.input(true, true, false))) == ShortcutAction::nextKeyframe);
		CHECK(rr.resolve({'E', 0}, rr.input(true, false, false)) == nullptr);
		// Added rows come after the original table, so an existing override keeps its row: Key=nudgeLayerLeft#0
		// is still the character-tab row, #1 is the new PAT-tab row.
		ShortcutRegistry r4;
		r4.applyOverride("Key=nudgeLayerLeft#0=74,1");   // Ctrl+J
		CHECK(act(r4.resolve({'J', shortcutCtrl}, C::characterView, none)) == ShortcutAction::nudgeLayerLeft);
		CHECK(act(r4.resolve({0x25, shortcutCtrl}, C::patEditor, none)) == ShortcutAction::nudgeLayerLeft);
		r4.applyOverride("Key=nextKeyframe#2=82,0");     // the E row -> R
		CHECK(act(r4.resolve({'R', 0}, C::patEditor, none)) == ShortcutAction::nextKeyframe);
		CHECK(r4.resolve({'E', 0}, C::patEditor, none) == nullptr);
		CHECK(ShortcutRegistry::ActionId(ShortcutAction::togglePartHighlight) == std::string("togglePartHighlight"));
	}

	{
		ShortcutRegistry grab;
		using C=ShortcutContext;
		ShortcutInput none;
		CHECK(act(grab.resolve({0xBC,shortcutAlt},C::characterView,none))==ShortcutAction::previousOpponentPreview);
		CHECK(act(grab.resolve({0xBE,shortcutAlt},C::characterView,none))==ShortcutAction::nextOpponentPreview);
		CHECK(grab.resolve({0xBE,shortcutAlt},C::commands,none)==nullptr);
		ShortcutInput text; text.textInput=true;
		CHECK(grab.resolve({0xBE,shortcutAlt},C::characterView,text)==nullptr);
		grab.applyOverride("Key=nextOpponentPreview#0=66,1");
		CHECK(act(grab.resolve({'B',shortcutCtrl},C::characterView,none))==ShortcutAction::nextOpponentPreview);
	}
	{
		ShortcutRegistry r; ShortcutInput none,text,drag; text.textInput=true; drag.mouseDrag=true;
		CHECK(act(r.resolve({0xDD,shortcutAlt},ShortcutContext::characterView,none))==ShortcutAction::nextPositionTarget);
		CHECK(r.resolve({0xDD,shortcutAlt},ShortcutContext::commands,none)==nullptr);
		CHECK(r.resolve({0xDD,shortcutAlt},ShortcutContext::characterView,text)==nullptr);
		CHECK(r.resolve({0xDD,shortcutAlt},ShortcutContext::characterView,drag)==nullptr);
		r.applyOverride("Key=nextPositionTarget#0=66,1");
		CHECK(act(r.resolve({'S',shortcutAlt},ShortcutContext::characterView,none))==ShortcutAction::togglePositionSolo);
		CHECK(r.resolve({'S',shortcutAlt},ShortcutContext::characterView,text)==nullptr);
		CHECK(act(r.resolve({'B',shortcutCtrl},ShortcutContext::characterView,none))==ShortcutAction::nextPositionTarget);
	}
	{
		// Opt-in layouts: J/L step + PgUp/PgDn states. Off by default (0.1.2), user overrides survive the switch,
		// and PgUp/PgDn never fire while a text field owns the keyboard.
		ShortcutRegistry r; ShortcutInput none,text; text.textInput=true;
		using C=ShortcutContext;
		CHECK(r.resolve({0x21,0},C::characterView,none)==nullptr);
		r.applyOverride("Key=playForward#0=80,3"); // Ctrl+Shift+P
		r.applyLayoutOptions(true,true);
		CHECK(act(r.resolve({'J',0},C::characterView,none))==ShortcutAction::stepTickBackward);
		CHECK(act(r.resolve({'L',0},C::characterView,none))==ShortcutAction::stepTickForward);
		CHECK(act(r.resolve({'J',shortcutShift},C::characterView,none))==ShortcutAction::playReverse);
		CHECK(act(r.resolve({'P',(uint8_t)(shortcutCtrl|shortcutShift)},C::characterView,none))==ShortcutAction::playForward);
		CHECK(act(r.resolve({0x21,0},C::characterView,none))==ShortcutAction::nextKeyframe);
		CHECK(r.resolve({0x21,0},C::characterView,text)==nullptr);
		CHECK(act(r.resolve({0x21,0},C::stageView,none))==ShortcutAction::previousStage);
		CHECK(r.conflicts().empty());
		r.applyLayoutOptions(false,false);
		CHECK(act(r.resolve({'J',0},C::characterView,none))==ShortcutAction::playReverse);
		CHECK(r.resolve({0x21,0},C::characterView,none)==nullptr);
	}
	if (g_failures) {
		std::fprintf(stderr, "shortcut_router_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("SHORTCUT_ROUTER_TEST_PASS\n");
	return 0;
}
