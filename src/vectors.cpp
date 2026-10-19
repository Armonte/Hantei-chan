// .CG loader
//
// .CG contains information about sprite mappings from the ENC and PVR tiles.

#include "vectors.h"
#include "misc.h"

#include <windows.h>
#include <string>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>
#include <limits>
#include <filesystem>

bool VectorTXT::GetPrimaryMotion(int id,SubVector& motion) const {
	if(!loaded || !vector_list || !sub_vector_list || id<0 || id>=256) return false;
	const auto& v=vector_list->vectors[id];
	if(v.id!=id || v.VecNum<0 || v.VecNum>=256) return false;
	motion=sub_vector_list->subvectors[v.VecNum]; return motion.id==v.VecNum;
}

bool VectorTXT::load(const char* name) {
	if (loaded)
	{
		free();
	}

	char* data;
	unsigned int size;

	if (!ReadInMem(name, data, size)) {
		return 0;
	}

	// verify size and header
	if (size < 17 || memcmp(data, "\x2f\x2f\x20\x83\x78\x83\x4e\x83\x67\x83\x8b\x83\x8a\x83\x58\x83\x67", 17)) {
		delete[] data;

		return 0;
	}

	// Parsing advances data byte-by-byte, so retain the allocation base for
	// release after the two lookup tables have been populated.
	std::unique_ptr<char[]> fileData(data);
	char* const end = data + size;
	auto has = [&](size_t count) { return data <= end && static_cast<size_t>(end - data) >= count; };
	auto parseNumber = [](const std::string& text, int& value) {
		if (text.empty()) return false;
		try {
			size_t consumed = 0;
			const long parsed = std::stol(text, &consumed, 10);
			if (consumed != text.size() || parsed < (std::numeric_limits<int>::min)() ||
				parsed > (std::numeric_limits<int>::max)()) return false;
			value = static_cast<int>(parsed);
			return true;
		} catch (...) { return false; }
	};

	sub_vector_list = new SubVectorList{};
	int* svlp = (int*)sub_vector_list; //sub_vector_list_pointer
	bool continuous = false;
	int curNumIndex = 0;
	int curSubVecIndex = 0;
	int subIndex = 0;
	int intCurNum = 0;
	std::string curNum = "";
	bool comment = false;
	while (has(3) && memcmp(data, "END", 3))
	{
		if (!comment && ((memcmp(data, "\x2f", 1) > 0 && memcmp(data, "\x3A", 1) < 0) || !memcmp(data, "\x2d", 1))) //char 0 - 9
		{
			continuous = true;
			curNum += data[0];
		}
		else if (!memcmp(data, "\x2f\x2f", 2))
		{
			comment = true;
		}
		else if (!memcmp(data, "\x0d", 1) || !memcmp(data, "\x0a", 1))
		{
			comment = false;
		}
		else
		{
			if (continuous)
			{
				if (!parseNumber(curNum, intCurNum)) { curNum.clear(); continuous = false; data++; continue; }
				if (subIndex == 0)
				{
					curSubVecIndex = intCurNum;
				}
				// vector.txt is extensible and some files append unrelated numeric
				// tables after the standard records. Never use their IDs as indexes
				// into Gonptechan's fixed 256-entry compatibility table.
				if (curSubVecIndex >= 0 && curSubVecIndex < 256 && subIndex >= 0 && subIndex < 5)
					svlp[curSubVecIndex * 5 + subIndex] = intCurNum;
				subIndex++;
				curNum = "";
				if (subIndex > 4)
				{
					subIndex = 0;
				}
			}
			continuous = false;
		}
		data++;
	}

	if (!has(3)) { free(); return false; }
	vector_list = new VectorList{};
	for (auto& vector : vector_list->vectors) vector.id = -1;
	int* vlp = (int*)vector_list; //vector_list_pointer
	curNum = "";
	comment = false;
	subIndex = 0;
	intCurNum = 0;
	int curVecCnt = 0;
	int curSubVec = 0;
	int curVecIndex = 0;
	bool isName = false;
	data += 2;
	while (data < end) {
		++data;
		if (!has(3) || !memcmp(data, "END", 3)) break;
		if (!memcmp(data, "\x2f\x2f", 2)) // '//'
		{
			comment = true;
		}
		else if (!memcmp(data, "\x0a", 1)) // '\n'
		{
			comment = false;
		}

		if (comment) continue;

		//if ((memcmp(data, "\x2f", 1) > 0 && memcmp(data, "\x3A", 1) < 0) || !memcmp(data, "\x2d", 1)) //'0' - '9' and '-'
		if (memcmp(data, "\x20", 1) && memcmp(data, "\x0a", 1) && memcmp(data, "\x0d", 1))
		{
			continuous = true;
			if (!isName && ((memcmp(data, "\x2f", 1) > 0 && memcmp(data, "\x3A", 1) < 0) || !memcmp(data, "\x2d", 1))) //'0' - '9' and '-'
				curNum += data[0];
			else
				isName = true;
		}
		//else if (!memcmp(data, "\x20", 1) || !memcmp(data, "\x0a", 1)) // ' ' and '\n'
		else
		{
			if (continuous && !isName)
			{
				if (!parseNumber(curNum, intCurNum)) { curNum.clear(); continuous = false; continue; }
				if (subIndex == 0)
				{
					curVecIndex = intCurNum;
				}
				if (subIndex == 1)
				{
					curVecCnt = intCurNum;
				}
				if (curVecIndex >= 0 && curVecIndex < 256 && subIndex >= 0 && subIndex < 22)
					vlp[curVecIndex * 22 + subIndex] = intCurNum;
				subIndex++;
				curNum = "";
			}

			if (!memcmp(data, "\x0a", 1)) // '\n'
			{
				if (curVecCnt > 0)
				{
					curSubVec++;
					subIndex = 6 * curSubVec;
					curVecCnt--;
				}
				else
				{
					subIndex = 0;
					curSubVec = 0;
				}
			}

			isName = false;
			continuous = false;
		}
	}
	if (!has(3)) { free(); return false; }

	loaded = true;
	loadedPath=name;

	return 1;
}

void VectorTXT::free() {
	loadedPath.clear();
	if (sub_vector_list) {
		delete sub_vector_list;
	}
	sub_vector_list = nullptr;
	if (vector_list) {
		delete vector_list;
	}
	vector_list = nullptr;
	loaded = false;
}

VectorTXT::VectorTXT() {
	loaded = false;
	sub_vector_list = 0;
	vector_list = 0;
}

VectorTXT::~VectorTXT() {
	free();
}

