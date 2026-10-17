#include "copy_data.h"

#include <tinyalloc.h>
#include <windows.h>
#include <new>
#include <string>

namespace {

// tinyalloc parameters. Every process sharing the heap must use the same ones.
constexpr size_t kHeapBlocks = 65535;
constexpr size_t kSplitThresh = 256;
constexpr size_t kAlignment = 16;

struct SharedClipboard {
	bool initialized = false;
	bool shared = false;
	HANDLE handle = nullptr;
	void* memory = nullptr;
	CopyData* data = nullptr;
};
SharedClipboard g_clip;

// The mapping holds raw C++ objects, so two builds with different CopyData
// layouts must never share one (a newer build reading an older layout was a
// crash on paste). The layout sizes are part of the name.
std::wstring DefaultMappingName()
{
	return L"hanteichan-shared_mem-v2-" + std::to_wstring(sizeof(CopyData)) + L"-" +
	       std::to_wstring(sizeof(Frame_T<LinearAllocator>)) + L"-" +
	       std::to_wstring(sizeof(Sequence_T<LinearAllocator>));
}

} // namespace

CopyData* AcquireSharedCopyData(const wchar_t* mappingName)
{
	if (g_clip.initialized) return g_clip.data;
	g_clip.initialized = true;

	SYSTEM_INFO sInfo;
	GetSystemInfo(&sInfo);
	const size_t gran = sInfo.dwAllocationGranularity;
	const size_t bufSize = 0x100 * gran;                        // tinyalloc heap (16 MB)
	const size_t appendSize = (1 + sizeof(CopyData) / gran) * gran; // CopyData after it
	const size_t total = bufSize + appendSize;

	const std::wstring name = mappingName ? std::wstring(mappingName) : DefaultMappingName();
	HANDLE handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
		(DWORD)((unsigned long long)total >> 32), (DWORD)(total & 0xffffffffu), name.c_str());
	const DWORD err = GetLastError();

	// Containers in the mapping hold absolute pointers, so every process must map
	// the view at the same address.
	void* baseAddress = (void*)((size_t)sInfo.lpMinimumApplicationAddress + gran * 0x5000);
	void* memory = handle ? MapViewOfFileEx(handle, FILE_MAP_ALL_ACCESS, 0, 0, total, baseAddress) : nullptr;

	if (memory) {
		if (err != ERROR_ALREADY_EXISTS) {
			ta_init(memory, (char*)memory + bufSize, kHeapBlocks, kSplitThresh, kAlignment, false);
			g_clip.data = new ((char*)memory + bufSize) CopyData;
		} else {
			// Another instance created the heap and the CopyData. tinyalloc keeps
			// its heap pointer in per-process statics, so it still has to be told
			// where the heap is; without this the first copy in a second instance
			// dereferenced a null heap (issue #8).
			ta_init(memory, (char*)memory + bufSize, kHeapBlocks, kSplitThresh, kAlignment, true);
			g_clip.data = reinterpret_cast<CopyData*>((char*)memory + bufSize);
		}
		g_clip.handle = handle;
		g_clip.memory = memory;
		g_clip.shared = true;
		return g_clip.data;
	}

	// The common base address is taken in this process (or the mapping failed):
	// fall back to a private clipboard in ordinary memory.
	if (handle) CloseHandle(handle);
	memory = VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!memory) {
		// Last resort: tinyalloc on the process heap.
		memory = ::operator new(total);
	}
	ta_init(memory, (char*)memory + bufSize, kHeapBlocks, kSplitThresh, kAlignment, false);
	g_clip.data = new ((char*)memory + bufSize) CopyData;
	g_clip.memory = memory;
	g_clip.shared = false;
	return g_clip.data;
}

bool SharedCopyDataIsShared()
{
	return g_clip.shared;
}
