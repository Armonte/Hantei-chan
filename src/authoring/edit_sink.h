#ifndef AUTHORING_EDIT_SINK_H_GUARD
#define AUTHORING_EDIT_SINK_H_GUARD
// [authoring] How a panel edits the sidecar workspace so that every edit is ONE undo step, saved at once and
// live-applied (docs/HANTEI_AUTHORING_MODE.md §5.5, §8.2 "apply is instant on every edit").
//
//   sink.Begin("reservePortrait (hud)");  // on ImGui::IsItemActivated(), or right before a one-shot edit
//   ... change the SidecarDoc's TagIni ...
//   sink.Edited();                         // after every change (a drag throttles its saves to ~6 per second)
//   sink.End();                            // on ImGui::IsItemDeactivated(): history entry + final save + ApplyTuning
//
// A one-shot edit (a checkbox, a combo, a button) is Begin + change + End in the same frame. Begin while a gesture is
// open is a no-op (so nested widgets are safe). The window implements it (authoring_window.cpp).
#include <string>

namespace tagtune { class SidecarWorkspace; }

namespace authoring {

class EditSink {
public:
	virtual ~EditSink() = default;
	virtual tagtune::SidecarWorkspace& Workspace() = 0;
	virtual bool CanEdit() const = 0;               // false: session lock / lever table mismatch / no tree
	virtual void Begin(const std::string& label) = 0;
	virtual void Edited() = 0;
	virtual void End() = 0;
	void OneShot(const std::string& label) { Begin(label); Edited(); End(); }
};

} // namespace authoring

#endif
