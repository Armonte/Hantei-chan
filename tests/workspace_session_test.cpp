// WorkspaceSession: tab ownership across the main window and detached
// windows. Cases from Gonptechan EX's workspace_session_test (drop 1abc27f9)
// plus the moves the detached-window UI performs.
#include "workspace_session.h"

#include <cstdio>
#include <vector>

static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)

using Tabs = std::vector<WorkspaceSession::Id>;

int main()
{
	WorkspaceSession session;
	CHECK(session.add(10)); CHECK(session.add(20)); CHECK(session.add(30));
	CHECK(session.host(0)->active == 30);
	CHECK(session.detach(20, 200));
	CHECK(session.host(0)->tabs == (Tabs{10, 30}));
	CHECK(session.host(0)->active == 30 && session.host(200)->active == 20 && session.valid());
	CHECK(session.close(20)); CHECK(session.host(200) == nullptr && session.valid());

	// Moving an inactive tab is independent from press-time selection.
	CHECK(session.select(0, 30)); CHECK(session.detach(10, 300));
	CHECK(session.host(0)->active == 30 && session.host(300)->active == 10);
	CHECK(session.attach(10, 0));
	CHECK(session.host(0)->tabs == (Tabs{10, 30}));
	CHECK(session.host(0)->active == 10 && session.valid());

	// Closing selects the right neighbour, or the left neighbour at the end.
	CHECK(session.add(40)); CHECK(session.select(0, 30)); CHECK(session.close(30));
	CHECK(session.host(0)->active == 40); CHECK(session.close(40));
	CHECK(session.host(0)->active == 10 && session.valid());

	// Reordering never changes selection, including when moving an inactive tab.
	CHECK(session.add(20)); CHECK(session.add(30)); CHECK(session.select(0, 10));
	CHECK(session.reorder(0, 30, 0));
	CHECK(session.host(0)->tabs == (Tabs{30, 10, 20}));
	CHECK(session.host(0)->active == 10);
	CHECK(session.reorder(0, 10, 3));
	CHECK(session.host(0)->tabs == (Tabs{30, 20, 10}));
	CHECK(session.host(0)->active == 10 && session.valid());

	// Detached-window moves used by the UI: tab to an existing window, the
	// source window disappears when emptied, detach refuses a live host id.
	CHECK(session.detach(20, 7));
	CHECK(!session.detach(30, 7));                  // host 7 exists already
	CHECK(session.move(30, 7, 0));                   // drop onto window 7, first slot
	CHECK(session.host(7)->tabs == (Tabs{30, 20}));
	CHECK(session.host(7)->active == 30);
	CHECK(session.host(0)->tabs == (Tabs{10}));
	CHECK(session.move(20, 0, 0, false));            // back to main, not selected
	CHECK(session.host(0)->tabs == (Tabs{20, 10}) && session.host(0)->active == 10);
	CHECK(session.move(30, 0));                      // last tab leaves window 7
	CHECK(session.host(7) == nullptr);
	CHECK(session.hostIds() == (Tabs{0}));
	CHECK(session.owner(30).value_or(99) == 0);
	CHECK(!session.owner(12345).has_value());
	CHECK(session.valid());

	// Duplicate adds and unknown ids are refused.
	CHECK(!session.add(10)); CHECK(!session.add(0));
	CHECK(!session.select(0, 999)); CHECK(!session.close(999));

	// The main host survives being emptied.
	session.clear();
	CHECK(session.host(0) != nullptr && session.host(0)->tabs.empty() && session.valid());

	if (g_failures) { std::printf("workspace_session_test: %d failure(s)\n", g_failures); return 1; }
	std::printf("workspace_session_test: PASS\n");
	return 0;
}
