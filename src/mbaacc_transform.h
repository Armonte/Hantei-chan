#pragma once

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

// MBAACC builds CharacterSubObj transforms with D3DX row vectors. Hantei-chan
// renders with GLM column vectors and a Y-down orthographic viewport. Keep the
// conversion and its editor-interaction inverse in one place.
//
// Adopted from Gonptechan EX (drop 1abc27f9). EX verified it against a runtime
// capture (MBAACC 1.07, Kyo pattern 149 EF1 -> pattern 412, angle 2288, facing
// left). The facing/sign model matches MBAA Effect_InitializeComplex 0x454900:
// facing is resolved first, then only X is mirrored (see HANTEI_PREVIEW_SIM.md).
namespace MbaaTransform {
inline glm::mat4 ActorMatrix(float authoredTurns, bool facingFlipped)
{
	glm::mat4 result(1.f);
	// Authored angle with left facing resolves (after removing MBAA's separate
	// 2x CG draw scale) to F * R(+angle) in GLM column-vector form.
	if (facingFlipped) result = glm::scale(result, glm::vec3(-1.f, 1.f, 1.f));
	return glm::rotate(result, authoredTurns * glm::two_pi<float>(), glm::vec3(0.f, 0.f, 1.f));
}

inline float AuthoredXDeltaFromScreen(float screenDelta, bool facingFlipped)
{
	return facingFlipped ? -screenDelta : screenDelta;
}

inline float AuthoredRotationDeltaFromScreen(float clockwiseScreenRadians, bool facingFlipped)
{
	// Rotation dragging is a visual interaction. The authored angle follows the
	// Y-down cursor directly when unflipped; the actor-facing reflection reverses
	// that relationship for a flipped asset.
	return clockwiseScreenRadians * (facingFlipped ? -1.f : 1.f);
}
}
