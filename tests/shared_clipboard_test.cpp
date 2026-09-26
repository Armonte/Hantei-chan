// Cross-instance clipboard test (issue #8). The parent copies a pattern into the
// shared clipboard and launches itself as a second process, which reads that
// pattern, then copies frames of its own (allocating from the shared heap). The
// parent then reads the child's frames and copies again. Before the fix the
// second process never initialised tinyalloc and crashed on its first copy.
//
//   shared_clipboard_test            (parent)
//   shared_clipboard_test --child N  (spawned by the parent)

#include "copy_data.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_fail = 0;
#define CHECK(x) do { if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

Sequence MakePattern(int frames, const char* name)
{
	Sequence s;
	s.name = name;
	s.codeName = "code_name_long_enough_to_leave_small_string_storage";
	s.psts = 3;
	s.frames.resize(frames);
	for (int i = 0; i < frames; ++i) {
		auto& f = s.frames[i];
		f.AF.layers.resize(2);
		f.AF.layers[0].spriteId = 100 + i;
		f.AF.duration = i + 1;
		f.EF.resize(3);
		f.EF[1].type = 1;
		f.EF[1].parameters[0] = i;
		f.IF.resize(2);
		f.IF[0].type = 25;
		f.hitboxes[0] = Hitbox{{-i, -2 * i, i, 2 * i}};
		f.hitboxes[25] = Hitbox{{1, 2, 3, i}};
	}
	return s;
}

void CheckPattern(const Sequence_T<LinearAllocator>& p, int frames, const char* name)
{
	CHECK(std::string(p.name.c_str()) == name);
	CHECK(p.psts == 3);
	CHECK((int)p.frames.size() == frames);
	for (int i = 0; i < (int)p.frames.size(); ++i) {
		const auto& f = p.frames[i];
		CHECK(f.AF.layers.size() == 2 && f.AF.layers[0].spriteId == 100 + i);
		CHECK(f.EF.size() == 3 && f.EF[1].parameters[0] == i);
		CHECK(f.IF.size() == 2 && f.IF[0].type == 25);
		auto it = f.hitboxes.find(25);
		CHECK(f.hitboxes.size() == 2 && it != f.hitboxes.end() && it->second.xy[3] == i);
	}
}

int Child(const wchar_t* mapping)
{
	CopyData* clip = AcquireSharedCopyData(mapping);
	CHECK(SharedCopyDataIsShared());
	CheckPattern(clip->pattern, 40, "parent pattern with a name longer than SSO");
	// Allocate in this process: copy frames into the shared clipboard.
	Sequence mine = MakePattern(25, "child");
	clip->frames.clear();
	for (const auto& f : mine.frames) {
		Frame_T<LinearAllocator> t;
		t = f;
		clip->frames.push_back(t);
	}
	clip->pattern = mine; // replaces (frees) the parent's pattern
	return g_fail == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc == 3 && !std::strcmp(argv[1], "--child")) {
		std::wstring name(argv[2], argv[2] + std::strlen(argv[2]));
		return Child(name.c_str());
	}

	const std::string mappingA = "hanteichan-clipboard-test-" + std::to_string(GetCurrentProcessId());
	const std::wstring mapping(mappingA.begin(), mappingA.end());
	CopyData* clip = AcquireSharedCopyData(mapping.c_str());
	CHECK(SharedCopyDataIsShared());
	clip->pattern = MakePattern(40, "parent pattern with a name longer than SSO");

	for (int round = 0; round < 3; ++round) {
		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --child " + mapping;
		STARTUPINFOW si{}; si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
			std::printf("CreateProcess failed\n");
			return 2;
		}
		WaitForSingleObject(pi.hProcess, 30000);
		DWORD code = 99;
		GetExitCodeProcess(pi.hProcess, &code);
		CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
		std::printf("round %d: child exit code %lu\n", round, (unsigned long)code);
		CHECK(code == 0);
		// The child's copies are visible here, and the heap still works.
		CheckPattern(clip->pattern, 25, "child");
		CHECK(clip->frames.size() == 25);
		clip->pattern = MakePattern(40, "parent pattern with a name longer than SSO");
		clip->frames.clear();
		clip->frames.shrink_to_fit();
	}

	std::printf(g_fail == 0 ? "SHARED_CLIPBOARD_TEST_PASS\n" : "SHARED_CLIPBOARD_TEST_FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
