#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include "client_identity.h"

namespace {
bool g_checkOnly = false;

const wchar_t* Localized(const wchar_t* english, const wchar_t* russian,
                         LANGID language = GetUserDefaultUILanguage()) {
    return PRIMARYLANGID(language) == LANG_RUSSIAN ? russian : english;
}

int Fail(const wchar_t* message, const wchar_t* russian) {
    std::fwprintf(stderr, L"Nozdor Action: %ls (Windows error %lu)\n", message, GetLastError());
    if (!g_checkOnly) MessageBoxW(nullptr, Localized(message, russian), L"Nozdor Action", MB_OK | MB_ICONERROR);
    return 1;
}

std::uintptr_t RemoteModule(DWORD pid, const wchar_t* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::uintptr_t result = 0;
    if (Module32FirstW(snapshot, &entry)) do {
        if (_wcsicmp(entry.szModule, name) == 0) {
            result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            break;
        }
    } while (Module32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return result;
}

LPTHREAD_START_ROUTINE RemoteLoadLibrary(DWORD pid) {
    FARPROC function = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!function) return nullptr;
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(function), &owner)) return nullptr;
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(owner, path, MAX_PATH)) return nullptr;
    const wchar_t* name = std::wcsrchr(path, L'\\');
    const auto remote = RemoteModule(pid, name ? name + 1 : path);
    if (remote) return reinterpret_cast<LPTHREAD_START_ROUTINE>(remote +
        reinterpret_cast<std::uintptr_t>(function) - reinterpret_cast<std::uintptr_t>(owner));
    // shortcut: pre-loader suspended starts use Windows' shared system-DLL mapping;
    // replace with a remote export bootstrap if a host uses per-process remapping.
    return reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<std::uintptr_t>(function));
}

bool LoadModule(PROCESS_INFORMATION& process, const wchar_t* dll) {
    const SIZE_T bytes = (std::wcslen(dll) + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(process.hProcess, nullptr, bytes,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) return false;
    SIZE_T written = 0;
    const auto start = RemoteLoadLibrary(process.dwProcessId);
    bool ok = start && WriteProcessMemory(process.hProcess, remotePath, dll, bytes, &written) &&
              written == bytes;
    HANDLE thread = ok ? CreateRemoteThread(process.hProcess, nullptr, 0, start, remotePath, 0, nullptr) : nullptr;
    ok = thread && WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0;
    DWORD result = 0;
    if (ok) ok = GetExitCodeThread(thread, &result) && result != 0;
    const wchar_t* name = std::wcsrchr(dll, L'\\');
    const wchar_t* forward = std::wcsrchr(dll, L'/');
    if (forward && (!name || forward > name)) name = forward;
    if (ok) ok = RemoteModule(process.dwProcessId, name ? name + 1 : dll) == result;
    if (thread) CloseHandle(thread);
    if (ok) VirtualFreeEx(process.hProcess, remotePath, 0, MEM_RELEASE);
    // Failed or timed-out loads leave memory until the launcher terminates its child.
    return ok;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    static_assert(sizeof(void*) == 4, "Launcher must match the x86 game");
    wchar_t ownPath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, ownPath, MAX_PATH);
    if (!length || length >= MAX_PATH) return Fail(L"Launcher path is too long", L"Слишком длинный путь к лаунчеру.");
    wchar_t* slash = std::wcsrchr(ownPath, L'\\');
    if (!slash) return Fail(L"Cannot locate launcher directory", L"Не удалось найти папку лаунчера.");
    *slash = 0;
    wchar_t exe[MAX_PATH], dll[MAX_PATH], defaultExe[MAX_PATH];
    if (std::swprintf(defaultExe, MAX_PATH, L"%ls\\..\\..\\WoW.exe", ownPath) < 0 ||
        std::swprintf(dll, MAX_PATH, L"%ls\\NozdorAction.dll", ownPath) < 0)
        return Fail(L"Prototype path is too long", L"Слишком длинный путь к модификации.");
    const wchar_t* input = defaultExe;
    bool supplied = false;
    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--check") == 0) g_checkOnly = true;
        else if (!supplied) { input = argv[i]; supplied = true; }
        else return Fail(L"Usage: NozdorActionLauncher.exe [WoW.exe] [--check]", L"Запуск: NozdorActionLauncher.exe [путь к WoW.exe] [--check]");
    }
    const DWORD fullLength = GetFullPathNameW(input, MAX_PATH, exe, nullptr);
    if (!fullLength || fullLength >= MAX_PATH) return Fail(L"Invalid client path", L"Некорректный путь к клиенту игры.");
    if (!SupportedClient(exe))
        return Fail(L"Client SHA-256 is unsupported. This build accepts only the recorded WoW.exe.",
                    L"Этот WoW.exe не поддерживается: контрольная сумма отличается. Нужен совместимый клиент из инструкции.");
    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES)
        return Fail(L"NozdorAction.dll must be beside the launcher", L"NozdorAction.dll должна лежать рядом с лаунчером. Скопируй всё содержимое архива в папку игры.");
    if (g_checkOnly) {
        std::wprintf(L"PASS: exact client SHA-256; DLL present. Game was not started.\n%ls\n", exe);
        return 0;
    }
    wchar_t working[MAX_PATH];
    std::wcscpy(working, exe);
    wchar_t* last = std::wcsrchr(working, L'\\');
    if (!last) return Fail(L"Cannot determine the client directory", L"Не удалось определить папку игры.");
    *last = 0;
    wchar_t command[MAX_PATH + 3];
    std::swprintf(command, MAX_PATH + 3, L"\"%ls\"", exe);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe, command, nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                        nullptr, working, &startup, &process)) return Fail(L"Cannot start the client", L"Не удалось запустить клиент игры.");
    bool ok = LoadModule(process, dll);
    if (ok) ok = ResumeThread(process.hThread) != static_cast<DWORD>(-1);
    if (!ok) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!ok) return Fail(L"DLL did not initialize; the new test client was stopped. Read NozdorAction.log.",
                         L"DLL не загрузилась; запуск игры остановлен. Подробности в client-prototype\\bin\\NozdorAction.log.");
    std::wprintf(L"Nozdor Action loaded. /nfa: settings. Assign Toggle, Interact and Cursor in addon options.\n");
    return 0;
}
