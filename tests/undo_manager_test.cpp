// Standalone tests for UndoManager (document-level transactions).
// Build target: undo_manager_test (see CMakeLists.txt). Exit code 0 = pass.
#include "undo_manager.h"

#include <chrono>
#include <cstdio>
#include <cstring>

std::set<int> numberSet;
int maxCount;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static std::vector<Sequence> makeDoc(int patterns, int framesEach)
{
	std::vector<Sequence> doc(patterns);
	for (int p = 0; p < patterns; ++p) {
		doc[p].name = "pattern " + std::to_string(p);
		doc[p].initialized = framesEach > 0;
		doc[p].frames.resize(framesEach);
		for (int f = 0; f < framesEach; ++f) {
			Frame& fr = doc[p].frames[f];
			fr.AF.layers.resize(1);
			fr.AF.layers[0].spriteId = p * 100 + f;
			fr.AF.duration = 3;
			fr.EF.resize(1);
			std::memset(&fr.EF[0], 0, sizeof(Frame_EF));
			fr.IF.resize(1);
			std::memset(&fr.IF[0], 0, sizeof(Frame_IF));
			fr.hitboxes[1] = Hitbox{{-10, -20, 10, 0}};
		}
	}
	return doc;
}

// One UI frame: optional edit, then endFrame with the gesture flag.
template<typename F>
static void uiFrame(UndoManager& u, bool gesture, F&& edit, bool notify = true)
{
	edit();
	if (notify) u.markModified();
	u.endFrame(gesture);
}
static void idleFrame(UndoManager& u, bool gesture = false) { u.endFrame(gesture); }

static void testDragCoalescing()
{
	auto doc = makeDoc(10, 3);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	for (int v = 1; v <= 5; ++v)
		uiFrame(u, true, [&] { doc[2].frames[1].EF[0].parameters[0] = v; });
	CHECK(u.undoCount() == 0);          // nothing committed while held
	idleFrame(u, false);                // release
	CHECK(u.undoCount() == 1);
	const auto* e = u.undo();
	CHECK(e && e->changes.size() == 1 && e->changes[0].index == 2);
	CHECK(doc[2].frames[1].EF[0].parameters[0] == 0);
	CHECK(!u.canUndo());
	e = u.redo();
	CHECK(e && doc[2].frames[1].EF[0].parameters[0] == 5);
}

static void testUnmarkedEditCaughtOnGestureEnd()
{
	auto doc = makeDoc(4, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	// A widget changed its value but never called markModified().
	uiFrame(u, true, [&] { doc[1].frames[0].AF.duration = 99; }, false);
	idleFrame(u, false);
	CHECK(u.undoCount() == 1);
	u.undo();
	CHECK(doc[1].frames[0].AF.duration == 3);
}

static void testImmediateUndoMidGesture()
{
	auto doc = makeDoc(4, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	uiFrame(u, true, [&] { doc[0].frames[0].AS.speed[0] = 7; });
	// Ctrl+Z before the release frame was drawn.
	const auto* e = u.undo();
	CHECK(e != nullptr);
	CHECK(doc[0].frames[0].AS.speed[0] == 0);
	CHECK(u.undoCount() == 0 && u.redoCount() == 1);
}

static void testCrossPatternAndMultiPatternSteps()
{
	auto doc = makeDoc(20, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	u.noteFocus(3, 1);
	// Paste into several patterns that are not the visible one (Pop all and paste).
	uiFrame(u, false, [&] {
		doc[7] = doc[1];
		doc[8] = doc[1];
		doc[15].name = "renamed";
	});
	CHECK(u.undoCount() == 1);
	const auto* top = u.peekUndo();
	CHECK(top && top->changes.size() == 3);
	CHECK(top && top->focusPattern == 7);   // focus moved to a touched pattern
	u.undo();
	CHECK(doc[7].name == "pattern 7" && doc[8].name == "pattern 8" && doc[15].name == "pattern 15");
	u.redo();
	CHECK(doc[7].name == "pattern 1" && doc[15].name == "renamed");
}

static void testFrameInsertDeleteAndBoxes()
{
	auto doc = makeDoc(5, 3);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	uiFrame(u, false, [&] { doc[4].frames.insert(doc[4].frames.begin() + 1, Frame{}); });
	uiFrame(u, false, [&] { doc[4].frames.erase(doc[4].frames.begin()); });
	uiFrame(u, false, [&] { doc[4].frames[0].hitboxes[25] = Hitbox{{1, 2, 3, 4}}; });
	CHECK(u.undoCount() == 3);
	u.undo();
	CHECK(doc[4].frames[0].hitboxes.count(25) == 0);
	u.undo();
	CHECK(doc[4].frames.size() == 4);
	u.undo();
	CHECK(doc[4].frames.size() == 3 && doc[4].frames[1].AF.layers[0].spriteId == 401);
}

static void testTransactionsAndCancel()
{
	auto doc = makeDoc(5, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	// Viewport box drag: explicit transaction spanning idle UI frames.
	u.beginTransaction("Draw box");
	for (int i = 0; i < 4; ++i)
		uiFrame(u, false, [&] { doc[0].frames[0].hitboxes[2] = Hitbox{{0, 0, i, i}}; });
	CHECK(u.undoCount() == 0);
	CHECK(u.commitTransaction());
	CHECK(u.undoCount() == 1 && u.peekUndo()->label == "Draw box");

	// Escape: cancel restores the pre-gesture state and records nothing.
	u.beginTransaction("Move");
	doc[1].frames[1].AF.layers[0].offset_x = 42;
	doc[3].frames[0].EF[0].parameters[1] = 5;
	u.markModified();
	u.endFrame(false);
	u.cancelTransaction();
	CHECK(doc[1].frames[1].AF.layers[0].offset_x == 0);
	CHECK(doc[3].frames[0].EF[0].parameters[1] == 0);
	CHECK(u.undoCount() == 1);

	// Nested transactions commit once.
	u.beginTransaction("outer");
	u.beginTransaction("inner");
	doc[2].level = 9;
	u.markModified();
	CHECK(!u.commitTransaction());
	doc[2].flag = 4;
	CHECK(u.commitTransaction());
	CHECK(u.undoCount() == 2);
	u.undo();
	CHECK(doc[2].level == 0 && doc[2].flag == 0);
}

static void testRedoBranchAndDirtyState()
{
	auto doc = makeDoc(5, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	CHECK(u.isClean());
	uiFrame(u, false, [&] { doc[0].psts = 1; });
	uiFrame(u, false, [&] { doc[0].psts = 2; });
	CHECK(!u.isClean());
	u.markClean();                 // saved at psts=2
	CHECK(u.isClean());
	uiFrame(u, false, [&] { doc[0].psts = 3; });
	CHECK(!u.isClean());
	u.undo();                      // back to saved state
	CHECK(u.isClean());
	u.undo();                      // before saved state
	CHECK(!u.isClean());
	u.redo();
	CHECK(u.isClean());
	u.redo();
	CHECK(!u.isClean() && doc[0].psts == 3);
	u.undo();
	CHECK(u.isClean());
	// Branch: a new edit after undo discards redo; saved state stays reachable.
	uiFrame(u, false, [&] { doc[0].psts = 7; });
	CHECK(!u.canRedo());
	CHECK(!u.isClean());
	u.undo();
	CHECK(u.isClean() && doc[0].psts == 2);
	// Undo back to load state and Sequence::modified is recomputed.
	doc[0].modified = true;
	u.undo();
	u.undo();
	CHECK(doc[0].psts == 0 && !doc[0].modified);
	u.redo();
	CHECK(doc[0].modified);
}

static void testTrimMakesSavedUnreachable()
{
	auto doc = makeDoc(2, 1);
	UndoManager u;
	u.attach(&doc);
	u.setMaxLevels(3);
	u.ensureBaseline();
	u.markClean();
	for (int i = 1; i <= 5; ++i)
		uiFrame(u, false, [&] { doc[0].level = i; });
	CHECK(u.undoCount() == 3);
	while (u.undo()) {}
	CHECK(doc[0].level == 2);   // oldest reachable state
	CHECK(!u.isClean());        // load state was trimmed away
}

static void testNoOpEditIsNotRecorded()
{
	auto doc = makeDoc(3, 1);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	uiFrame(u, false, [&] { doc[1].level = doc[1].level; doc[1].modified = true; });
	CHECK(u.undoCount() == 0);
	CHECK(u.isClean());
}

static void testDisplayNormalisationIsNotAnEdit()
{
	auto doc = makeDoc(3, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	// Insert a blank keyframe (no layers), committed as one step.
	uiFrame(u, false, [&] { doc[1].frames.insert(doc[1].frames.begin(), Frame{}); });
	CHECK(u.undoCount() == 1);
	// The renderer then gives it a default layer just by drawing it.
	doc[1].frames[0].AF.layers.push_back({});
	idleFrame(u, true);
	idleFrame(u, false);          // gesture edge: must not record anything
	CHECK(u.undoCount() == 1);
	u.undo();
	CHECK(doc[1].frames.size() == 2);
}

static void testUnmarkedDriftDoesNotBlockUndo()
{
	auto doc = makeDoc(3, 2);
	UndoManager u;
	u.attach(&doc);
	u.ensureBaseline();
	uiFrame(u, false, [&] { doc[0].level = 5; });
	// Something rewrites pattern 2 without notifying and without a gesture.
	doc[2].frames[0].AF.duration = 77;
	const auto* e = u.undo();       // must undo the real step, not the drift
	CHECK(e && e->changes.size() == 1 && e->changes[0].index == 0);
	CHECK(doc[0].level == 0 && doc[2].frames[0].AF.duration == 77);
	CHECK(!u.canUndo());
}

static void testMemorySharingAndSpeed()
{
	// Roughly a large MBAACC character: 1000 slots, ~700 used, 12 frames each.
	auto doc = makeDoc(1000, 0);
	for (int p = 0; p < 700; ++p) {
		auto one = makeDoc(1, 12);
		doc[p] = one[0];
	}
	UndoManager u;
	u.attach(&doc);
	auto t0 = std::chrono::steady_clock::now();
	u.ensureBaseline();
	auto t1 = std::chrono::steady_clock::now();
	for (int i = 0; i < 100; ++i)
		uiFrame(u, false, [&] { doc[i % 5].frames[0].AF.duration = 10 + i; });
	auto t2 = std::chrono::steady_clock::now();
	CHECK(u.undoCount() == 100);
	const size_t perPattern = UndoManager::ApproxSequenceBytes(doc[0]);
	// 100 single-pattern steps hold about 100 extra pattern copies, not 100 documents.
	CHECK(u.historyBytes() <= perPattern * 110);
	const double baselineMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
	const double commitMs = std::chrono::duration<double, std::milli>(t2 - t1).count() / 100.0;
	std::printf("  baseline capture %.2f ms, full-document diff+commit %.3f ms/step, history %zu KiB\n",
		baselineMs, commitMs, u.historyBytes() / 1024);
	CHECK(commitMs < 16.0);
}

int main()
{
	testDragCoalescing();
	testUnmarkedEditCaughtOnGestureEnd();
	testImmediateUndoMidGesture();
	testCrossPatternAndMultiPatternSteps();
	testFrameInsertDeleteAndBoxes();
	testTransactionsAndCancel();
	testRedoBranchAndDirtyState();
	testTrimMakesSavedUnreachable();
	testNoOpEditIsNotRecorded();
	testDisplayNormalisationIsNotAnEdit();
	testUnmarkedDriftDoesNotBlockUndo();
	testMemorySharingAndSpeed();
	if (g_failures) {
		std::fprintf(stderr, "undo_manager_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("UNDO_MANAGER_TEST_PASS\n");
	return 0;
}
