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

	// Detect file format
	bool is_ha4 = false;
	bool is_ha4_binary = false;
	bool is_ha6 = false;

	// Check HA6 header (MBAACC/UNI/Dengeki)
	if (!memcmp(data, "Hantei6DataFile", 15)) {
		is_ha6 = true;
	}
	// Check HA4 binary header (Act Cadenza distribution .DAT files)
	else if (size > 64 && !memcmp(data, "Hantei4", 7)) {
		// Binary .DAT format - compressed with offset tables
		is_ha4_binary = true;
		std::cout << "Detected HA4 binary .DAT format (compressed)\n";
	}
	// Check HA4 tag-based header (Act Cadenza editor exports)
	// HA4 files don't have a clear signature like HA6
	// Instead, check for _STR tag at start (after any potential header)
	// or check for PSTR pattern structure
	else if (size > 32 && !memcmp(data, "_STR", 4)) {
		// Likely HA4 format - starts directly with _STR
		is_ha4 = true;
	}
	else if (size > 32 && (!memcmp(data + 0x20, "_STR", 4) || !memcmp(data + 0x10, "_STR", 4))) {
		// HA4 format with possible header
		is_ha4 = true;
	}

	// If neither format detected, try to detect by looking for PSTR tags
	if (!is_ha6 && !is_ha4 && !is_ha4_binary) {
		for (unsigned int offset = 0; offset < std::min(size, 256u); offset += 4) {
			if (!memcmp(data + offset, "PSTR", 4)) {
				// Found pattern start - likely HA4 or HA6
				// Check if there's a HA6 header before it
				if (offset >= 0x20 && !memcmp(data, "Hantei6DataFile", 15)) {
					is_ha6 = true;
				} else {
					is_ha4 = true;
				}
				break;
			}
		}
	}

	if (!is_ha6 && !is_ha4 && !is_ha4_binary) {
		std::cerr << "Error: Unknown file format (not HA4 or HA6)\n";
		delete[] data;
		return 0;
	}

	test.filename = filename;

	// Load HA6 format (MBAACC/UNI/Dengeki)
	if (is_ha6) {
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

		unsigned int sequence_count = d[1];

		if(!patch)
			Free();

		if(sequence_count > m_nsequences)
			m_sequences.resize(sequence_count);
		m_nsequences = sequence_count;

		d += 2;
		// parse and recursively store data
		d = fd_main_load(d, d_end, m_sequences, m_nsequences, utf8);
	}
	// Load HA4 format (Act Cadenza)
	else if (is_ha4) {
		std::cout << "Detected HA4 format (Act Cadenza) - converting to HA6...\n";

		// HA4 files may or may not have a header
		// Try different offsets
		unsigned int *d = nullptr;
		unsigned int *d_end = (unsigned int *)(data + size);

		// Check for _STR at various offsets
		if (!memcmp(data, "_STR", 4)) {
			d = (unsigned int *)data;
		} else if (!memcmp(data + 0x10, "_STR", 4)) {
			d = (unsigned int *)(data + 0x10);
		} else if (!memcmp(data + 0x20, "_STR", 4)) {
			d = (unsigned int *)(data + 0x20);
		} else {
			std::cerr << "Error: Could not find _STR tag in HA4 file\n";
			delete[] data;
			return 0;
		}

		if (memcmp(d, "_STR", 4)) {
			delete[] data;
			return 0;
		}

		unsigned int sequence_count = d[1];

		if(!patch)
			Free();

		if(sequence_count > m_nsequences)
			m_sequences.resize(sequence_count);
		m_nsequences = sequence_count;

		d += 2;
		// parse HA4 data and convert to HA6 internal format
		d = fd_main_load_ha4(d, d_end, m_sequences, m_nsequences);

		std::cout << "HA4 import complete - loaded " << sequence_count << " sequences\n";
	}
	// Load HA4 binary format (Act Cadenza distribution .DAT files)
	else if (is_ha4_binary) {
		std::cout << "Detected HA4 binary .DAT format (Act Cadenza) - decompressing...\n";

		// Binary .DAT files have "Hantei4" header
		unsigned int *d = (unsigned int *)data;
		unsigned int *d_end = (unsigned int *)(data + size);

		if(!patch)
			Free();

		// Binary format has 256 sequences (offset table based)
		m_nsequences = 256;
		m_sequences.resize(m_nsequences);

		// Parse binary .DAT and convert to HA6 internal format
		d = fd_main_load_ha4_binary(d, d_end, m_sequences, m_nsequences);

		std::cout << "HA4 binary import complete - loaded sequences\n";

		// Extract embedded CG data if present
		// File structure: header(68) + offset_table(1024) + anim_data + IMAGE_DATA + strings
		// IMAGE_DATA = HA4_wrapper + HA6_CG_data (starts with "BMP Cutter3")
		struct HA4Header {
			char magic[8];
			uint8_t padding[8];
			uint32_t version;
			uint32_t anim_data_size;
			uint32_t unknown;
			uint32_t anim_data_size2;
			uint32_t image_data_size;
		};

		const HA4Header *header = (const HA4Header*)data;
		if (header->image_data_size > 0) {
			std::cout << "HA4: Extracting embedded CG data (" << header->image_data_size << " bytes)...\n";

			// CG blob starts at anim_data_size offset
			const uint8_t *cg_blob_start = (const uint8_t*)data + header->anim_data_size;

			// Search for "BMP Cutter3" magic within the CG blob to verify it's HA6 format
			const char *bmp_magic = "BMP Cutter3";
			const uint8_t *ha6_cg_start = nullptr;
			size_t search_size = std::min((size_t)header->image_data_size, (size_t)2000000); // Search first 2MB

			for (size_t i = 0; i < search_size - 11; i++) {
				if (memcmp(cg_blob_start + i, bmp_magic, 11) == 0) {
					ha6_cg_start = cg_blob_start + i;
					std::cout << "HA4: Found HA6 CG data at offset +" << i << " into CG blob\n";
					break;
				}
			}

			if (ha6_cg_start != nullptr) {
				// IMPORTANT: The HA6 CG file has offset tables with indices RELATIVE TO THE CG BLOB START,
				// not relative to "BMP Cutter3". Even though "BMP Cutter3" might be 1MB into the blob,
				// the indices table expects to see all data from cg_blob_start.
				//
				// Solution: Extract the ENTIRE CG blob (from cg_blob_start), not just from "BMP Cutter3".
				// This ensures all offsets in the indices table are valid.

				size_t ha6_cg_size = header->image_data_size;

				std::cout << "HA4: Extracting full CG blob (" << ha6_cg_size << " bytes) to preserve offset tables...\n";

				// Save ENTIRE CG blob to temp file
				std::string temp_cg_path = std::string(filename) + ".tmp.cg";
				std::ofstream cg_file(temp_cg_path, std::ios::binary);
				if (cg_file.is_open()) {
					cg_file.write((const char*)cg_blob_start, ha6_cg_size);
					cg_file.close();

					// Store the temp CG path so CharacterInstance can load it
					m_extracted_cg_path = temp_cg_path;
					std::cout << "HA4: Saved CG blob to temporary file: " << temp_cg_path << "\n";
				} else {
					std::cerr << "HA4 Error: Could not write temporary CG file\n";
				}
			} else {
				std::cerr << "HA4 Warning: Could not find HA6 CG data (missing 'BMP Cutter3' magic)\n";
			}
		}
	}

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
	std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
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
	std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
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
					ss << u8"Untitled";
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

		// Parse the pattern/sequence number (after input, before flags)
		int pattern_index = -1;
		if(iss >> std::hex >> pattern_index >> std::dec) {
			// If we have a valid pattern index and a non-empty comment, set it as the pattern name
			if(pattern_index >= 0 && pattern_index < (int)m_sequences.size() && !commentPart.empty()) {
				// Only set name if sequence exists and name is empty
				if(!m_sequences[pattern_index].empty && m_sequences[pattern_index].name.empty()) {
					m_sequences[pattern_index].name = commentPart;
					m_sequences[pattern_index].codeName = commentPart;
				}
			}
		}

		cmd.comment = commentPart;
		m_commands.push_back(cmd);
	}

	file.close();
	std::cout << "Loaded " << m_commands.size() << " commands from " << filename << std::endl;
	std::cout << "Applied pattern names where available" << std::endl;
	return true;
}

FrameData::FrameData() {
	m_nsequences = 0;
	m_loaded = 0;
}

FrameData::~FrameData() {
	Free();
}
