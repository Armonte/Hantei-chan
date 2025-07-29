// .CG loader
//
// .CG contains information about sprite mappings from the ENC and PVR tiles.

#include "vectors.h"
#include "misc.h"

#include <windows.h>
#include <string>

#include <cstdlib>
#include <cstring>

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
	if (memcmp(data, "\x2f\x2f\x20\x83\x78\x83\x4e\x83\x67\x83\x8b\x83\x8a\x83\x58\x83\x67", 17)) {
		delete[] data;

		return 0;
	}

	sub_vector_list = new SubVectorList();
	int* svlp = (int*)sub_vector_list; //sub_vector_list_pointer
	bool continuous = false;
	int curNumIndex = 0;
	int curSubVecIndex = 0;
	int subIndex = 0;
	int intCurNum = 0;
	std::string curNum = "";
	bool comment = false;
	while (memcmp(data, "END", 3))
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
				intCurNum = std::stoi(curNum);
				if (subIndex == 0)
				{
					curSubVecIndex = intCurNum;
				}
				svlp[curSubVecIndex * 5 + subIndex] = std::stoi(curNum);
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

	vector_list = new VectorList();
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
	while (memcmp(++data, "END", 3))
	{
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
				intCurNum = std::stoi(curNum);
				if (subIndex == 0)
				{
					curVecIndex = intCurNum;
				}
				if (subIndex == 1)
				{
					curVecCnt = intCurNum;
				}
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

	loaded = true;

	return 1;
}

void VectorTXT::free() {
	if (sub_vector_list) {
		delete[] sub_vector_list;
	}
	sub_vector_list = nullptr;
	if (vector_list) {
		delete[] vector_list;
	}
	vector_list = nullptr;
}

VectorTXT::VectorTXT() {
	loaded = false;
	sub_vector_list = 0;
	vector_list = 0;
}

VectorTXT::~VectorTXT() {
	free();
}

