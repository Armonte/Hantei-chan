#include "framedata.h"
#include "framedata_load.h"
#include <fstream>
#include "misc.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iostream>

int maxCount = 0;
std::set<int> numberSet;

void FrameData::initEmpty()
{
	Free();
	m_nsequences = 1000;
	m_sequences.resize(m_nsequences);
	m_loaded = 1;
}

bool FrameData::load(const char *filename, bool patch) {
	// allow loading over existing data

	char *data;
	unsigned int size;

	if (!ReadInMem(filename, data, size)) {
		return 0;
	}

	// verify header
	if (memcmp(data, "Hantei6DataFile", 15)) {
		delete[] data;

		return 0;
	}

	// Check for legacy UTF-8 flag (old Hantei-chan set byte 31 to 0xFF for UTF-8 files)
	// Modern files always use Shift-JIS and don't set this flag
	bool utf8 = ((unsigned char*)data)[31] == 0xFF;

	// initialize the root
	unsigned int *d = (unsigned int *)(data + 0x20);
	unsigned int *d_end = (unsigned int *)(data + size);
	if (memcmp(d, "_STR", 4)) {
		delete[] data;
		return 0;
	}

	test.filename = filename;

	unsigned int sequence_count = d[1];

	if(!patch)
		Free();

	if(sequence_count > m_nsequences)
		m_sequences.resize(sequence_count);
	m_nsequences = sequence_count;

	d += 2;
	// parse and recursively store data
	d = fd_main_load(d, d_end, m_sequences, m_nsequences, utf8);

	// Clear modified flags after loading - only track NEW edits from this session
	for(auto& seq : m_sequences) {
		seq.modified = false;
	}

	// cleanup and finish
	delete[] data;

	m_loaded = 1;
	return 1;
}


#define VAL(X) ((const char*)&X)
#define PTR(X) ((const char*)X)

void FrameData::save(const char *filename)
{
	std::ofstream file(filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
	if (!file.is_open())
		return;

	for(auto& seq : m_sequences)
	for(auto &frame : seq.frames)
	for(auto it = frame.hitboxes.begin(); it != frame.hitboxes.end();)
	{
		Hitbox &box = it->second;
		//Delete degenerate boxes when exporting.
		if( (box.xy[0] == box.xy[2]) ||
			(box.xy[1] == box.xy[3]) )
		{
			frame.hitboxes.erase(it++);
		}
		else
		{
			//Fix inverted boxes. Don't know if needed.
			if(box.xy[0] > box.xy[2])
				std::swap(box.xy[0], box.xy[2]);
			if(box.xy[1] > box.xy[3])
				std::swap(box.xy[1], box.xy[3]);
			++it;
		}
	}

	char header[32] = "Hantei6DataFile";

	// Keep header in original format - no modification flag
	file.write(header, sizeof(header));

	uint32_t size = get_sequence_count();
	file.write("_STR", 4); file.write(VAL(size), 4);

	for(uint32_t i = 0; i < get_sequence_count(); i++)
	{
		file.write("PSTR", 4); file.write(VAL(i), 4);
		WriteSequence(file, &m_sequences[i]);
		file.write("PEND", 4);
	}

	file.write("_END", 4);
	file.close();
}

void FrameData::save_modified_only(const char *filename)
{
	std::ofstream file(filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
	if (!file.is_open())
		return;

	// Clean up hitboxes for modified sequences only
	for(auto& seq : m_sequences)
	{
		if(!seq.modified) continue;

		for(auto &frame : seq.frames)
		for(auto it = frame.hitboxes.begin(); it != frame.hitboxes.end();)
		{
			Hitbox &box = it->second;
			//Delete degenerate boxes when exporting.
			if( (box.xy[0] == box.xy[2]) ||
				(box.xy[1] == box.xy[3]) )
			{
				frame.hitboxes.erase(it++);
			}
			else
			{
				//Fix inverted boxes. Don't know if needed.
				if(box.xy[0] > box.xy[2])
					std::swap(box.xy[0], box.xy[2]);
				if(box.xy[1] > box.xy[3])
					std::swap(box.xy[1], box.xy[3]);
				++it;
			}
		}
	}

	char header[32] = "Hantei6DataFile";

	// Keep header in original format - no modification flag
	file.write(header, sizeof(header));

	uint32_t size = get_sequence_count();
	file.write("_STR", 4); file.write(VAL(size), 4);

	// Only write modified sequences
	for(uint32_t i = 0; i < get_sequence_count(); i++)
	{
		if(m_sequences[i].modified)
		{
			file.write("PSTR", 4); file.write(VAL(i), 4);
			WriteSequence(file, &m_sequences[i]);
			file.write("PEND", 4);
		}
	}

	file.write("_END", 4);
	file.close();
}

void FrameData::Free() {
	m_sequences.clear();
	m_nsequences = 0;
	m_loaded = 0;
}

int FrameData::get_sequence_count() {
	if (!m_loaded) {
		return 0;
	}
	return m_nsequences;
}

Sequence* FrameData::get_sequence(int n) {
	if (!m_loaded) {
		return 0;
	}
	
	if (n < 0 || (unsigned int)n >= m_nsequences) {
		return 0;
	}
	
	return &m_sequences[n];
}

std::string FrameData::GetDecoratedName(int n)
{
		std::stringstream ss;
		ss.flags(std::ios_base::right);

		ss << std::setfill('0') << std::setw(3) << n << " ";

		if(!m_sequences[n].empty)
		{
			bool noFrames = m_sequences[n].frames.empty();
			if(noFrames)
				ss << u8"〇 ";

			if(m_sequences[n].name.empty() && m_sequences[n].codeName.empty() && !noFrames)
			{
					ss << u8"---";
			}
		}

		// Strings are already stored as UTF-8 in memory (converted during load)
		ss << m_sequences[n].name;
		if(!m_sequences[n].codeName.empty())
			ss << " - " << m_sequences[n].codeName;

		// Add asterisk for modified patterns
		if(m_sequences[n].modified)
			ss << " *";

		return ss.str();
}

Command* FrameData::get_command(int id)
{
	for(auto &cmd : m_commands) {
		if(cmd.id == id)
			return &cmd;
	}
	return nullptr;
}

void FrameData::mark_modified(int sequence_index)
{
	if(sequence_index >= 0 && sequence_index < (int)m_sequences.size()) {
		m_sequences[sequence_index].modified = true;
	}
}

bool FrameData::load_commands(const char *filename)
{
	std::ifstream file(filename);
	if(!file.is_open()) {
		std::cout << "Failed to open command file: " << filename << std::endl;
		return false;
	}

	m_commands.clear();
	std::string line;
	int lineNum = 0;

	while(std::getline(file, line)) {
		lineNum++;

		// Skip empty lines and comment-only lines
		if(line.empty() || line[0] == '/' || line[0] == '#')
			continue;

		// Find the comment part (after //)
		size_t commentPos = line.find("//");
		std::string dataPart = (commentPos != std::string::npos) ? line.substr(0, commentPos) : line;
		std::string commentPart = (commentPos != std::string::npos) ? line.substr(commentPos + 2) : "";

		// Trim comment
		while(!commentPart.empty() && (commentPart[0] == ' ' || commentPart[0] == '\t' || commentPart[0] == '\xe3' || commentPart[0] == '\x80'))
			commentPart = commentPart.substr(1);
		while(!commentPart.empty() && (commentPart.back() == ' ' || commentPart.back() == '\t' || commentPart.back() == '\r' || commentPart.back() == '\n'))
			commentPart.pop_back();

		// Parse data part
		std::istringstream iss(dataPart);
		Command cmd;

		if(!(iss >> cmd.id >> cmd.input))
			continue; // Failed to parse ID and input

		cmd.comment = commentPart;
		m_commands.push_back(cmd);
	}

	file.close();
	std::cout << "Loaded " << m_commands.size() << " commands from " << filename << std::endl;
	return true;
}

FrameData::FrameData() {
	m_nsequences = 0;
	m_loaded = 0;
}

FrameData::~FrameData() {
	Free();
}
