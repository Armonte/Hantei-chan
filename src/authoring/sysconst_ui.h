#ifndef AUTHORING_SYSCONST_UI_H_GUARD
#define AUTHORING_SYSCONST_UI_H_GUARD

// "System constants" window: edit MBAACC data\_type.txt and data\vector.txt in place (sysconst_model.h),
// with values that differ from a vanilla folder highlighted and, for hit vectors, how many frames of the
// open characters use each one.

#include "sysconst_model.h"

#include <functional>
#include <string>

namespace sysconst {

class Window {
public:
	bool open = false;
	// Hit-vector id -> number of frames (in the open characters) whose ATVV/ATHV/ATGV use it.
	std::function<int(int)> vectorUses;

	bool openFolder(const std::string& dataDirUtf8, std::string* error);
	void draw();
	bool dirty() const { return m_type.dirty() || m_vec.dirty(); }

private:
	void drawType();
	void drawVector();
	bool save();

	std::string m_dir, m_vanillaDir, m_status, m_filter;
	bool m_statusError = false;
	TypeFile m_type, m_typeBase;
	VectorFile m_vec, m_vecBase;
	bool m_hasType = false, m_hasVec = false, m_hasBase = false;
	bool m_onlyChanged = false;
};

} // namespace sysconst

#endif
