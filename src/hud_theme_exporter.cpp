#include "hud_theme_exporter.h"

#include "../third_party/json/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

nlohmann::json make_meter_entry(const char* color, int overlay_speed, bool overlay_locked = false) {
	nlohmann::json entry;
	entry["argb"] = color;
	entry["overlay_speed"] = overlay_speed;
	if (overlay_locked) {
		entry["overlay_locked"] = true;
	}
	return entry;
}

} // namespace

bool ExportHudThemeProfile(const std::filesystem::path& directory, bool overwrite) {
	try {
		if (directory.empty()) {
			return false;
		}

		std::filesystem::create_directories(directory);

		const auto outputPath = directory / "hud_theme.json";
		if (!overwrite && std::filesystem::exists(outputPath)) {
			return true;
		}

		nlohmann::json root;
		root["schema_version"] = 1;

		nlohmann::json metadata;
		metadata["name"] = "Hantei Default";
		metadata["author"] = "Hantei-chan";
		metadata["description"] = "Placeholder HUD theme exported from Hantei-chan.";
		root["metadata"] = metadata;

		nlohmann::json meter;
		meter["lower"] = make_meter_entry("#ffc80000", 1);
		meter["middle"] = make_meter_entry("#ffc8c800", 2);
		meter["upper"] = make_meter_entry("#ff00c800", 3);
		meter["unlimited"] = make_meter_entry("#ff3296ff", 2);
		meter["heat"] = make_meter_entry("#ff5a5ae6", -2);
		meter["max"] = make_meter_entry("#fffaa000", -2);
		meter["blood_heat"] = make_meter_entry("#ffb4b4b4", -2);
		meter["break"] = make_meter_entry("#ffbe64c8", -2, true);

		nlohmann::json guard;
		guard["quality_high"] = "#ff00bee6";
		guard["quality_low"] = "#ffe60a0a";
		guard["break"] = "#ff767676";

		nlohmann::json colors;
		colors["meter"] = meter;
		colors["guard"] = guard;
		root["colors"] = colors;

		nlohmann::json layout;
		layout["moon_icons"] = {
			{"visible", true},
			{"pivot", "center"},
			{"offset", {0, 0}}
		};
		layout["portraits"] = nlohmann::json::array();
		root["layout"] = layout;

		nlohmann::json assets;
		assets["gauge"] = {
			{"pack", "0003.p"},
			{"folder", "/GRP/gauge_AA"}
		};
		root["assets"] = assets;

		std::ofstream file(outputPath);
		if (!file.is_open()) {
			return false;
		}

		file << root.dump(2);
		return true;
	} catch (...) {
		return false;
	}
}

