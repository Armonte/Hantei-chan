// Parts model undo channel. See parts_undo.h.
#include "parts_undo.h"
#include "parts.h"

#include <cstring>
#include <memory>

namespace {

struct PartsSnap {
	bool loaded = false;
	std::vector<std::shared_ptr<const PartSet<>>> sets;
	std::shared_ptr<const std::vector<CutOut<>>> cuts;
	std::shared_ptr<const std::vector<Shape<>>> shapes;
	std::shared_ptr<const std::vector<PartGfx<>>> gfx;
};

bool PropEq(const PartProperty& a, const PartProperty& b)
{
	return a.propId == b.propId && a.priority == b.priority && memcmp(a.rotation, b.rotation, sizeof a.rotation) == 0
		&& memcmp(a.pras, b.pras, sizeof a.pras) == 0 && a.x == b.x && a.y == b.y && a.scaleX == b.scaleX && a.scaleY == b.scaleY
		&& a.ppId == b.ppId && a.additive == b.additive && a.filter == b.filter && a.pral == b.pral && a.prfl == b.prfl
		&& a.prpa == b.prpa && a.hasPrpa == b.hasPrpa && a.flip == b.flip && memcmp(a.bgra, b.bgra, 4) == 0
		&& memcmp(a.addColor, b.addColor, sizeof a.addColor) == 0;
}

bool SetEq(const PartSet<>& a, const PartSet<>& b)
{
	if (a.partId != b.partId || a.wasLoaded != b.wasLoaded || a.name != b.name || a.groups.size() != b.groups.size()) return false;
	for (size_t i = 0; i < a.groups.size(); i++) if (!PropEq(a.groups[i], b.groups[i])) return false;
	return true;
}

bool CutEq(const CutOut<>& a, const CutOut<>& b)
{
	return a.id == b.id && a.name == b.name && memcmp(a.uv, b.uv, sizeof a.uv) == 0 && memcmp(a.xy, b.xy, sizeof a.xy) == 0
		&& memcmp(a.wh, b.wh, sizeof a.wh) == 0 && a.texture == b.texture && memcmp(a.ppjp, b.ppjp, sizeof a.ppjp) == 0
		&& a.vaoIndex == b.vaoIndex && a.colorSlot == b.colorSlot && a.shapeIndex == b.shapeIndex
		&& memcmp(a.ppte, b.ppte, sizeof a.ppte) == 0 && a.pptx == b.pptx && a.altTags == b.altTags && a.nameFixed == b.nameFixed;
}

bool ShapeEq(const Shape<>& a, const Shape<>& b)
{
	return a.id == b.id && a.name == b.name && a.type == b.type && a.vertexCount == b.vertexCount && a.vertexCount2 == b.vertexCount2
		&& a.length == b.length && a.length2 == b.length2 && a.radius == b.radius && a.dRadius == b.dRadius && a.width == b.width && a.dz == b.dz;
}

bool GfxEq(const PartGfx<>& a, const PartGfx<>& b)
{
	return a.id == b.id && a.name == b.name && a.data == b.data && a.w == b.w && a.h == b.h && a.bpp == b.bpp && a.type == b.type
		&& memcmp(a.uvBpp, b.uvBpp, sizeof a.uvBpp) == 0 && memcmp(a.pgte, b.pgte, sizeof a.pgte) == 0 && a.pgtp == b.pgtp
		&& a.s3tc == b.s3tc && a.imageData == b.imageData && a.imageSize == b.imageSize && a.noCompress == b.noCompress;
}

template<typename T, typename Eq>
bool VecEq(const std::vector<T>& a, const std::vector<T>& b, Eq eq)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++) if (!eq(a[i], b[i])) return false;
	return true;
}

} // namespace

bool PartsUndoChannel::active() const
{
	return m_parts && m_parts->loaded && (!enabled || enabled());
}

UndoChannel::Snap PartsUndoChannel::capture(const Snap& previous)
{
	auto prev = std::static_pointer_cast<const PartsSnap>(previous);
	auto s = std::make_shared<PartsSnap>();
	const Parts& p = *m_parts;
	s->loaded = p.loaded;
	s->sets.reserve(p.partSets.size());
	for (size_t i = 0; i < p.partSets.size(); i++) {
		if (prev && i < prev->sets.size() && SetEq(*prev->sets[i], p.partSets[i])) s->sets.push_back(prev->sets[i]);
		else s->sets.push_back(std::make_shared<const PartSet<>>(p.partSets[i]));
	}
	s->cuts = prev && VecEq(*prev->cuts, p.cutOuts, CutEq) ? prev->cuts : std::make_shared<const std::vector<CutOut<>>>(p.cutOuts);
	s->shapes = prev && VecEq(*prev->shapes, p.shapes, ShapeEq) ? prev->shapes : std::make_shared<const std::vector<Shape<>>>(p.shapes);
	s->gfx = prev && VecEq(*prev->gfx, p.gfxMeta, GfxEq) ? prev->gfx : std::make_shared<const std::vector<PartGfx<>>>(p.gfxMeta);
	return s;
}

bool PartsUndoChannel::equalsLive(const Snap& snap) const
{
	auto s = std::static_pointer_cast<const PartsSnap>(snap);
	const Parts& p = *m_parts;
	if (!s || s->loaded != p.loaded || s->sets.size() != p.partSets.size()) return false;
	for (size_t i = 0; i < p.partSets.size(); i++) if (!SetEq(*s->sets[i], p.partSets[i])) return false;
	return VecEq(*s->cuts, p.cutOuts, CutEq) && VecEq(*s->shapes, p.shapes, ShapeEq) && VecEq(*s->gfx, p.gfxMeta, GfxEq);
}

void PartsUndoChannel::restore(const Snap& snap)
{
	auto s = std::static_pointer_cast<const PartsSnap>(snap);
	if (!s || !m_parts) return;
	Parts& p = *m_parts;
	const bool sheets = !VecEq(*s->gfx, p.gfxMeta, GfxEq);
	p.partSets.resize(s->sets.size());
	for (size_t i = 0; i < s->sets.size(); i++) if (!SetEq(*s->sets[i], p.partSets[i])) p.partSets[i] = *s->sets[i];
	if (!VecEq(*s->cuts, p.cutOuts, CutEq)) p.cutOuts = *s->cuts;
	if (!VecEq(*s->shapes, p.shapes, ShapeEq)) p.shapes = *s->shapes;
	if (sheets) {
		// keep the GL texture ids of records whose pixels did not change (re-upload only when needed)
		std::vector<PartGfx<>> g = *s->gfx;
		for (size_t i = 0; i < g.size() && i < p.gfxMeta.size(); i++)
			if (g[i].data == p.gfxMeta[i].data && g[i].w == p.gfxMeta[i].w) g[i].textureIndex = p.gfxMeta[i].textureIndex;
		p.gfxMeta = std::move(g);
		if (onSheetsRestored) onSheetsRestored();
	}
	if (onRestored) onRestored();
}

size_t PartsUndoChannel::approxBytes(const Snap& snap) const
{
	auto s = std::static_pointer_cast<const PartsSnap>(snap);
	if (!s) return 0;
	size_t b = sizeof(PartsSnap) + s->sets.size() * sizeof(void*) * 2;
	for (auto& ps : s->sets) b += sizeof(PartSet<>) + ps->groups.size() * sizeof(PartProperty);
	b += s->cuts->size() * sizeof(CutOut<>) + s->shapes->size() * sizeof(Shape<>) + s->gfx->size() * sizeof(PartGfx<>);
	return b;
}
