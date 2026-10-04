#ifndef CMDFILE_CMD_FRAMEDATA_H_GUARD
#define CMDFILE_CMD_FRAMEDATA_H_GUARD

#include <string>

class FrameData;

namespace cmdfile {

class Document;

// Rebuilds FrameData::m_commands from a parsed command file (active commands only; the
// first definition of a duplicated ID wins, as in MBAACC).
void ApplyCommandTable(FrameData& frameData, const Document& doc, const std::string& path);

} // namespace cmdfile

#endif
