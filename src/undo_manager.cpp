#include "undo_manager.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Content comparison
// ---------------------------------------------------------------------------
// Plain-data blocks (AS, AT, EF, IF, hitbox coordinates) are compared
// bytewise. They are trivially copyable, so a snapshot copy reproduces their
// bytes exactly; the only possible error is a spurious "changed" when a whole
// struct is overwritten with equal values but different padding, which merely
// records a no-op step. A real change can never be missed.
//
// Layer, Frame_AF and Sequence are compared field by field (layers because of
// the empty-layer normalisation below). The static_asserts below fire when a
// member is added to any of them, so a new field cannot silently fall out of
// change detection.
static_assert(std::is_trivially_copyable<Frame_AS>::value, "Frame_AS must stay trivially copyable for undo diffing");
static_assert(std::is_trivially_copyable<Frame_AT>::value, "Frame_AT must stay trivially copyable for undo diffing");
static_assert(std::is_trivially_copyable<Frame_EF>::value, "Frame_EF must stay trivially copyable for undo diffing");
static_assert(std::is_trivially_copyable<Frame_IF>::value, "Frame_IF must stay trivially copyable for undo diffing");
static_assert(std::is_trivially_copyable<Hitbox>::value, "Hitbox must stay trivially copyable for undo diffing");
#if defined(__x86_64__) || defined(_M_X64)
static_assert(sizeof(Layer_Type) == 60, "Layer layout changed: update layerEquals() in undo_manager.cpp, then this size");
static_assert(sizeof(Frame_AF) == 80, "Frame_AF layout changed: update afEquals() in undo_manager.cpp, then this size");
static_assert(sizeof(Sequence) == 112, "Sequence layout changed: update SequenceContentEquals() in undo_manager.cpp, then this size");
#endif
static_assert(sizeof(Frame) == sizeof(Frame_AF) + sizeof(Frame_AS) + sizeof(Frame_AT)
	+ sizeof(Frame::EF) + sizeof(Frame::IF) + sizeof(BoxList),
	"Frame gained a member: update frameEquals() in undo_manager.cpp");

namespace {

template<typename T>
bool bytesEqual(const T& a, const T& b)
{
	return std::memcmp(&a, &b, sizeof(T)) == 0;
}

template<typename Vec>
bool podVectorEqual(const Vec& a, const Vec& b)
{
	if (a.size() != b.size()) return false;
	if (a.empty()) return true;
	return std::memcmp(a.data(), b.data(), sizeof(a[0]) * a.size()) == 0;
}

bool layerEquals(const Layer_Type& a, const Layer_Type& b)
{
	return a.spriteId == b.spriteId
		&& a.usePat == b.usePat
		&& a.offset_y == b.offset_y
		&& a.offset_x == b.offset_x
		&& a.blend_mode == b.blend_mode
		&& std::memcmp(a.rgba, b.rgba, sizeof(a.rgba)) == 0
		&& std::memcmp(a.rotation, b.rotation, sizeof(a.rotation)) == 0
		&& std::memcmp(a.scale, b.scale, sizeof(a.scale)) == 0
		&& a.priority == b.priority;
}

// The renderer and the AF panel give a keyframe with no layers one default
// layer just by displaying it. Treat "no layers" and "one default layer" as
// the same content, otherwise viewing such a keyframe would look like an edit
// (and re-appear after every undo of it).
bool layersEqual(const Frame_AF& a, const Frame_AF& b)
{
	static const Layer_Type defaultLayer{};
	const auto& la = a.layers;
	const auto& lb = b.layers;
	if (la.size() == lb.size()) {
		for (size_t i = 0; i < la.size(); ++i)
			if (!layerEquals(la[i], lb[i])) return false;
		return true;
	}
	if (la.empty() && lb.size() == 1) return layerEquals(lb[0], defaultLayer);
	if (lb.empty() && la.size() == 1) return layerEquals(la[0], defaultLayer);
	return false;
}

bool afEquals(const Frame_AF& a, const Frame_AF& b)
{
	return layersEqual(a, b)
		&& a.jump == b.jump
		&& a.duration == b.duration
		&& a.aniType == b.aniType
		&& a.aniFlag == b.aniFlag
		&& a.landJump == b.landJump
		&& a.interpolationType == b.interpolationType
		&& a.priority == b.priority
		&& a.loopCount == b.loopCount
		&& a.loopEnd == b.loopEnd
		&& a.AFRT == b.AFRT
		&& a.frameId == b.frameId
		&& std::memcmp(a.param, b.param, sizeof(a.param)) == 0
		&& a.afjh == b.afjh;
}

bool boxesEqual(const BoxList& a, const BoxList& b)
{
	if (a.size() != b.size()) return false;
	auto ia = a.begin();
	auto ib = b.begin();
	for (; ia != a.end(); ++ia, ++ib) {
		if (ia->first != ib->first) return false;
		if (!bytesEqual(ia->second, ib->second)) return false;
	}
	return true;
}

bool frameEquals(const Frame& a, const Frame& b)
{
	return bytesEqual(a.AS, b.AS)
		&& bytesEqual(a.AT, b.AT)
		&& afEquals(a.AF, b.AF)
		&& podVectorEqual(a.EF, b.EF)
		&& podVectorEqual(a.IF, b.IF)
		&& boxesEqual(a.hitboxes, b.hitboxes);
}

} // namespace

bool UndoManager::SequenceContentEquals(const Sequence& a, const Sequence& b)
{
	// Sequence::modified is bookkeeping ("changed since load"), not content.
	if (a.frames.size() != b.frames.size()) return false;
	if (a.psts != b.psts || a.level != b.level || a.flag != b.flag || a.pups != b.pups) return false;
	if (a.empty != b.empty || a.initialized != b.initialized) return false;
	if (a.usedAFGX != b.usedAFGX || a.usedATV2 != b.usedATV2) return false;
	if (a.name != b.name || a.codeName != b.codeName) return false;
	for (size_t i = 0; i < a.frames.size(); ++i) {
		if (!frameEquals(a.frames[i], b.frames[i])) return false;
	}
	return true;
}

size_t UndoManager::ApproxSequenceBytes(const Sequence& s)
{
	size_t bytes = sizeof(Sequence) + s.name.capacity() + s.codeName.capacity();
	for (const auto& f : s.frames) {
		bytes += sizeof(Frame);
		bytes += f.AF.layers.capacity() * sizeof(Layer_Type);
		bytes += f.EF.capacity() * sizeof(Frame_EF);
		bytes += f.IF.capacity() * sizeof(Frame_IF);
		bytes += f.hitboxes.size() * (sizeof(Hitbox) + 48); // node overhead estimate
	}
	return bytes;
}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

void UndoManager::attach(std::vector<Sequence>* document)
{
	if (document == m_doc) return;
	m_doc = document;
	reset();
}

void UndoManager::reset()
{
	m_hasBaseline = false;
	m_shadow.clear();
	m_loaded.clear();
	m_undo.clear();
	m_redo.clear();
	m_baseRevision = 0;
	m_savedRevision = 0;
	m_nextId = 1;
	m_dirty = false;
	m_prevGesture = false;
	m_txDepth = 0;
	m_txLabel.clear();
	m_stepFocusLatched = false;
}

bool UndoManager::ensureBaseline()
{
	if (!m_doc) return false;
	if (m_hasBaseline) return true;
	m_shadow.clear();
	m_shadow.reserve(m_doc->size());
	for (const auto& seq : *m_doc)
		m_shadow.push_back(std::make_shared<const Sequence>(seq));
	m_loaded = m_shadow;
	m_hasBaseline = true;
	return true;
}

void UndoManager::clear()
{
	const bool wasClean = isClean();
	m_undo.clear();
	m_redo.clear();
	m_baseRevision = m_nextId++;
	// The saved state stays reachable only if we are sitting on it
	// (revision 0 can never be current again).
	m_savedRevision = wasClean ? m_baseRevision : 0;
}

// ---------------------------------------------------------------------------
// Grouping
// ---------------------------------------------------------------------------

void UndoManager::noteFocus(int pattern, int frame)
{
	m_focusPattern = pattern;
	m_focusFrame = frame;
}

void UndoManager::markModified()
{
	if (!m_stepFocusLatched) {
		m_stepFocusPattern = m_focusPattern;
		m_stepFocusFrame = m_focusFrame;
		m_stepFocusLatched = true;
	}
	m_dirty = true;
}

void UndoManager::beginTransaction(const char* label)
{
	ensureBaseline();
	if (m_txDepth == 0) {
		// Anything edited before the gesture is its own step.
		if (m_dirty) commitInternal(nullptr);
		m_txLabel = label ? label : "";
		m_stepFocusPattern = m_focusPattern;
		m_stepFocusFrame = m_focusFrame;
		m_stepFocusLatched = true;
	}
	++m_txDepth;
}

bool UndoManager::commitTransaction()
{
	if (m_txDepth <= 0) return false;
	if (--m_txDepth > 0) return false;
	std::string label;
	label.swap(m_txLabel);
	return commitInternal(label.empty() ? nullptr : label.c_str());
}

void UndoManager::cancelTransaction()
{
	if (m_txDepth <= 0) return;
	m_txDepth = 0;
	m_txLabel.clear();
	if (m_doc && m_hasBaseline) {
		Entry pending;
		pending.changes = diff();
		pending.countBefore = m_shadow.size();
		pending.countAfter = m_doc->size();
		if (!pending.changes.empty() || pending.countBefore != pending.countAfter)
			applySide(pending, false);
	}
	m_dirty = false;
	m_stepFocusLatched = false;
}

bool UndoManager::endFrame(bool gestureActive)
{
	if (!ensureBaseline()) return false;

	const bool fallingEdge = m_prevGesture && !gestureActive;
	if (gestureActive && !m_prevGesture && !m_stepFocusLatched) {
		m_stepFocusPattern = m_focusPattern;
		m_stepFocusFrame = m_focusFrame;
		m_stepFocusLatched = true;
	}
	m_prevGesture = gestureActive;

	if (gestureActive || m_txDepth > 0)
		return false;
	if (!m_dirty && !fallingEdge)
		return false;
	return commitInternal(nullptr);
}

bool UndoManager::flush(const char* label)
{
	if (!ensureBaseline()) return false;
	return commitInternal(label);
}

std::vector<UndoManager::PatternDelta> UndoManager::diff() const
{
	std::vector<PatternDelta> out;
	if (!m_doc) return out;
	const size_t live = m_doc->size();
	const size_t shadow = m_shadow.size();
	const size_t common = std::min(live, shadow);
	for (size_t i = 0; i < common; ++i) {
		const Sequence& cur = (*m_doc)[i];
		const SeqPtr& old = m_shadow[i];
		if (old && SequenceContentEquals(cur, *old)) continue;
		PatternDelta d;
		d.index = (int)i;
		d.before = old;
		d.after = std::make_shared<const Sequence>(cur);
		out.push_back(std::move(d));
	}
	for (size_t i = common; i < live; ++i) {
		PatternDelta d;
		d.index = (int)i;
		d.after = std::make_shared<const Sequence>((*m_doc)[i]);
		out.push_back(std::move(d));
	}
	for (size_t i = common; i < shadow; ++i) {
		PatternDelta d;
		d.index = (int)i;
		d.before = m_shadow[i];
		out.push_back(std::move(d));
	}
	return out;
}

bool UndoManager::commitInternal(const char* label)
{
	m_dirty = false;
	const bool latched = m_stepFocusLatched;
	m_stepFocusLatched = false;
	if (!m_doc || !m_hasBaseline) return false;

	Entry e;
	e.changes = diff();
	e.countBefore = m_shadow.size();
	e.countAfter = m_doc->size();
	if (e.changes.empty() && e.countBefore == e.countAfter)
		return false;

	e.id = m_nextId++;
	if (label) e.label = label;
	e.focusPattern = latched ? m_stepFocusPattern : m_focusPattern;
	e.focusFrame = latched ? m_stepFocusFrame : m_focusFrame;

	m_shadow.resize(e.countAfter);
	for (const auto& d : e.changes) {
		if ((size_t)d.index < m_shadow.size()) {
			m_shadow[d.index] = d.after;
			updateModifiedFlag(d.index);
		}
	}
	// If the focus pattern was not touched (edit came from elsewhere, e.g. a
	// cross-pattern paste), point navigation at the first changed pattern.
	bool focusTouched = false;
	for (const auto& d : e.changes)
		if (d.index == e.focusPattern) { focusTouched = true; break; }
	if (!focusTouched && !e.changes.empty()) {
		e.focusPattern = e.changes.front().index;
		e.focusFrame = -1;
	}

	m_redo.clear();
	pushEntry(std::move(e));
	return true;
}

void UndoManager::pushEntry(Entry&& e)
{
	m_undo.push_back(std::move(e));
	trim();
}

void UndoManager::trim()
{
	while (m_undo.size() > m_maxLevels) {
		m_baseRevision = m_undo.front().id;
		m_undo.erase(m_undo.begin());
	}
}

void UndoManager::updateModifiedFlag(size_t index)
{
	if (!m_doc || index >= m_doc->size()) return;
	const bool changedSinceLoad = index >= m_loaded.size() || index >= m_shadow.size()
		|| m_shadow[index] != m_loaded[index];
	(*m_doc)[index].modified = changedSinceLoad;
}

void UndoManager::applySide(const Entry& e, bool toAfter)
{
	const size_t n = toAfter ? e.countAfter : e.countBefore;
	if (m_doc->size() < n) m_doc->resize(n);
	if (m_shadow.size() < n) m_shadow.resize(n);
	for (const auto& d : e.changes) {
		if (d.index < 0 || (size_t)d.index >= n) continue;
		const SeqPtr& src = toAfter ? d.after : d.before;
		if (src) {
			(*m_doc)[d.index] = *src;
			m_shadow[d.index] = src;
		} else {
			(*m_doc)[d.index] = Sequence{};
			m_shadow[d.index] = std::make_shared<const Sequence>();
		}
	}
	m_doc->resize(n);
	m_shadow.resize(n);
	for (const auto& d : e.changes)
		if (d.index >= 0 && (size_t)d.index < n) updateModifiedFlag(d.index);
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

void UndoManager::settlePending()
{
	if (m_dirty) {
		// A notified edit whose gesture has not ended yet (e.g. Ctrl+Z pressed
		// before the release frame) becomes its own step, so it is what gets
		// undone.
		commitInternal(nullptr);
		return;
	}
	// Unnotified drift that no gesture boundary has picked up yet is not a
	// user step (e.g. display-time normalisation). Absorb it into the shadow
	// without an entry so it can never shadow the real history.
	for (auto& d : diff()) {
		if ((size_t)d.index < m_shadow.size() && d.after) m_shadow[d.index] = d.after;
	}
	if (m_doc && m_shadow.size() != m_doc->size()) {
		// Pattern-list size drift is always recorded.
		commitInternal(nullptr);
	}
	m_stepFocusLatched = false;
}

const UndoManager::Entry* UndoManager::undo()
{
	if (!ensureBaseline()) return nullptr;
	settlePending();
	if (m_undo.empty()) return nullptr;
	Entry e = std::move(m_undo.back());
	m_undo.pop_back();
	applySide(e, false);
	m_redo.push_back(std::move(e));
	return &m_redo.back();
}

const UndoManager::Entry* UndoManager::redo()
{
	if (!ensureBaseline()) return nullptr;
	// A pending edit after an undo branches history: it is committed (which
	// clears redo), so there is nothing to redo.
	settlePending();
	if (m_redo.empty()) return nullptr;
	Entry e = std::move(m_redo.back());
	m_redo.pop_back();
	applySide(e, true);
	m_undo.push_back(std::move(e));
	trim();
	return m_undo.empty() ? nullptr : &m_undo.back();
}

// ---------------------------------------------------------------------------
// Dirty state
// ---------------------------------------------------------------------------

uint64_t UndoManager::currentRevision() const
{
	return m_undo.empty() ? m_baseRevision : m_undo.back().id;
}

void UndoManager::markClean()
{
	if (ensureBaseline())
		commitInternal(nullptr);
	m_savedRevision = currentRevision();
}

size_t UndoManager::historyBytes() const
{
	std::unordered_set<const Sequence*> seen;
	for (const auto& s : m_shadow) seen.insert(s.get());
	size_t bytes = 0;
	auto count = [&](const SeqPtr& p) {
		if (p && seen.insert(p.get()).second) bytes += ApproxSequenceBytes(*p);
	};
	for (const auto& e : m_undo) for (const auto& d : e.changes) { count(d.before); count(d.after); }
	for (const auto& e : m_redo) for (const auto& d : e.changes) { count(d.before); count(d.after); }
	return bytes;
}
