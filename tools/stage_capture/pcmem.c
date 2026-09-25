// pcmem.exe - tiny process-memory tool for the stage ground-truth capture harness
// (tools/stage_capture/stage_capture.sh). Test-copy use only: it attaches to the MBAA.exe whose image path
// contains a given substring (e.g. "mbaacc_winaspect"), so it can never touch the user's play copy.
//
// SAFETY: every memory op takes an explicit PID (the one stage_capture.sh launched and recorded) and is
// refused unless that PID's image path contains "mbaacc_winaspect". There is no name-based targeting.
//   pcmem find <pathsubstr>                  -> prints the pid of an MBAA.exe under that path (exit 1 if none)
//   pcmem <pid> r <addr> <len> <outfile>     -> read bytes to a file
//   pcmem <pid> rd <addr>                    -> print one dword (hex and decimal)
//   pcmem <pid> w <addr> <hexbytes>          -> write bytes (code pages are made writable)
//   pcmem <pid> freeze | unfreeze            -> RET-patch / restore the six stage updaters
//   pcmem <pid> step <n>                     -> (frozen) run the six stage updaters n times, game suspended
//   pcmem <pid> script <file>                -> run lines "w addr hex" / "r addr len file" / "rd addr" / "step n" /
//                                                    "suspend" / "resume" / "sleep ms"
// Build: i686-w64-mingw32-gcc -O2 -o pcmem.exe pcmem.c -lpsapi
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LONG(NTAPI* NtSuspendProcess_t)(HANDLE);

static DWORD findPid(const char* sub) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof pe;
    DWORD found = 0;
    if (Process32First(snap, &pe)) do {
        if (_stricmp(pe.szExeFile, "MBAA.exe") != 0) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        char path[MAX_PATH * 2] = {0}; DWORD n = sizeof path;
        if (QueryFullProcessImageNameA(h, 0, path, &n)) {
            char low[MAX_PATH * 2]; char sl[256];
            strcpy(low, path); strncpy(sl, sub, 255); sl[255] = 0;
            for (char* p = low; *p; ++p) *p = (char)tolower(*p);
            for (char* p = sl; *p; ++p) *p = (char)tolower(*p);
            if (strstr(low, sl)) found = pe.th32ProcessID;
        }
        CloseHandle(h);
    } while (!found && Process32Next(snap, &pe));
    CloseHandle(snap);
    return found;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int doWrite(HANDLE h, unsigned long addr, const char* hex) {
    unsigned char buf[4096]; size_t n = 0;
    for (const char* p = hex; p[0] && p[1] && n < sizeof buf;) {
        if (*p == ' ' || *p == ':') { ++p; continue; }
        int a = hexval(p[0]), b = hexval(p[1]);
        if (a < 0 || b < 0) { fprintf(stderr, "bad hex\n"); return 1; }
        buf[n++] = (unsigned char)(a * 16 + b); p += 2;
    }
    DWORD old = 0;
    VirtualProtectEx(h, (LPVOID)addr, n, PAGE_EXECUTE_READWRITE, &old);
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)addr, buf, n, &wr);
    DWORD tmp; VirtualProtectEx(h, (LPVOID)addr, n, old, &tmp);
    FlushInstructionCache(h, (LPVOID)addr, n);
    if (!ok || wr != n) { fprintf(stderr, "write %lx failed (%lu)\n", addr, GetLastError()); return 1; }
    return 0;
}

static int doRead(HANDLE h, unsigned long addr, unsigned long len, const char* out) {
    unsigned char* buf = (unsigned char*)malloc(len);
    SIZE_T rd = 0;
    if (!ReadProcessMemory(h, (LPCVOID)addr, buf, len, &rd) || rd != len) {
        fprintf(stderr, "read %lx+%lu failed (%lu)\n", addr, len, GetLastError()); free(buf); return 1;
    }
    FILE* f = fopen(out, "wb"); if (!f) { free(buf); return 1; }
    fwrite(buf, 1, len, f); fclose(f); free(buf);
    return 0;
}

static int doRd(HANDLE h, unsigned long addr) {
    DWORD v = 0; SIZE_T rd = 0;
    if (!ReadProcessMemory(h, (LPCVOID)addr, &v, 4, &rd)) return 1;
    printf("%08lx %ld %g\n", (unsigned long)v, (long)v, *(float*)&v);
    return 0;
}

static NtSuspendProcess_t pSuspend, pResume;

// Run MBAA's six stage updaters (Background_UpdateAndRender's a1=1 block, the same
// functions pchost's tickStageBackground calls per Present) exactly n times on a
// remote thread. Use it while the stage is frozen (settings+0x164 = 1) so nobody
// else advances the stage: that gives an exact n-tick step.
static const unsigned long fns[6] = { 0x4B88B0, 0x4B8530, 0x4B8CD0, 0x4B8DF0, 0x4B5210, 0x4B9100 };
static const unsigned char fnFirst[6] = { 0x57, 0x53, 0x83, 0x55, 0x83, 0x51 };   // original first bytes

// Freeze the stage: first byte of each updater -> RET (C3). pchost pins the game's own
// stage-animation option every frame and drives the updaters from its Present hook, so
// patching the functions is the only freeze that holds.
static int setFrozen(HANDLE h, int frozen) {
    for (int i = 0; i < 6; ++i) {
        char hex[4]; sprintf(hex, "%02x", frozen ? 0xC3 : fnFirst[i]);
        extern int doWrite(HANDLE, unsigned long, const char*);
        if (doWrite(h, fns[i], hex)) return 1;
    }
    return 0;
}

static int doStep(HANDLE h, unsigned long n) {
    unsigned char code[96]; int k = 0;
    code[k++] = 0x53;                                   // push ebx
    code[k++] = 0xBB; memcpy(code + k, &n, 4); k += 4;  // mov ebx, n
    int loop = k;
    for (int i = 0; i < 6; ++i) {
        code[k++] = 0xB8; memcpy(code + k, &fns[i], 4); k += 4;   // mov eax, fn
        code[k++] = 0xFF; code[k++] = 0xD0;                       // call eax
    }
    code[k++] = 0x4B;                                   // dec ebx
    code[k++] = 0x75; code[k] = (unsigned char)(loop - (k + 1)); k++;   // jnz loop
    code[k++] = 0x5B;                                   // pop ebx
    code[k++] = 0x31; code[k++] = 0xC0;                 // xor eax, eax
    code[k++] = 0xC2; code[k++] = 0x04; code[k++] = 0x00;   // ret 4
    if (!n) return 0;
    LPVOID mem = VirtualAllocEx(h, 0, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return 1;
    // Suspend every game thread, un-freeze, run the updaters n times on our own
    // remote thread, freeze again, resume: nobody else can tick the stage meanwhile.
    pSuspend(h);
    setFrozen(h, 0);
    SIZE_T wr = 0;
    WriteProcessMemory(h, mem, code, k, &wr);
    FlushInstructionCache(h, mem, k);
    HANDLE th = CreateRemoteThread(h, 0, 0, (LPTHREAD_START_ROUTINE)mem, 0, 0, 0);
    if (!th) { setFrozen(h, 1); pResume(h); VirtualFreeEx(h, mem, 0, MEM_RELEASE); return 1; }
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    setFrozen(h, 1);
    pResume(h);
    VirtualFreeEx(h, mem, 0, MEM_RELEASE);
    return 0;
}

static int runOp(HANDLE h, int argc, char** argv) {
    if (argc < 1) return 1;
    if (!strcmp(argv[0], "w") && argc >= 3) return doWrite(h, strtoul(argv[1], 0, 16), argv[2]);
    if (!strcmp(argv[0], "r") && argc >= 4) return doRead(h, strtoul(argv[1], 0, 16), strtoul(argv[2], 0, 0), argv[3]);
    if (!strcmp(argv[0], "rd") && argc >= 2) return doRd(h, strtoul(argv[1], 0, 16));
    if (!strcmp(argv[0], "step") && argc >= 2) return doStep(h, strtoul(argv[1], 0, 0));
    if (!strcmp(argv[0], "freeze")) return setFrozen(h, 1);
    if (!strcmp(argv[0], "unfreeze")) return setFrozen(h, 0);
    if (!strcmp(argv[0], "suspend")) return pSuspend(h) < 0;
    if (!strcmp(argv[0], "resume")) return pResume(h) < 0;
    if (!strcmp(argv[0], "sleep") && argc >= 2) { Sleep(atoi(argv[1])); return 0; }
    fprintf(stderr, "unknown op %s\n", argv[0]);
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: see pcmem.c\n"); return 2; }
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    pSuspend = (NtSuspendProcess_t)GetProcAddress(nt, "NtSuspendProcess");
    pResume = (NtSuspendProcess_t)GetProcAddress(nt, "NtResumeProcess");
    if (!strcmp(argv[1], "find")) {
        DWORD pid = findPid(argv[2]);
        if (!pid) return 1;
        printf("%lu\n", (unsigned long)pid);
        return 0;
    }
    DWORD pid = strtoul(argv[1], 0, 10);
    if (!pid) { fprintf(stderr, "need an explicit pid\n"); return 1; }
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) { fprintf(stderr, "OpenProcess failed %lu\n", GetLastError()); return 1; }
    {
        char path[MAX_PATH * 2] = {0}; DWORD n = sizeof path;
        if (!QueryFullProcessImageNameA(h, 0, path, &n)) { CloseHandle(h); fprintf(stderr, "no image path\n"); return 1; }
        for (char* p = path; *p; ++p) *p = (char)tolower(*p);
        if (!strstr(path, "\\mbaacc_winaspect\\mbaa.exe")) {
            fprintf(stderr, "REFUSED: pid %lu is %s, not the test copy\n", (unsigned long)pid, path);
            CloseHandle(h); return 3;
        }
    }
    int rc = 0;
    if (!strcmp(argv[2], "script") && argc >= 4) {
        FILE* f = fopen(argv[3], "r");
        if (!f) return 1;
        char line[8192];
        while (fgets(line, sizeof line, f)) {
            char* av[8]; int ac = 0;
            for (char* t = strtok(line, " \t\r\n"); t && ac < 8; t = strtok(0, " \t\r\n")) av[ac++] = t;
            if (!ac || av[0][0] == '#') continue;
            if (runOp(h, ac, av)) { rc = 1; fprintf(stderr, "op failed: %s\n", av[0]); }
        }
        fclose(f);
    } else {
        rc = runOp(h, argc - 2, argv + 2);
    }
    CloseHandle(h);
    return rc;
}
