#ifndef HAN2_TYPED_FILES_H_GUARD
#define HAN2_TYPED_FILES_H_GUARD

// Generic editor for the fixed-layout data files that are plain arrays of typed records (command tables, win quotes, CPU scripts, character tables ...).
// A file is a byte buffer plus a list of REGIONS (offset, stride, count, reflection table from the generated IDA types); edits write straight into the
// buffer, so an untouched file saves byte-identical. Enciphered files keep their deciphered working copy and say how to turn it back (toStored).
#include "han2/han2_reflect.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace han2ui {

struct TypedRegion {
	std::string title;
	size_t offset = 0, stride = 0, count = 0;
	const Han2FieldInfo *fields = nullptr; int nfields = 0;
	std::function<std::string(const uint8_t *rec, size_t index)> label;   // row caption (optional)
	std::string group;                                                      // regions with the same group are shown under one tree node
};
struct TypedFile {
	std::string kind;                                    // human readable file kind
	std::vector<TypedRegion> regions;
	std::vector<uint8_t> work;                           // the bytes the regions address (deciphered when the stored form is enciphered)
	std::function<void(const std::vector<uint8_t> &work, std::vector<uint8_t> &stored)> toStored;   // null = work is the stored form
	std::vector<std::vector<uint8_t>> keep;              // unused
};

// Recognises a file by name and size/structure; false = not a typed file (the viewer falls back to hex).
bool DescribeTypedFile(const std::string &nameCp932, const std::vector<uint8_t> &stored, TypedFile &out);
void TypedFileStored(const TypedFile &f, std::vector<uint8_t> &stored);

// Reflection record editor (han2_inspector.cpp): returns true when a byte changed.
bool EditRecordFields(const char *id, uint8_t *rec, const Han2FieldInfo *tbl, int n);

} // namespace han2ui
#endif
