#include "misc.h"
#include <windows.h>
#include <string>

bool ReadInMem(const char *filename, char *&data, unsigned int &size)
{
	auto file = CreateFileA(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if(file == INVALID_HANDLE_VALUE || GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		data = nullptr;
		size = 0;
		return false;
	}

	size = GetFileSize(file, nullptr);
	if(size != INVALID_FILE_SIZE)
	{
		data = new char[size];
		DWORD readBytes;
		if(!ReadFile(file, data, size, &readBytes, nullptr))
		{
			delete[] data;
			CloseHandle(file);
			data = nullptr;
			size = 0;
			return false;
		}
	}

	CloseHandle(file);
	return true;
}	


// Shift-JIS (CP932) <-> UTF-8 conversion using Windows API
// Much faster and smaller than maintaining a 3000+ line conversion table!

std::string sj2utf8(const std::string &input)
{
	if(input.empty())
		return std::string();

	// CP_ACP = 932 (Shift-JIS) on Japanese Windows, but we use 932 explicitly
	const UINT CP_SJIS = 932;

	// First convert Shift-JIS to UTF-16
	int wideLen = MultiByteToWideChar(CP_SJIS, 0, input.c_str(), (int)input.length(), nullptr, 0);
	if(wideLen == 0)
		return std::string(); // Conversion failed

	std::wstring wide(wideLen, 0);
	MultiByteToWideChar(CP_SJIS, 0, input.c_str(), (int)input.length(), &wide[0], wideLen);

	// Then convert UTF-16 to UTF-8
	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
	if(utf8Len == 0)
		return std::string(); // Conversion failed

	std::string output(utf8Len, 0);
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, &output[0], utf8Len, nullptr, nullptr);

	return output;
}

std::string utf82sj(const std::string &input)
{
	if(input.empty())
		return std::string();

	const UINT CP_SJIS = 932;

	// First convert UTF-8 to UTF-16
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.length(), nullptr, 0);
	if(wideLen == 0)
		return std::string(); // Conversion failed

	std::wstring wide(wideLen, 0);
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.length(), &wide[0], wideLen);

	// Then convert UTF-16 to Shift-JIS
	int sjisLen = WideCharToMultiByte(CP_SJIS, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
	if(sjisLen == 0)
		return std::string(); // Conversion failed

	std::string output(sjisLen, 0);
	WideCharToMultiByte(CP_SJIS, 0, wide.c_str(), wideLen, &output[0], sjisLen, nullptr, nullptr);

	return output;
}


