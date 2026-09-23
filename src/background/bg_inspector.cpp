#include "bg_inspector.h"
#include "bg_file.h"
#include "bg_renderer.h"
#include "../filedialog.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace bg {

namespace {

const char* kAniTypes[] = {
	"0 despawn", "1 next", "2 jump", "3 next", "4 jump", "5 loop (jump/loopEnd)"
};
const char* kBlend[] = { "0 opaque (alpha 255)", "1 alpha (+10)", "2 additive", "3 multiply" };

template <typename T>
bool EditScalar(const char* label, ImGuiDataType type, T* v) {
	return ImGui::InputScalar(label, type, v);
}

bool EditU8Flag(const char* label, uint8_t* v) {
	bool b = *v != 0;
	if (ImGui::Checkbox(label, &b)) { *v = b ? 1 : 0; return true; }
	return false;
}

// One 52-byte record: type/w2 plus d[1..12], with a decoded hint.
bool EditRecord(EventRecord& r, bool trigger) {
	bool ch = false;
	ImGui::PushItemWidth(90);
	ch |= EditScalar("type", ImGuiDataType_S16, &r.type); ImGui::SameLine();
	ch |= EditScalar("w2", ImGuiDataType_S16, &r.w2);
	for (int k = 1; k <= 12; ++k) {
		char l[8];
		snprintf(l, sizeof(l), "d%d", k);
		ch |= EditScalar(l, ImGuiDataType_S32, &r.d[k]);
		if (k % 4 != 0) ImGui::SameLine();
	}
	ImGui::PopItemWidth();
	if (trigger) {
		if (r.type == 1)
			ImGui::TextDisabled("pos trigger: when %s %s %d (1/128 px) -> %s%d",
			                    r.d[3] == 0 ? "x" : (r.d[3] == 1 ? "y" : "?"),
			                    r.d[4] == 0 ? ">" : (r.d[4] == 1 ? "<" : "?"), r.d[2],
			                    r.d[1] == -1 ? "despawn" : "frame ", r.d[1] == -1 ? 0 : r.d[1]);
		else
			ImGui::TextDisabled("unknown trigger type (ignored by the game)");
	} else {
		switch (r.type) {
		case 1: ImGui::TextDisabled("spawn object slot %d at (%d, %d)", r.w2, r.d[1], r.d[2]); break;
		case 2: ImGui::TextDisabled("spawn slot %d + rand%%%d at x[%d..%d] y[%d..%d]",
		                            r.w2, std::max(1, (int)(int16_t)(r.d[5] & 0xFFFF)),
		                            r.d[1], r.d[3], r.d[2], r.d[4]); break;
		case 100: ImGui::TextDisabled("%srandom %s velocity [%d,%d) accel [%d,%d)",
		                              r.w2 ? "(w2 != 0: ignored) " : "",
		                              r.d[5] ? "Y" : "X", r.d[1], r.d[2], r.d[3], r.d[4]); break;
		default: ImGui::TextDisabled("unknown command type (ignored by the game)"); break;
		}
	}
	if (ch) r.SyncRaw();
	return ch;
}

void DrawSideFiles(File& file) {
	const StageListEntry* e = file.GetStageListEntry();
	const StageList& list = file.GetStageList();
	if (ImGui::TreeNode("BgList.ini")) {
		if (list.path.empty()) ImGui::TextDisabled("not found next to the stage");
		else {
			ImGui::TextWrapped("%s (%zu stages)", list.path.c_str(), list.entries.size());
			if (e) {
				ImGui::Text("[Bg_%03d] DataFile = %s", e->index, e->dataFile.c_str());
				ImGui::Text("IsSelectAble = %d  IsGiantStage = %d  InfoFile = %d",
				            e->isSelectable, e->isGiantStage, e->infoFile);
				ImGui::Text("StageColorVal = %.2f", e->stageColorVal);
				ImGui::TextDisabled("StageColorVal feeds fColorHosei of the BgPointBlur effect;\n"
				                    "InfoFile is parsed but the game loads <DataFile>Info.txt anyway.");
			} else {
				ImGui::TextDisabled("this stage has no [Bg_NNN] entry");
			}
			if (ImGui::TreeNode("All entries")) {
				for (const auto& x : list.entries)
					ImGui::Text("%03d %-6s sel=%d giant=%d color=%.2f%s", x.index, x.dataFile.c_str(),
					            x.isSelectable, x.isGiantStage, x.stageColorVal,
					            x.infoFile ? " info" : "");
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
	const StageInfo& info = file.GetStageInfo();
	if (ImGui::TreeNode("Info.txt (lights / weather)")) {
		if (!info.loaded) ImGui::TextDisabled("no <stage>Info.txt next to the stage");
		else {
			ImGui::TextWrapped("%s", info.path.c_str());
			ImGui::Text("LightNum = %zu", info.lights.size());
			for (size_t i = 0; i < info.lights.size(); ++i)
				ImGui::Text("  Light%02zu Pos=%d (world x %d) Power=%d", i, info.lights[i].pos,
				            info.lights[i].pos - 512, info.lights[i].power);
			ImGui::Text("DropObj = %d  Type = %d", info.dropObj, info.dropType);
			if (info.dropObj && info.dropType == 0)
				ImGui::Text("  bitmap %s.bmp  PatNum=%d FrameNum=%d Wait=%d  %dx%d  (%s)",
				            info.dropFile.c_str(), info.patNum, info.frameNum, info.wait,
				            info.w, info.h, file.DropBitmapPath().empty() ? "bmp missing" : "bmp found");
			else if (info.dropObj && info.dropType == 1)
				ImGui::Text("  rain: Max=%d H=%d Alpha=%d Wait(speed)=%d", info.count, info.h,
				            info.alpha, info.wait);
			else if (info.dropObj && info.dropType == -1)
				ImGui::TextDisabled("  type -1: 3D grid effect (not previewed)");
			ImGui::TextDisabled("Lights add one character shadow pass each (not a stage draw);\n"
			                    "shown as markers.");
		}
		ImGui::TreePop();
	}
	const LightFile& lf = file.GetLightFile();
	if (ImGui::TreeNode("light.txt (MBAC)")) {
		if (!lf.loaded) ImGui::TextDisabled("no <stage>light.txt");
		else {
			ImGui::TextWrapped("%s", lf.path.c_str());
			for (size_t i = 0; i < lf.lights.size(); ++i)
				ImGui::Text("  light %zu pos=%d power=%d", i, lf.lights[i].pos, lf.lights[i].power);
			ImGui::TextDisabled("Read by MBAC only (MBAACC ships copies but never opens them).");
		}
		ImGui::TreePop();
	}
}

} // namespace

InspectorResult DrawInspector(File& file, Renderer& renderer) {
	InspectorResult res;
	auto& objects = file.GetObjects();

	// --- save ---
	if (ImGui::Button("Save")) {
		if (file.Save(file.GetFilename().c_str())) file.ClearDirty();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) {
		std::string path = FileDialog(fileType::DAT, true);
		if (!path.empty() && file.Save(path.c_str())) file.ClearDirty();
	}
	ImGui::SameLine();
	ImGui::TextDisabled(file.IsDirty() ? "(modified)" : "(saved)");

	// --- game flavour / variants / RNG ---
	int game = (int)file.GetGame();
	const char* games[] = { "MBAACC", "MBAC" };
	ImGui::PushItemWidth(110);
	if (ImGui::Combo("Game", &game, games, 2)) file.SetGame((Game)game);
	ImGui::PopItemWidth();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Same file format; the renderers and RNGs differ.\n"
		                  "MBAC: frame +16/+18 scale, PAT rotation, no part position,\n"
		                  "no 0xEA tint, LCG RNG, no weather.");
	std::string sib = file.SiblingVariantPath();
	if (!sib.empty()) {
		ImGui::SameLine();
		if (ImGui::Button(file.IsShortVariant() ? "Open full variant" : "Open _s variant"))
			res.openPath = sib;
	}
	int seed = file.GetSeed();
	ImGui::PushItemWidth(110);
	if (ImGui::InputInt("RNG seed", &seed, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
		file.SetSeed(seed);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Restart")) file.ResetRuntime();
	ImGui::Text("tick %llu  rng draws %llu", (unsigned long long)file.GetTick(),
	            (unsigned long long)file.GetRng().Calls());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("MBAACC: RngState_Initialize(seed, stream 0), the stream used by random\n"
		                  "spawns, random velocity and weather. The game seeds it from its\n"
		                  "master seed each round; other users of stream 0 (some effects) shift it.");
	bool w = renderer.IsShowingWeather();
	if (ImGui::Checkbox("Weather", &w)) renderer.SetShowWeather(w);
	ImGui::SameLine();
	bool l = renderer.IsShowingLights();
	if (ImGui::Checkbox("Light markers", &l)) renderer.SetShowLights(l);
	if (file.GetDrops().IsActive())
		ImGui::TextDisabled("weather: %zu particles (type %d)", file.GetDrops().Particles().size(),
		                    file.GetDrops().Type());

	if (ImGui::CollapsingHeader("Stage files")) DrawSideFiles(file);

	// --- objects ---
	static int sel = 0;
	static int selFrame = 0;
	if (sel >= (int)objects.size()) sel = 0;
	renderer.SetSelectedObject(sel);

	if (!ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) return res;
	if (ImGui::SmallButton("Show All")) for (auto& o : objects) o.visible = true;
	ImGui::SameLine();
	if (ImGui::SmallButton("Solo Selected"))
		for (size_t i = 0; i < objects.size(); ++i) objects[i].visible = ((int)i == sel);
	int live[256] = {0};
	for (const auto& in : file.GetInstances())
		if (in.state >= 1 && in.objIndex >= 0 && in.objIndex < 256) ++live[in.objIndex];
	ImGui::BeginChild("objlist", ImVec2(0, 160), true);
	for (size_t i = 0; i < objects.size(); ++i) {
		const Object& o = objects[i];
		ImGui::PushID((int)i);
		ImGui::Checkbox("##vis", &objects[i].visible);
		ImGui::SameLine();
		char label[128];
		snprintf(label, sizeof(label), "slot %d  L%d P%d  %zuf  x%d%s%s%s%s",
		         o.originalIndex, o.layer, o.parallax, o.frames.size(), i < 256 ? live[i] : 0,
		         o.foreground ? " FG" : "", o.noAutoSpawn ? " spawned-only" : "",
		         o.commands.empty() ? "" : " cmd", o.triggers.empty() ? "" : " trg");
		if (ImGui::Selectable(label, sel == (int)i)) { sel = (int)i; selFrame = 0; }
		ImGui::PopID();
	}
	ImGui::EndChild();

	if (sel < 0 || sel >= (int)objects.size()) return res;
	Object& obj = objects[sel];
	bool ch = false;

	ImGui::Text("Object slot %d", obj.originalIndex);
	ImGui::PushItemWidth(100);
	ch |= ImGui::InputInt("Parallax (256 = world)", &obj.parallax);
	ch |= ImGui::InputInt("Layer", &obj.layer);
	ImGui::PopItemWidth();
	ch |= EditU8Flag("No auto-spawn (+20)", &obj.noAutoSpawn);
	ImGui::SameLine();
	ch |= EditU8Flag("In front of characters (+21)", &obj.foreground);
	ch |= EditU8Flag("Smooth filter (+22)", &obj.linearFilter);
	if (ch) file.MarkDirty();
	if (obj.currentFrame >= 0)
		ImGui::TextDisabled("first live instance: frame %d  timer %d  pos (%d, %d)",
		                    obj.currentFrame, obj.frameDuration, obj.posX >> 7, obj.posY >> 7);
	if (ImGui::Button("<< Step")) file.StepObjectBackward(sel);
	ImGui::SameLine();
	if (ImGui::Button("Step >>")) file.StepObjectForward(sel);

	// --- frames ---
	if (ImGui::TreeNodeEx("Frames", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (selFrame >= (int)obj.frames.size()) selFrame = (int)obj.frames.size() - 1;
		if (selFrame < 0) selFrame = 0;
		ImGui::BeginChild("frlist", ImVec2(0, 120), true);
		for (size_t f = 0; f < obj.frames.size(); ++f) {
			const Frame& fr = obj.frames[f];
			char label[128];
			snprintf(label, sizeof(label), "%s%2zu spr %d  (%d,%d)  dur %u  ani %d%s",
			         (int)f == obj.currentFrame ? ">" : " ", f, fr.spriteId, fr.offsetX,
			         fr.offsetY, (unsigned)(uint16_t)fr.duration, fr.aniType,
			         fr.interpolate ? " lerp" : "");
			if (ImGui::Selectable(label, selFrame == (int)f)) selFrame = (int)f;
		}
		ImGui::EndChild();
		if (ImGui::SmallButton("Insert")) { int n = file.InsertFrame(sel, selFrame, false); if (n >= 0) selFrame = n; }
		ImGui::SameLine();
		if (ImGui::SmallButton("Duplicate")) { int n = file.InsertFrame(sel, selFrame, true); if (n >= 0) selFrame = n; }
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete")) file.DeleteFrame(sel, selFrame);

		if (selFrame >= 0 && selFrame < (int)obj.frames.size()) {
			Frame& fr = obj.frames[selFrame];
			bool fch = false;
			ImGui::PushItemWidth(100);
			fch |= EditScalar("Sprite (>=10000 CG, <10000 PAT)", ImGuiDataType_S16, &fr.spriteId);
			fch |= EditScalar("Offset X", ImGuiDataType_S16, &fr.offsetX); ImGui::SameLine();
			fch |= EditScalar("Offset Y", ImGuiDataType_S16, &fr.offsetY);
			fch |= EditScalar("Duration", ImGuiDataType_U16, (uint16_t*)&fr.duration);
			int blend = std::min<int>(fr.blendMode, 3);
			ImGui::PushItemWidth(160);
			if (ImGui::Combo("Blend (+9)", &blend, kBlend, 4)) { fr.blendMode = (uint8_t)blend; fch = true; }
			ImGui::PopItemWidth();
			fch |= EditScalar("Opacity (+10)", ImGuiDataType_U8, &fr.opacity);
			int ani = std::min<int>(fr.aniType, 5);
			ImGui::PushItemWidth(160);
			if (ImGui::Combo("Anim type (+11)", &ani, kAniTypes, 6)) { fr.aniType = (uint8_t)ani; fch = true; }
			ImGui::PopItemWidth();
			fch |= EditScalar("Jump (+12)", ImGuiDataType_U8, &fr.jumpFrame); ImGui::SameLine();
			fch |= EditScalar("Loop end (+21)", ImGuiDataType_U8, &fr.loopEnd);
			fch |= EditScalar("Loop count (+22)", ImGuiDataType_U8, &fr.loopCount);
			fch |= EditU8Flag("Interpolate to next (+20)", &fr.interpolate);
			fch |= EditScalar("Scale X (+16, MBAC)", ImGuiDataType_S16, &fr.scaleX); ImGui::SameLine();
			fch |= EditScalar("Scale Y (+18)", ImGuiDataType_S16, &fr.scaleY);
			fch |= EditU8Flag("Clear X (+44)", &fr.flagClearX); ImGui::SameLine();
			fch |= EditU8Flag("Clear Y (+45)", &fr.flagClearY); ImGui::SameLine();
			fch |= EditU8Flag("Set X (+46)", &fr.flagSetX); ImGui::SameLine();
			fch |= EditU8Flag("Set Y (+47)", &fr.flagSetY);
			fch |= EditScalar("Vel X (+52)", ImGuiDataType_S16, &fr.velX); ImGui::SameLine();
			fch |= EditScalar("Vel Y (+54)", ImGuiDataType_S16, &fr.velY);
			fch |= EditScalar("Acc X (+60)", ImGuiDataType_S16, &fr.accX); ImGui::SameLine();
			fch |= EditScalar("Acc Y (+62)", ImGuiDataType_S16, &fr.accY);
			ImGui::PopItemWidth();
			ImGui::Text("Trigger refs (+100) / command refs (+116), -1 = none:");
			ImGui::PushItemWidth(40);
			for (int k = 0; k < 8; ++k) {
				ImGui::PushID(k);
				fch |= EditScalar("##t", ImGuiDataType_S16, &fr.triggerRef[k]);
				ImGui::PopID();
				if (k < 7) ImGui::SameLine();
			}
			for (int k = 0; k < 8; ++k) {
				ImGui::PushID(100 + k);
				fch |= EditScalar("##c", ImGuiDataType_S16, &fr.commandRef[k]);
				ImGui::PopID();
				if (k < 7) ImGui::SameLine();
			}
			ImGui::PopItemWidth();
			if (fch) file.MarkDirty();
		}
		ImGui::TreePop();
	}

	// --- event records ---
	for (int t = 0; t < 2; ++t) {
		bool trig = t == 0;
		auto& tab = trig ? obj.triggers : obj.commands;
		char hdr[64];
		snprintf(hdr, sizeof(hdr), "%s (%zu)###%s", trig ? "Position triggers" : "Frame commands",
		         tab.size(), trig ? "trg" : "cmd");
		if (!ImGui::TreeNode(hdr)) continue;
		for (size_t i = 0; i < tab.size(); ++i) {
			ImGui::PushID((int)i + (trig ? 0 : 1000));
			ImGui::Separator();
			ImGui::Text("#%zu", i);
			if (EditRecord(tab[i], trig)) { obj.recordsEdited = true; file.MarkDirty(); }
			ImGui::PopID();
		}
		if (ImGui::SmallButton("Add record")) file.AddRecord(sel, trig);
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove last")) file.DeleteLastRecord(sel, trig);
		ImGui::TextDisabled("Records are only reached through frame refs; adding or removing one\n"
		                    "rewrites this object's table block as [triggers][commands].");
		ImGui::TreePop();
	}
	return res;
}

} // namespace bg
