#include "misc.h"
#include <windows.h>
#include <string>
#include <algorithm>
#include <cctype>

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

	// Clear any previous error state
	SetLastError(0);

	// First convert Shift-JIS to UTF-16
	// Use MB_ERR_INVALID_CHARS flag to detect invalid sequences
	int wideLen = MultiByteToWideChar(CP_SJIS, MB_ERR_INVALID_CHARS, input.c_str(), (int)input.length(), nullptr, 0);
	if(wideLen == 0) {
		DWORD error = GetLastError();
		// If it's just an invalid character error, try without the flag
		if(error == ERROR_NO_UNICODE_TRANSLATION) {
			SetLastError(0);
			wideLen = MultiByteToWideChar(CP_SJIS, 0, input.c_str(), (int)input.length(), nullptr, 0);
			if(wideLen == 0) {
				// Still failed, return empty
				return std::string();
			}
		} else {
			// Real error
			return std::string();
		}
	}

	std::wstring wide(wideLen, 0);
	int result1 = MultiByteToWideChar(CP_SJIS, 0, input.c_str(), (int)input.length(), &wide[0], wideLen);
	if(result1 == 0) {
		// Conversion failed on second call
		return std::string();
	}

	// Then convert UTF-16 to UTF-8
	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
	if(utf8Len == 0)
		return std::string(); // Conversion failed

	std::string output(utf8Len, 0);
	int result2 = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, &output[0], utf8Len, nullptr, nullptr);
	if(result2 == 0)
		return std::string(); // Conversion failed

	// Resize to actual converted length
	output.resize(result2);
	return output;
}

std::string utf82sj(const std::string &input)
{
	if(input.empty())
		return std::string();

	const UINT CP_SJIS = 932;

	// First convert UTF-8 to UTF-16
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.length(), nullptr, 0);
	if(wideLen == 0) {
		DWORD error = GetLastError();
		// Return empty string on conversion failure
		return std::string();
	}

	std::wstring wide(wideLen, 0);
	if(MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.length(), &wide[0], wideLen) == 0) {
		return std::string(); // Conversion failed
	}

	// Then convert UTF-16 to Shift-JIS
	// Shift-JIS symbols like ★ (0x819a), ☆ (0x8199), ● (0x819c) are valid and should convert correctly
	int sjisLen = WideCharToMultiByte(CP_SJIS, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
	if(sjisLen == 0)
		return std::string(); // Conversion failed

	std::string output(sjisLen, 0);
	int result = WideCharToMultiByte(CP_SJIS, 0, wide.c_str(), wideLen, &output[0], sjisLen, nullptr, nullptr);
	if(result == 0)
		return std::string(); // Conversion failed

	// Resize to actual converted length (result may be less than sjisLen)
	output.resize(result);
	return output;
}

// Normalize path separators for consistency
std::string normalizePath(const std::string& path)
{
	if (path.empty()) {
		return path;
	}
	
	std::string normalized = path;
	// Convert backslashes to forward slashes for consistency
	std::replace(normalized.begin(), normalized.end(), '\\', '/');
	// Remove trailing slashes for consistency (but keep root slashes)
	while (normalized.length() > 1 && normalized.back() == '/') {
		normalized.pop_back();
	}
	// Normalize drive letter to uppercase on Windows (e.g., "c:" -> "C:")
	if (normalized.length() >= 2 && normalized[1] == ':') {
		normalized[0] = std::toupper(normalized[0]);
	}
	return normalized;
}


