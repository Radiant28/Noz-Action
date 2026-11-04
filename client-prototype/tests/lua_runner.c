#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef struct lua_State lua_State;
typedef lua_State* (__cdecl *NewState)(void);
typedef void (__cdecl *OpenLibs)(lua_State*);
typedef int (__cdecl *LoadFile)(lua_State*, const char*, const char*);
typedef int (__cdecl *ProtectedCall)(lua_State*, int, int, int, intptr_t, void*);
typedef const char* (__cdecl *ToString)(lua_State*, int, size_t*);
typedef const char* (__cdecl *PushString)(lua_State*, const char*);
typedef void (__cdecl *SetGlobal)(lua_State*, const char*);
typedef void (__cdecl *Close)(lua_State*);

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) return 2;
    HMODULE library = LoadLibraryW(L"C:\\mingw32\\bin\\lua54.dll");
    if (!library) { fputs("Installed x86 lua54.dll is required for host addon tests\n", stderr); return 2; }
#define LOAD(type, symbol) type symbol = (type)(void*)GetProcAddress(library, #symbol)
    LOAD(NewState, luaL_newstate);
    LOAD(OpenLibs, luaL_openlibs);
    LOAD(LoadFile, luaL_loadfilex);
    LOAD(ProtectedCall, lua_pcallk);
    LOAD(ToString, lua_tolstring);
    LOAD(PushString, lua_pushstring);
    LOAD(SetGlobal, lua_setglobal);
    LOAD(Close, lua_close);
    if (!luaL_newstate || !luaL_openlibs || !luaL_loadfilex || !lua_pcallk ||
        !lua_tolstring || !lua_pushstring || !lua_setglobal || !lua_close) return 2;
    lua_State* state = luaL_newstate();
    if (!state) return 2;
    luaL_openlibs(state);
    lua_pushstring(state, argv[2]);
    lua_setglobal(state, "ADDON_FILE");
    lua_pushstring(state, argc == 4 ? argv[3] : "enUS");
    lua_setglobal(state, "TEST_LOCALE");
    int error = luaL_loadfilex(state, argv[1], NULL);
    if (!error) error = lua_pcallk(state, 0, 0, 0, 0, NULL);
    if (error) fprintf(stderr, "%s\n", lua_tolstring(state, -1, NULL));
    lua_close(state);
    FreeLibrary(library);
    return error ? 1 : 0;
}
