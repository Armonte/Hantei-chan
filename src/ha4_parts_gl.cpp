// GL texture upload for parts converted from an MBAC old-format PAT
// (mirrors the PGTX branch of Parts::Load).
#include "ha4_parts.h"
#include "parts/parts.h"

namespace ha4 {

void UploadPartsTextures(Parts &parts)
{
	for (auto *t : parts.textures) delete t;
	parts.textures.clear();
	for (auto &gfx : parts.gfxMeta) {
		parts.textures.push_back(new Texture);
		gfx.textureIndex = 0;
		if (!gfx.data) continue;
		parts.textures.back()->LoadDirect(gfx.data, gfx.w, gfx.h, true);
		parts.textures.back()->Apply(false, false);
		gfx.textureIndex = parts.textures.back()->id;
	}
}

} // namespace ha4
