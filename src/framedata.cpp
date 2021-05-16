#include "framedata.h"
#include "framedata_load.h"
#include <fstream>
#include "misc.h"
#include <cstring>
#include <sstream>
#include <iomanip>

Sequence::Sequence():
psts(0), level(0), flag(0),
empty(true),
initialized(false)
{}

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

	char header[32] = "Hantei6DataFile";

	//Set special byte to know if the file was written by our tool
	header[31] = 0xFF; 

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

			ss << m_sequences[n].name;
			if(m_sequences[n].name.empty() && !noFrames)
			{
					ss << u8"Untitled";
			}
		}
		
		return ss.str();
}

FrameData::FrameData() {
	m_nsequences = 0;
	m_loaded = 0;
}

FrameData::~FrameData() {
	Free();
}
