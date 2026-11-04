#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

// Only install while the target's main thread is still suspended by our launcher.
inline bool ReplaceCall(unsigned char* site, const void* original, const void* hook) {
    if (site[0] != 0xE8) return false;
    std::uint32_t oldOffset;
    std::memcpy(&oldOffset, site + 1, sizeof(oldOffset));
    const auto after = reinterpret_cast<std::uintptr_t>(site + 5);
    if (after + oldOffset != reinterpret_cast<std::uintptr_t>(original)) return false;
    const auto replacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(hook) - after);
    DWORD protection;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &protection)) return false;
    std::memcpy(site + 1, &replacement, sizeof(replacement));
    if (!FlushInstructionCache(GetCurrentProcess(), site, 5)) {
        std::memcpy(site + 1, &oldOffset, sizeof(oldOffset));
        FlushInstructionCache(GetCurrentProcess(), site, 5);
        DWORD ignored;
        VirtualProtect(site, 5, protection, &ignored);
        return false;
    }
    DWORD ignored;
    if (VirtualProtect(site, 5, protection, &ignored)) return true;
    std::memcpy(site + 1, &oldOffset, sizeof(oldOffset));
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualProtect(site, 5, protection, &ignored);
    return false;
}
