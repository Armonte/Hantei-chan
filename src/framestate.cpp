#include "framestate.h"
#include <tinyalloc.h>
#include <windows.h>

constexpr const wchar_t *sharedMemHandleName = L"hanteichan-shared_mem";

FrameState::FrameState()
{
	SYSTEM_INFO sInfo;
	GetSystemInfo(&sInfo);

	// 16MB for tinyalloc buffer
	size_t bufSize = 0x100 * sInfo.dwAllocationGranularity;
	// Additional space for CopyData struct at end
	size_t appendSize = (1 + sizeof(CopyData) / sInfo.dwAllocationGranularity) * sInfo.dwAllocationGranularity;

	sharedMemHandle = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		bufSize + appendSize,
		sharedMemHandleName);

	auto exists = GetLastError();

	// Base address for consistent mapping across instances
	void *baseAddress = (void*)((size_t)sInfo.lpMinimumApplicationAddress + sInfo.dwAllocationGranularity * 0x5000);

	sharedMem = MapViewOfFileEx(
		sharedMemHandle,
		FILE_MAP_ALL_ACCESS,
		0, 0,
		bufSize + appendSize,
		baseAddress);

	// Initialize tinyalloc with buffer
	// exists == ERROR_ALREADY_EXISTS means another instance already initialized it
	ta_init(sharedMem, (char*)sharedMem + bufSize, 65535, 256, 16, exists == ERROR_ALREADY_EXISTS);

	if (exists != ERROR_ALREADY_EXISTS) {
		// First instance - placement new CopyData at end of buffer
		copied = new((char*)sharedMem + bufSize) CopyData;
	} else {
		// Subsequent instances - reinterpret existing CopyData
		copied = reinterpret_cast<CopyData*>((char*)sharedMem + bufSize);
	}
}

FrameState::~FrameState()
{
	if (sharedMem) {
		UnmapViewOfFile(sharedMem);
	}
	if (sharedMemHandle) {
		CloseHandle(sharedMemHandle);
	}
}
