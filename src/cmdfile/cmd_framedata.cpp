// FrameData's command table (used by the IF 11/31/35 panels to name command IDs) now comes
// from the lossless command-file parser. The old loader read ExComCheck keys such as
// "000_CheckNum = 60" as command 0 and cut CP932 lead bytes out of comments.

#include "../framedata.h"
#include "cmd_document.h"
#include "cmd_framedata.h"
#include "cmd_io.h"

namespace cmdfile {

void ApplyCommandTable(FrameData& frameData, const Document& doc, const std::string& path)
{
	frameData.m_commands.clear();
	frameData.m_commands.reserve(doc.commands.size());
	for (const auto& rec : doc.commands) {
		const auto id = ParseInteger(rec.fields[CF_Id]);
		if (!id) continue;
		Command cmd;
		cmd.id = static_cast<int>(*id);
		cmd.input = rec.fields[CF_Input];
		cmd.comment = Cp932ToUtf8(rec.comment);
		cmd.pattern = static_cast<int>(ParseInteger(rec.fields[CF_Pattern]).value_or(-1));
		cmd.teamSolo = static_cast<int>(ParseInteger(rec.fields[CF_TeamSolo]).value_or(0));
		frameData.m_commands.push_back(std::move(cmd));
	}
	frameData.m_commandsPath = path;
}

} // namespace cmdfile

bool FrameData::load_commands(const char* filename)
{
	std::string bytes;
	if (!filename || !cmdfile::ReadFileBytes(filename, bytes)) return false;
	cmdfile::ApplyCommandTable(*this, cmdfile::Document::parse(bytes), filename);
	return true;
}
