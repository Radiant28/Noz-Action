#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <cstdio>
#include <cstring>

inline bool SupportedClient(const wchar_t* path) {
    constexpr char expected[] = "c33f54116161b0baf3d6f3b58276688336e96c50a80e921f10a8d5a6c38a4a60";
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool ok = CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
              CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    unsigned char buffer[16384];
    DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = CryptHashData(hash, buffer, count, 0) != FALSE;
    }
    unsigned char digest[32];
    DWORD size = sizeof(digest);
    if (ok) ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) && size == sizeof(digest);
    char text[65] = {};
    if (ok) for (unsigned i = 0; i < sizeof(digest); ++i) std::sprintf(text + i * 2, "%02x", digest[i]);
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok && std::strcmp(text, expected) == 0;
}
