// Standalone tests for ShortcutRegistry / ShortcutRouter. Exit code 0 = pass.
#include "shortcut_router.h"

#include <cstdio>

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
	CHECK(reg.resolve({0x25, shortcutCtrl}, ShortcutContext::patEditor, false, false) == nullptr);
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

	if (g_failures) {
		std::fprintf(stderr, "shortcut_router_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("SHORTCUT_ROUTER_TEST_PASS\n");
	return 0;
}
