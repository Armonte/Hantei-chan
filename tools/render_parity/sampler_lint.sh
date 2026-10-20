#!/bin/bash
# sampler_lint.sh <repo-root>: every ImGui draw of an app texture uses the
# texture's own sampler (src/imgui_utils.h ui::Image / ui::ImageWithOwnSampler /
# ui::BeginOwnSampler). The baseline build (IMGUI_IMPL_OPENGL_ES2) never bound a
# sampler; without ES2 the GL3 backend binds a linear one unless asked not to,
# which blurs the nearest-filter pixel-art previews. A raw ImGui::Image /
# ImageWithBg / ImageButton / ImDrawList::AddImage* call fails this check unless
# its line says "own sampler" (a run opened by ui::BeginOwnSampler).
set -u
ROOT="${1:?usage: sampler_lint.sh <repo-root>}"
cd "$ROOT" || exit 2

hits="$(grep -rnE 'ImGui::Image(WithBg|Button)?\(|(->|\.)AddImage(Quad|Rounded)?\(' src --include='*.cpp' --include='*.h' | grep -v 'own sampler')"
if [ -n "$hits" ]; then
	echo "sampler_lint: ImGui image draws without the texture's own sampler:" >&2
	echo "$hits" >&2
	exit 1
fi
echo "sampler_lint: ok"
