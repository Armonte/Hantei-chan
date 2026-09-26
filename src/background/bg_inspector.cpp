#include "bg_inspector.h"
#include "bg_file.h"
#include "bg_renderer.h"
#include "../filedialog.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <string>

namespace bg {

void (*g_onPatPlacementChanged)(bool authoring) = nullptr;
static void PatPlacementChanged(bool a) { if (g_onPatPlacementChanged) g_onPatPlacementChanged(a); }

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
		TextIni& ini = file.InfoText();
		ImGui::TextWrapped("%s%s", ini.Path().c_str(), info.loaded ? "" : "  (not present: edits create it on save)");
		if (ImGui::Button("Save Info.txt")) file.SaveInfo();
		ImGui::SameLine();
		ImGui::TextDisabled(file.IsInfoDirty() ? "(modified)" : "(saved)");
		auto setInt = [&](const char* key, int v) { file.SetInfoValue(key, std::to_string(v)); };
		ImGui::PushItemWidth(110);
		int n = (int)info.lights.size();
		if (ImGui::InputInt("LightNum", &n)) setInt("LightNum", std::max(0, std::min(10, n)));
		for (int i = 0; i < (int)info.lights.size(); ++i) {
			char k[32];
			int pos = info.lights[i].pos, pw = info.lights[i].power;
			snprintf(k, sizeof(k), "Light%02dPos", i);
			if (ImGui::InputInt(k, &pos)) setInt(k, pos);
			ImGui::SameLine();
			snprintf(k, sizeof(k), "Light%02dPower", i);
			if (ImGui::InputInt(k, &pw)) setInt(k, pw);
			ImGui::SameLine();
			ImGui::TextDisabled("world x %d", info.lights[i].pos - 512);
		}
		bool drop = info.dropObj != 0;
		if (ImGui::Checkbox("DropObj (weather)", &drop)) setInt("DropObj", drop ? 1 : 0);
		if (drop) {
			int t = info.dropType;
			const char* types[] = {"-1 grid room (bg99)", "0 bitmap particles", "1 rain"};
			int ti = t + 1;
			if (ImGui::Combo("DropObj_Type", &ti, types, 3)) setInt("DropObj_Type", ti - 1);
			if (t == 0) {
				char buf[64];
				snprintf(buf, sizeof(buf), "%s", info.dropFile.c_str());
				if (ImGui::InputText("DropObjFile", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) file.SetInfoValue("DropObjFile", buf);
				int v;
				v = info.patNum;   if (ImGui::InputInt("DropObj_PatNum", &v)) setInt("DropObj_PatNum", v);
				v = info.frameNum; if (ImGui::InputInt("DropObj_FrameNum", &v)) setInt("DropObj_FrameNum", v);
				v = info.wait;     if (ImGui::InputInt("DropObj_Wait", &v)) setInt("DropObj_Wait", v);
				v = info.w;        if (ImGui::InputInt("DropObj_W", &v)) setInt("DropObj_W", v);
				v = info.h;        if (ImGui::InputInt("DropObj_H", &v)) setInt("DropObj_H", v);
				ImGui::TextDisabled("  bitmap %s", file.DropBitmapPath().empty() ? "missing" : "found");
			} else if (t == 1) {
				int v;
				v = info.count; if (ImGui::InputInt("DropObj_Max", &v)) setInt("DropObj_Max", v);
				ImGui::SameLine(); ImGui::TextDisabled("(game array holds 100)");
				v = info.alpha; if (ImGui::InputInt("DropObj_Alpha", &v)) setInt("DropObj_Alpha", v);
				v = info.h;     if (ImGui::InputInt("DropObj_H (streak)", &v)) setInt("DropObj_H", v);
				v = info.wait;  if (ImGui::InputInt("DropObj_Wait (speed)", &v)) setInt("DropObj_Wait", v);
			}
		}
		ImGui::PopItemWidth();
		ImGui::TextDisabled("Keys are read from the whole file; the first occurrence wins.\n"
		                    "Lights: one fighter shadow per light instead of the flat shadow.");
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

static void DrawInspectorBody(File& file, Renderer& renderer, InspectorResult& res) {
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
	if (ImGui::Button("Undo") && file.CanUndo()) file.Undo();
	ImGui::SameLine();
	if (ImGui::Button("Redo") && file.CanRedo()) file.Redo();
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
	{
		int pp = renderer.GetPatPlacement() == Renderer::PatPlacement::Authoring ? 1 : 0;
		const char* pps[] = { "Game-exact", "Authoring" };
		ImGui::PushItemWidth(120);
		if (ImGui::Combo("PAT placement", &pp, pps, 2)) {
			renderer.SetPatPlacement(pp ? Renderer::PatPlacement::Authoring : Renderer::PatPlacement::Game);
			PatPlacementChanged(pp != 0);
		}
		ImGui::PopItemWidth();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Game-exact: PAT part positions as MBAACC applies them (checked against the game).\n"
			                  "Authoring: parts at the PAT canvas origin (320, 320) are drawn unpositioned, as MBAC\n"
			                  "would. Only the wind (bg18/bg20/bg47 slot 8) uses it; MBAACC draws that wind\n"
			                  "below the floor, where no game camera can see it.");
	}
	{
		static float heat = 0.0f;
		ImGui::PushItemWidth(120);
		ImGui::SliderFloat("HEAT preview", &heat, 0.0f, 1.0f, heat > 0.0f ? "%.2f" : "off");
		ImGui::PopItemWidth();
		renderer.SetHeatPreview(heat, (float)ImGui::GetTime());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The BgPointBlur post effect MBAA runs while a fighter is in HEAT / BLOOD HEAT:\n"
			                  "radial blur toward the centre, brightened by 1 + StageColorVal * value, masked by\n"
			                  "grp/_NewFx/EXFADE08, half-desaturated at 1; the foreground band fades out.\n"
			                  "Drawn over the game view. 1 = held; the game ramps in over 15 frames, out over 30.");
	}
	bool l = renderer.IsShowingLights();
	if (ImGui::Checkbox("Light markers", &l)) renderer.SetShowLights(l);
	{
		auto& si = renderer.GetStandIns();
		ImGui::Checkbox("Stand-in fighters + shadows", &si.enabled);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Two silhouettes with the game's fighter shadows: one flat shadow, or one\n"
			                  "per Info.txt light (strength fades over the light's Power).");
		if (si.enabled) {
			ImGui::PushItemWidth(200);
			ImGui::DragFloat2("Fighter x", si.x, 1.0f, -512.0f, 512.0f, "%.0f");
			ImGui::PopItemWidth();
		}
	}
	if (file.GetDrops().IsActive())
		ImGui::TextDisabled("weather: %zu particles (type %d)", file.GetDrops().Particles().size(),
		                    file.GetDrops().Type());

	if (ImGui::CollapsingHeader("Stage files")) DrawSideFiles(file);

	// --- objects ---
	static int sel = 0;
	static int selFrame = 0;
	if (sel >= (int)objects.size()) sel = 0;
	renderer.SetSelectedObject(sel);

	if (!ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) return;
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
		const bool originParts = Renderer::ObjectUsesCanvasOriginParts(file, (int)i);
		snprintf(label, sizeof(label), "slot %d  L%d P%d  %zuf  x%d%s%s%s%s%s",
		         o.originalIndex, o.layer, o.parallax, o.frames.size(), i < 256 ? live[i] : 0,
		         o.foreground ? " FG" : "", o.noAutoSpawn ? " spawned-only" : "",
		         o.commands.empty() ? "" : " cmd", o.triggers.empty() ? "" : " trg",
		         originParts ? "  [authoring/game placement differ]" : "");
		if (ImGui::Selectable(label, sel == (int)i)) { sel = (int)i; selFrame = 0; }
		if (originParts && ImGui::IsItemHovered())
			ImGui::SetTooltip("Its PAT parts sit at the canvas origin (320, 320). The game applies that offset and\n"
			                  "draws the object below the floor (off screen); PAT placement 'Authoring' draws it\n"
			                  "where it was authored (as MBAC does).");
		ImGui::PopID();
	}
	ImGui::EndChild();

	if (sel < 0 || sel >= (int)objects.size()) return;
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
}

InspectorResult DrawInspector(File& file, Renderer& renderer) {
	InspectorResult res;
	DrawInspectorBody(file, renderer, res);
	// One history step per gesture: commit once no widget is being edited.
	if (file.HasUncommittedEdit() && !ImGui::IsAnyItemActive()) file.CommitEdit();
	return res;
}

} // namespace bg
