#include "framestate.h"
#include <tinyalloc.h>
#include <windows.h>

constexpr const wchar_t *sharedMemHandleName = L"hanteichan-shared_mem";

// Global shared memory - initialized once per process
struct SharedMemoryGlobal {
	void *handle = nullptr;
	void *memory = nullptr;
	CopyData *copyData = nullptr;
	bool initialized = false;
	int refCount = 0;

	void Initialize() {
		if (initialized) {
			refCount++;
			return;
		}

		SYSTEM_INFO sInfo;
		GetSystemInfo(&sInfo);

		// 16MB for tinyalloc buffer
		size_t bufSize = 0x100 * sInfo.dwAllocationGranularity;
		// Additional space for CopyData struct at end
		size_t appendSize = (1 + sizeof(CopyData) / sInfo.dwAllocationGranularity) * sInfo.dwAllocationGranularity;

		handle = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			bufSize + appendSize,
			sharedMemHandleName);

		auto exists = GetLastError();

		// Base address for consistent mapping across instances
		void *baseAddress = (void*)((size_t)sInfo.lpMinimumApplicationAddress + sInfo.dwAllocationGranularity * 0x5000);

		memory = MapViewOfFileEx(
			handle,
			FILE_MAP_ALL_ACCESS,
			0, 0,
			bufSize + appendSize,
			baseAddress);

		// Initialize tinyalloc with buffer only if this is the first instance
		if (exists != ERROR_ALREADY_EXISTS) {
			// First instance - initialize tinyalloc and create CopyData
			ta_init(memory, (char*)memory + bufSize, 65535, 256, 16, false);
			copyData = new((char*)memory + bufSize) CopyData;
		} else {
			// Subsequent instances - reuse existing tinyalloc and CopyData
			// Just set the heap pointer without re-initializing
			copyData = reinterpret_cast<CopyData*>((char*)memory + bufSize);
		}

		initialized = true;
		refCount = 1;
	}

	void Cleanup() {
		refCount--;
		// NOTE: We intentionally do NOT cleanup when refCount hits 0
		// because tinyalloc's ta_init() can only be called once per process
		// (it has a static init_times counter that asserts == 0).
		// The shared memory will be cleaned up when the process exits.
		// This is fine since it's a process-wide resource.
	}
};

static SharedMemoryGlobal g_sharedMem;

FrameState::FrameState()
{
	// Initialize shared memory once per process
	g_sharedMem.Initialize();

	// All FrameStates share the same CopyData
	copied = g_sharedMem.copyData;
	sharedMemHandle = g_sharedMem.handle;
	sharedMem = g_sharedMem.memory;
}

FrameState::~FrameState()
{
	// Don't close handles - let global cleanup handle it
	g_sharedMem.Cleanup();
	sharedMemHandle = nullptr;
	sharedMem = nullptr;
}
