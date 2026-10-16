// MBAACC alignment math (docs/stage3d/SCOPE.md section 4): the source game's camera driven from MBAACC's 2D camera, and the
// stage3d.ini that makes PovertyCaster's pc-stage3d (CameraMap / StageIni) draw the same picture. Pure functions, no GL.
#ifndef S3D_ALIGN_H_GUARD
#define S3D_ALIGN_H_GUARD

#include <string>

namespace s3d {

struct AlignInput {
	float mbCamX = 0.f, mbZoom = 1.f;   // MBAACC 2D camera: world px (sim px / 128) and zoom
	float fighterScale = 1.f;           // s: MBAACC px per source px at the plane, x 1.5 (1.0 = source proportions)
	float centerX = 0.f;                // source world x units (1 unit = 640 source px) under MBAACC camera x = 0
	float floorPx = 432.f, yBias = 0.f; // MBAACC floor row and a small floor fix, MBAACC px
	int replaces = -1;                  // MBAACC 2D stage slot (-1 = not written)
	// from the loaded stage (BgList)
	float fovDeg = 45.f;                // BgList FOV: vertical, aspect-1 PerspectiveFovLH (UNI 1.42 camera)
	float vanishingPoint = 0.f;
	float scale[3] = {1, 1, 1}, position[3] = {0, 0, 0};
	float stageW = 4096.f;
	std::string package, name;
};

// MBAACC's camera clamp: 67584 fixed - 40960/zoom => |cam| <= 528 - 320/zoom world px (208 at zoom 1), never below 0.
float AlignCamLimit(float mbZoom);

// The source camera (camX in source 1280-wide px, camY, zoom) that shows what MBAACC's camera shows in the centre 4:3 crop.
struct AlignSourceCam { float camX, camY, zoom; };
AlignSourceCam AlignSourceCamera(const AlignInput& in);

// pc-stage3d keys for the same picture (SCOPE.md 4.2: unitPx = 360 * 2/3 * s; DFCI_ISSUES.md "Ini-only workaround").
float AlignUnitsToPx(const AlignInput& in);       // 240 * s
float AlignIniFov(const AlignInput& in);          // 2 atan(tan(FOV/2) * 16/9): pc converts back with 9/16 to vertical FOV
float AlignIniHorizonPx(const AlignInput& in);    // 240 (1 + VP) shifted with the floor (floor + bias - 640/1.5)
float AlignIniCenterX(const AlignInput& in);      // centerX in mesh units: centerX / 0.5625
std::string FormatAlignIni(const AlignInput& in);

}  // namespace s3d
#endif
