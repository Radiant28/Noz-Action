// Compile the actual loader helpers without introducing a second implementation.
#define wmain unused_launcher_main
#include "../src/launcher.cpp"
#undef wmain
#include <cassert>

int wmain(int argc, wchar_t** argv) {
    assert(argc == 3);
    assert(std::wcscmp(Localized(L"English", L"Русский", MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT)), L"Русский") == 0);
    assert(std::wcscmp(Localized(L"English", L"Русский", MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT)), L"English") == 0);
    wchar_t command[MAX_PATH + 3];
    assert(std::swprintf(command, MAX_PATH + 3, L"\"%ls\"", argv[1]) > 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(argv[1], command, nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process));
    const bool loaded = LoadModule(process, argv[2]);
    if (!loaded) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::fputs("FAIL: fixture DLL did not load into the new suspended child\n", stderr);
        return 1;
    }
    const DWORD resumed = ResumeThread(process.hThread);
    const DWORD wait = resumed == static_cast<DWORD>(-1) ? WAIT_FAILED :
        WaitForSingleObject(process.hProcess, 5000);
    DWORD code = 1;
    bool ok = wait == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &code) && code == 0;
    if (!ok) { TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000); }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    assert(ok);
    std::puts("PASS: real launcher loader initialized a fixture DLL in a new suspended x86 child and resumed it");
    return 0;
}
