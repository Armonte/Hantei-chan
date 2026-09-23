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
