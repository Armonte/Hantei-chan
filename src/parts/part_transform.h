#ifndef PART_TRANSFORM_H_GUARD
#define PART_TRANSFORM_H_GUARD
// The per-part placement the GL part renderer (Parts::Draw) uses, kept in one header-only place so the
// regression test (tests/fb_part_transform_test.cpp) exercises exactly the code that draws.
// French-Bread model + evidence: docs/formats/fb_part_transform.md.
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "parts_part.h"
#include "parts_partset.h"

namespace partxf {

// Where the part's transform origin sits in the layer's space.
//   MBAA / UNI / MBTL: the part position itself (the cut-out origin is where the art hangs from it).
//   French-Bread (fb = true): the part position is the quad's TOP-LEFT corner and the engine rotates and scales
//   about (position + cut-out origin): RBO PosePart_LerpPosition 0x44DC70 (pivot = x + ox, corner = pivot - ox*scale),
//   Pose_SubmitPartQuad 0x44E5B0 (quad spans -scale*ox .. -scale*ox + w*scale around the pivot).
inline glm::vec2 Pivot(bool fb, float x, float y, const CutOut<> &co)
{
	return fb ? glm::vec2(x + co.xy[0], y + co.xy[1]) : glm::vec2(x, y);
}

// Part matrix: translate(pivot) * Ry * Rx * Rz * translate(pras) * scale. `rotTurns` = (x, y, z) in turns
// (1 = 360 degrees); z is the in-plane angle. Positive z is clockwise on a y-down screen, which is the engine's
// convention (RBO Mat_RotateAboutPivotDeg 0x413350: x' = x cos - y sin, y' = x sin + y cos).
inline glm::mat4 PartMatrix(glm::vec2 pivot, glm::vec3 rotTurns, glm::vec2 pras, glm::vec2 scale)
{
	constexpr float tau = glm::pi<float>() * 2.f;
	glm::mat4 m(1.f);
	m = glm::translate(m, glm::vec3(pivot.x, pivot.y, 0.f));
	m = glm::rotate(m, rotTurns[1] * tau, glm::vec3(0.f, 1.f, 0.f));
	m = glm::rotate(m, rotTurns[0] * tau, glm::vec3(1.f, 0.f, 0.f));
	m = glm::rotate(m, rotTurns[2] * tau, glm::vec3(0.f, 0.f, 1.f));
	if (pras.x != 0.f || pras.y != 0.f) m = glm::translate(m, glm::vec3(pras.x, pras.y, 0.f));
	return glm::scale(m, glm::vec3(scale.x, scale.y, 1.f));
}

// The corner `t` (0..1 in x and y of the cut-out quad) in the matrix's local space: the quad hangs at -xy.
inline glm::vec2 QuadPoint(const CutOut<> &co, float tx, float ty)
{
	return glm::vec2(-co.xy[0] + co.wh[0] * tx, -co.xy[1] + co.wh[1] * ty);
}

// Draw order of a part set. French-Bread engines draw ASCENDING (layer << 8) + slot, the last part on top
// (RBO Pose_CollectSortedParts 0x44E030, GOF2 DrawSort_BubbleByPriority 0x43FAA0); our model keeps layer + slot/1000
// in `priority`. The other games draw the highest priority first.
inline void SortForDraw(bool fb, std::vector<PartProperty> &groups)
{
	if (fb) {
		std::stable_sort(groups.begin(), groups.end(), [](const PartProperty &a, const PartProperty &b) { return a.priority < b.priority; });
	} else {
		// stable sort on the reversed range: higher priority first, equal priorities keep their file order
		std::stable_sort(groups.rbegin(), groups.rend(), [](const PartProperty &a, const PartProperty &b) { return a.priority < b.priority; });
	}
}

} // namespace partxf
#endif
