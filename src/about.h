#ifndef ABOUT_H_GUARD
#define ABOUT_H_GUARD

#include "draw_window.h"
#include "version.h"
#include <imgui.h>

#ifndef HA6GUIVERSION
#define HA6GUIVERSION " custom"
#endif

//This is the main pane on the left
class AboutWindow 
{
public:
	bool isVisible = false;

	void Draw()
	{
		if(isVisible)
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1,1,1,1));
			ImGui::Begin("About", &isVisible, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
			ImGui::Text("Hantei-chan v" VERSION_STRING);
			ImGui::Text("Build %d (commit %s)", BUILD_NUMBER, GIT_COMMIT_HASH GIT_DIRTY_FLAG);
			ImGui::Text("Built: %s", BUILD_TIMESTAMP);
			ImGui::Separator();
			ImGui::Text("Made by omanko.");
			ImGui::Spacing();
			ImGui::Text("Special thanks to mauve, MadScientist, u4ick, and Rhekar");
			ImGui::Spacing();
			ImGui::Text("Fork by gonp & Armonté https://github.com/gonpgonp/Hantei-chan");
			ImGui::Text("\nApplication average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			ImGui::End();
			ImGui::PopStyleColor(2);
		}
	}

private:
};


#endif /* ABOUT_H_GUARD */
