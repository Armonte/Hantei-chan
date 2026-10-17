#include "s3d_align.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace s3d {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kSrcPerMb = 1.5f;   // 720 source lines over MBAACC's 480
float S(const AlignInput& in) { return std::max(0.01f, in.fighterScale); }
}  // namespace

float AlignCamLimit(float mbZoom) { return std::max(0.f, 528.f - 320.f / std::max(0.01f, mbZoom)); }

AlignSourceCam AlignSourceCamera(const AlignInput& in)
{
	// 1 MBAACC px = 1.5 / s source px; the source zoom z*s keeps the same world height (720/s px over 480 lines)
	const float cam = std::clamp(in.mbCamX, -AlignCamLimit(in.mbZoom), AlignCamLimit(in.mbZoom));
	return {in.centerX * 640.f + cam * kSrcPerMb / S(in), 0.f, in.mbZoom * S(in)};
}

float AlignUnitsToPx(const AlignInput& in) { return 360.f / kSrcPerMb * S(in); }
float AlignIniFov(const AlignInput& in)
{
	const float f = std::clamp(in.fovDeg, 1.f, 170.f) * kPi / 360.f;
	return 2.f * std::atan(std::tan(f) * 16.f / 9.f) * 180.f / kPi;
}
float AlignIniHorizonPx(const AlignInput& in)
{
	return 360.f * (1.f + in.vanishingPoint) / kSrcPerMb + (in.floorPx + in.yBias - 640.f / kSrcPerMb);
}
float AlignIniCenterX(const AlignInput& in) { return in.centerX * 640.f / 360.f; }

std::string FormatAlignIni(const AlignInput& in)
{
	std::string o;
	char b[512];
	auto add = [&](const char* fmt, auto... a) { std::snprintf(b, sizeof b, fmt, a...); o += b; };
	add("; written by Hantei-chan's 3D Stage view, MBAACC alignment (%s)\n", in.name.c_str());
	add("; MBAACC camera tuned at x %.1f zoom %.3f; fighter scale %.4f\n", in.mbCamX, in.mbZoom, in.fighterScale);
	if (in.replaces >= 0) add("replaces = %d\n", in.replaces);
	if (!in.package.empty()) o += "package = " + in.package + "\n";
	add("fit_mode = crop\n");
	add("center_x = %.6g\n", AlignIniCenterX(in));
	add("stage_w = %.6g\n", in.stageW > 0.f ? in.stageW : 4096.f);
	add("host_world_w = 1056\n");
	// the source camera 1:1 (UNI 1.42): 360 source px per unit at the plane = 240*s MBAACC px, vertical FOV = BgList FOV,
	// horizon from VanishingPoint, no auto ground and no extra depth parallax
	add("units_to_px = %.6g\n", AlignUnitsToPx(in));
	add("world_scale = 1\n");
	add("fighter_px = %.6g\n", 237.f * S(in));
	add("uni_char_px = 355\n");
	add("fov = %.6g\n", AlignIniFov(in));
	add("ref_fov = %.6g\n", AlignIniFov(in));
	add("floor_px = %.6g\n", in.floorPx + in.yBias);
	add("horizon_px = %.6g\n", AlignIniHorizonPx(in));
	add("cam_offset = 0, 0, 0\n");
	add("ground_auto = 0\nground_y = 0\npan_boost = 0\n");
	// BgList placement in mesh units (pc applies one uniform scale and no X squeeze: Position.x / 0.5625)
	add("scale = %.6g\n", in.scale[0]);
	add("pos = %.6g, %.6g, %.6g\n", in.position[0] / 0.5625f, in.position[1], in.position[2]);
	return o;
}

}  // namespace s3d
