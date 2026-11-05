#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cmath>
#include "call_hook.h"
#include "selection.h"
#include "camera_math.h"
#include "signatures.h"

static_assert(sizeof(void*) == 4, "Only the verified x86 client is supported");

namespace {
using Guid = std::uint64_t;
using Lua = void;
using LuaFunction = int (__cdecl*)(Lua*);
using GetState = Lua* (__cdecl*)();
using Register = int (__cdecl*)(const char*, LuaFunction);
using GetTop = int (__cdecl*)(Lua*);
using SetTop = void (__cdecl*)(Lua*, int);
using Type = int (__cdecl*)(Lua*, int);
using ToString = const char* (__cdecl*)(Lua*, int, std::size_t*);
using ToNumber = double (__cdecl*)(Lua*, int);
using ToBoolean = int (__cdecl*)(Lua*, int);
using ToUserdata = void* (__cdecl*)(Lua*, int);
using RawGetI = void (__cdecl*)(Lua*, int, int);
using PushString = void (__cdecl*)(Lua*, const char*);
using PushNumber = void (__cdecl*)(Lua*, double);
using PushBoolean = void (__cdecl*)(Lua*, int);
using GetField = void (__cdecl*)(Lua*, int, const char*);
using UpdateWorld = int (__attribute__((thiscall))*)(void*);
using GetPlayer = Guid (__cdecl*)();
using Resolve = void* (__cdecl*)(Guid, unsigned, const char*, unsigned);
using UnitTest = bool (__attribute__((thiscall))*)(void*);
using UnitRelation = bool (__attribute__((thiscall))*)(void*, void*);
using UnitReaction = int (__attribute__((thiscall))*)(void*, void*);
using SetTarget = void (__cdecl*)(Guid);
using TrackObject = int (__attribute__((thiscall))*)(void*, Guid);
using HoverChange = void* (__cdecl*)(const Guid*);
using HighlightObject = void (__attribute__((thiscall))*)(void*, int);
using InteractGuid = int (__cdecl*)(Guid);
using Lootable = bool (__attribute__((thiscall))*)(void*, DWORD);
using GetCursor = void (__cdecl*)(LONG*, LONG*);
using SetCursor = void (__cdecl*)(LONG, LONG);
using GetWindow = HWND (__cdecl*)(int);

using Vec3 = action::Vec3;
struct PickResult {
    Guid guid;
    Vec3 point;
    float distance;
    Vec3 start, end;
};
static_assert(sizeof(PickResult) == 48);
using Pick = int (__attribute__((thiscall))*)(void*, float, float, int, PickResult*);
using Position = Vec3* (__attribute__((thiscall))*)(void*, Vec3*);
using Collision = int (__attribute__((thiscall))*)(void*, Vec3*, float*, float*, Vec3*, float, float*);
using TraceLine = bool (__cdecl*)(const Vec3*, const Vec3*, Vec3*, float*, unsigned, void*);
using Zoom = void (__attribute__((thiscall))*)(void*, float, DWORD, float);

template<class T> T Function(std::uintptr_t address) { return reinterpret_cast<T>(address); }
template<class T> T& Field(void* object, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}

constexpr char kApiName[] = "NozdorActionNative";
constexpr char kVersion[] = "0.3.20";
HANDLE g_log = INVALID_HANDLE_VALUE;
bool g_enabled = false, g_autoTarget = true, g_captured = false;
bool g_active = false, g_inFrame = false;
bool g_cursorCenter = false, g_cursorSaved = false, g_cursorReturn = false;
int g_cursorKeys[2] = {VK_MENU, 0};
unsigned g_cursorModifiers[2] = {0, 0};
bool g_keepTarget = false;
bool g_softTarget = true, g_leftMouseConsumed = false;
bool g_hoverHookInstalled = false;
action::ClickSelection g_clickSelection;
Guid g_softHover = 0;
void* g_softWorld = nullptr;
bool g_cursorToggle = false, g_cursorInputAllowed = false;
bool g_cursorOverUI = false, g_rightMouseConsumed = false;
Guid g_aimPlate = 0, g_cursorPlate = 0;
Guid g_cursorInteractPlate = 0;
DWORD g_plateTime = 0;
HWND g_cursorWindow = nullptr;
WNDPROC g_previousWindowProc = nullptr;
action::CursorMode g_cursorMode;
float g_centerX = 0.5f, g_centerY = 0.5f;
POINT g_previousCursor{};
bool g_interactPending = false, g_cursorInteractPending = false;
action::CursorPoint g_cursorPick;
DWORD g_interactTime = 0, g_interactPause = 0;
float g_range = 40;
DWORD g_delay = 100, g_heartbeat = 0, g_frames = 0;
Guid g_pick = 0;
const char* g_status = "disabled";
action::Selection g_selection;
struct TargetProbe {
    int kind = -1;
    unsigned objectMask = 0;
    Guid raw = 0;
    float distance = -1;
    const char* reason = "not sampled";
    float left = 0, right = 0, bottom = 0, top = 0;
    Vec3 ray{};
    Vec3 origin{};
    bool models = false;
} g_probe;
char g_targetDiagnostic[512] = "No active targeting sample yet";
DWORD g_probeTime = 0;
bool g_cameraEnabled = false, g_cameraReady = false, g_cameraApplied = false;
bool g_zoomPending = false, g_cameraLogged = false;
float g_shoulder = 1.25f, g_height = 0.65f, g_distance = 6.0f, g_savedZoom = 0;
void* g_cameraSession = nullptr;
Guid g_cameraOwner = 0;
bool g_mounted = false, g_mountAdapt = true;
bool g_stockCamera = false, g_mountStockCamera = false, g_mountCustom = false;
float g_mountCameraDistance = 10, g_mountCameraHeight = 1.65f, g_mountShoulder = 1.8f;
float g_mountDistance = 3, g_mountHeight = 1, g_activeCameraSide = 0, g_activeCameraHeight = 0;
action::ProfileZoom g_profileZoom;
DWORD g_cameraTime = 0;

void Log(const char* message) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(g_log, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
    WriteFile(g_log, "\r\n", 2, &written, nullptr);
}

void OpenLog(HMODULE self) {
    wchar_t path[MAX_PATH];
    const DWORD count = GetModuleFileNameW(self, path, MAX_PATH);
    if (!count || count >= MAX_PATH) return;
    wchar_t* slash = std::wcsrchr(path, L'\\');
    if (!slash || static_cast<std::size_t>(slash - path) + 19 >= MAX_PATH) return;
    std::wcscpy(slash + 1, L"NozdorAction.log");
    g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

int ReplyError(Lua* state, const char* message) {
    Function<PushBoolean>(0x84E4D0)(state, 0);
    Function<PushString>(0x84E350)(state, message);
    return 2;
}

Guid NativeNameplateGuid(const void* frame) {
    const auto address = reinterpret_cast<std::uintptr_t>(frame);
    if (!address || address > UINTPTR_MAX - 0x2B0) return 0;
    std::uintptr_t vtable = 0;
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), frame, &vtable, sizeof(vtable), &read) ||
        read != sizeof(vtable) || vtable != 0xAA36F8) return 0;
    Guid guid = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address + 0x2A8),
                           &guid, sizeof(guid), &read) || read != sizeof(guid)) return 0;
    return guid;
}

bool CursorKeyHeld() {
    const unsigned held = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1u : 0u) |
        ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 2u : 0u) |
        ((GetAsyncKeyState(VK_MENU) & 0x8000) ? 4u : 0u);
    for (unsigned i = 0; i < 2; ++i) {
        if (g_cursorKeys[i] && action::CursorGestureDown(g_cursorKeys[i], g_cursorModifiers[i], held,
                                     (GetAsyncKeyState(g_cursorKeys[i]) & 0x8000) != 0, g_rightMouseConsumed)) return true;
    }
    return false;
}

bool CursorIsFree() {
    return g_cursorToggle ? g_cursorMode.latched : CursorKeyHeld();
}

void ResetMouseGestures() {
    g_leftMouseConsumed = g_rightMouseConsumed = false;
    g_clickSelection = {};
}

int __cdecl NativeApi(Lua* state) {
    const auto type = Function<Type>(0x84DEB0);
    if (type(state, 1) != 4) return ReplyError(state, "Expected a command string");
    const char* command = Function<ToString>(0x84E0E0)(state, 1, nullptr);
    if (std::strcmp(command, "plateguid") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 2 || type(state, 2) != 5)
            return ReplyError(state, "plateguid expects a frame");
        Function<RawGetI>(0x84E670)(state, 2, 0);
        const Guid guid = type(state, -1) == 2 ? NativeNameplateGuid(Function<ToUserdata>(0x84E1C0)(state, -1)) : 0;
        Function<SetTop>(0x84DBF0)(state, 2);
        char text[19] = "";
        if (guid) std::snprintf(text, sizeof(text), "0x%08X%08X", static_cast<unsigned>(guid >> 32), static_cast<unsigned>(guid));
        Function<PushString>(0x84E350)(state, text);
        return 1;
    }
    if (std::strcmp(command, "unitinfo") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 2 || type(state, 2) != 4)
            return ReplyError(state, "unitinfo expects a GUID string");
        std::size_t length = 0;
        const char* text = Function<ToString>(0x84E0E0)(state, 2, &length);
        Guid guid = 0;
        if (!action::ParseGuid(text, length, guid)) return ReplyError(state, "Invalid unit GUID");
        unsigned classId = 0;
        int reaction = 0;
        bool dead = false;
        if (guid && *reinterpret_cast<void**>(0xB7436C)) {
            void* unit = Function<Resolve>(0x4D4DB0)(guid, 8, "NozdorAction", 11);
            void* player = Function<Resolve>(0x4D4DB0)(Function<GetPlayer>(0x4D3790)(), 0x10, "NozdorAction", 12);
            void* stats = unit ? Field<void*>(unit, 0xD0) : nullptr;
            void* descriptor = unit ? Field<void*>(unit, 8) : nullptr;
            if (stats && descriptor && player) {
                if (Field<unsigned>(descriptor, 8) & 0x10) classId = Field<unsigned char>(stats, 0x45);
                reaction = Function<UnitReaction>(0x7251C0)(unit, player) + 1;
                dead = Function<UnitTest>(0x716490)(unit);
            }
        }
        Function<PushNumber>(0x84E2A0)(state, classId);
        Function<PushNumber>(0x84E2A0)(state, reaction);
        Function<PushBoolean>(0x84E4D0)(state, dead);
        return 3;
    }
    if (std::strcmp(command, "configure") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 30 ||
            type(state, 2) != 1 || type(state, 3) != 1 ||
            type(state, 4) != 3 || type(state, 5) != 3 || type(state, 6) != 1 ||
            type(state, 7) != 3 || type(state, 8) != 3 || type(state, 9) != 3 ||
            type(state, 10) != 1 || type(state, 11) != 3 || type(state, 12) != 3 ||
            type(state, 13) != 1 || type(state, 14) != 1 || type(state, 15) != 3 || type(state, 16) != 3 ||
            type(state, 17) != 3 || type(state, 18) != 3 || type(state, 19) != 3 || type(state, 20) != 3 ||
            type(state, 21) != 1 || type(state, 22) != 1 || type(state, 23) != 1 ||
            type(state, 24) != 1 || type(state, 25) != 1 || type(state, 26) != 1 ||
            type(state, 27) != 3 || type(state, 28) != 3 || type(state, 29) != 3 || type(state, 30) != 1)
            return ReplyError(state, "configure expects protocol 12 camera profiles and input settings");
        const double range = Function<ToNumber>(0x84E030)(state, 4);
        const double delay = Function<ToNumber>(0x84E030)(state, 5);
        if (!action::ValidSettings(range, delay))
            return ReplyError(state, "Range must be 5..300; delay must be 0..500 ms");
        const double side = Function<ToNumber>(0x84E030)(state, 7);
        const double height = Function<ToNumber>(0x84E030)(state, 8);
        const double distance = Function<ToNumber>(0x84E030)(state, 9);
        if (!action::ValidCamera(side, height, distance))
            return ReplyError(state, "Camera: shoulder -3..3; height -1..3; distance 3..25");
        const double centerX = Function<ToNumber>(0x84E030)(state, 11);
        const double centerY = Function<ToNumber>(0x84E030)(state, 12);
        const double mountDistance = Function<ToNumber>(0x84E030)(state, 15);
        const double mountHeight = Function<ToNumber>(0x84E030)(state, 16);
        if (!action::ValidCursorMount(centerX, centerY, mountDistance, mountHeight))
            return ReplyError(state, "Invalid cursor or mounted camera settings");
        const double cursorKey = Function<ToNumber>(0x84E030)(state, 17);
        const double cursorModifiers = Function<ToNumber>(0x84E030)(state, 18);
        const double secondKey = Function<ToNumber>(0x84E030)(state, 19);
        const double secondModifiers = Function<ToNumber>(0x84E030)(state, 20);
        if (!action::ValidCursorBinding(cursorKey, cursorModifiers) || !action::ValidCursorBinding(secondKey, secondModifiers))
            return ReplyError(state, "Invalid cursor virtual key");
        const double mountSide = Function<ToNumber>(0x84E030)(state, 27);
        const double mountCameraHeight = Function<ToNumber>(0x84E030)(state, 28);
        const double mountCameraDistance = Function<ToNumber>(0x84E030)(state, 29);
        if (!action::ValidMountedCamera(mountSide, mountCameraHeight, mountCameraDistance))
            return ReplyError(state, "Invalid mounted camera profile");
        const bool camera = Function<ToBoolean>(0x84E0B0)(state, 6) != 0;
        if ((camera && !g_cameraEnabled) || static_cast<float>(distance) != g_distance) g_zoomPending = true;
        g_cameraEnabled = camera;
        if (!camera) {
            ResetMouseGestures();
            g_cursorMode = {};
            g_active = false;
        }
        g_shoulder = static_cast<float>(side);
        g_height = static_cast<float>(height);
        g_distance = static_cast<float>(distance);
        g_enabled = Function<ToBoolean>(0x84E0B0)(state, 2) != 0;
        g_autoTarget = Function<ToBoolean>(0x84E0B0)(state, 3) != 0;
        g_range = static_cast<float>(range);
        g_delay = static_cast<DWORD>(delay);
        g_heartbeat = GetTickCount();
        g_cursorCenter = Function<ToBoolean>(0x84E0B0)(state, 10) != 0;
        g_centerX = static_cast<float>(centerX); g_centerY = static_cast<float>(centerY);
        g_mounted = Function<ToBoolean>(0x84E0B0)(state, 13) != 0;
        g_mountAdapt = Function<ToBoolean>(0x84E0B0)(state, 14) != 0;
        g_mountDistance = static_cast<float>(mountDistance); g_mountHeight = static_cast<float>(mountHeight);
        const bool bindingsChanged = g_cursorKeys[0] != cursorKey || g_cursorModifiers[0] != cursorModifiers ||
            g_cursorKeys[1] != secondKey || g_cursorModifiers[1] != secondModifiers;
        g_cursorKeys[0] = static_cast<int>(cursorKey); g_cursorModifiers[0] = static_cast<unsigned>(cursorModifiers);
        g_cursorKeys[1] = static_cast<int>(secondKey); g_cursorModifiers[1] = static_cast<unsigned>(secondModifiers);
        g_keepTarget = Function<ToBoolean>(0x84E0B0)(state, 21) != 0;
        const bool cursorToggle = Function<ToBoolean>(0x84E0B0)(state, 22) != 0;
        if (cursorToggle != g_cursorToggle || (bindingsChanged && !g_cursorKeys[0] && !g_cursorKeys[1])) g_cursorMode = {};
        else if (bindingsChanged) { g_cursorMode.wasDown = CursorKeyHeld(); g_cursorMode.wasAllowed = false; }
        g_cursorToggle = cursorToggle;
        g_cursorInputAllowed = Function<ToBoolean>(0x84E0B0)(state, 23) != 0;
        g_stockCamera = Function<ToBoolean>(0x84E0B0)(state, 24) != 0;
        g_mountStockCamera = Function<ToBoolean>(0x84E0B0)(state, 25) != 0;
        g_mountCustom = Function<ToBoolean>(0x84E0B0)(state, 26) != 0;
        g_mountShoulder = static_cast<float>(mountSide);
        g_mountCameraHeight = static_cast<float>(mountCameraHeight);
        g_mountCameraDistance = static_cast<float>(mountCameraDistance);
        const bool softTarget = Function<ToBoolean>(0x84E0B0)(state, 30) != 0;
        if (softTarget != g_softTarget) g_clickSelection = {};
        g_softTarget = softTarget;
        Function<PushBoolean>(0x84E4D0)(state, 1);
        return 1;
    }
    if (std::strcmp(command, "status") == 0) {
        char guid[19] = "";
        if (g_pick) std::snprintf(guid, sizeof(guid), "0x%08X%08X",
            static_cast<unsigned>(g_pick >> 32), static_cast<unsigned>(g_pick));
        Function<PushNumber>(0x84E2A0)(state, 12);
        Function<PushString>(0x84E350)(state, kVersion);
        Function<PushBoolean>(0x84E4D0)(state, g_active);
        Function<PushString>(0x84E350)(state, guid);
        Function<PushString>(0x84E350)(state, g_status);
        Function<PushNumber>(0x84E2A0)(state, g_frames);
        Function<PushBoolean>(0x84E4D0)(state, g_cameraApplied);
        Function<PushString>(0x84E350)(state, g_targetDiagnostic);
        Function<PushBoolean>(0x84E4D0)(state, g_cameraEnabled && CursorIsFree());
        return 9;
    }
    if (std::strcmp(command, "cursorui") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 5 || type(state, 2) != 1 || type(state, 3) != 1 ||
            type(state, 4) != 4 || type(state, 5) != 4)
            return ReplyError(state, "cursorui expects two booleans and two GUID strings");
        Guid aim = 0, cursor = 0;
        std::size_t length = 0;
        const char* text = Function<ToString>(0x84E0E0)(state, 4, &length);
        if (!action::ParseGuid(text, length, aim)) return ReplyError(state, "Invalid nameplate GUID");
        text = Function<ToString>(0x84E0E0)(state, 5, &length);
        if (!action::ParseGuid(text, length, cursor)) return ReplyError(state, "Invalid nameplate GUID");
        g_cursorInputAllowed = Function<ToBoolean>(0x84E0B0)(state, 2) != 0;
        g_cursorOverUI = Function<ToBoolean>(0x84E0B0)(state, 3) != 0;
        g_aimPlate = g_cursorInputAllowed && !g_cursorOverUI ? aim : 0;
        g_cursorPlate = g_cursorInputAllowed && !g_cursorOverUI ? cursor : 0;
        g_plateTime = GetTickCount();
        Function<PushBoolean>(0x84E4D0)(state, 1);
        return 1;
    }
    if (std::strcmp(command, "interact") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 1) return ReplyError(state, "interact takes no arguments");
        if (!g_enabled || !g_cameraEnabled || GetTickCount() - g_heartbeat > 500)
            return ReplyError(state, "Mode is off or paused");
        if (CursorIsFree()) return ReplyError(state, "Free cursor is active");
        g_interactPending = true;
        g_interactTime = GetTickCount();
        Function<PushBoolean>(0x84E4D0)(state, 1);
        return 1;
    }
    if (std::strcmp(command, "interactcursor") == 0) {
        if (Function<GetTop>(0x84DBD0)(state) != 1) return ReplyError(state, "interactcursor takes no arguments");
        if (!g_cameraEnabled || !g_cursorInputAllowed || GetTickCount() - g_heartbeat > 500 || !CursorIsFree())
            return ReplyError(state, "Mode is off or paused");
        if (g_cursorKeys[0] != VK_RBUTTON && g_cursorKeys[1] != VK_RBUTTON)
            return ReplyError(state, "RMB cursor binding required");
        RECT rect{};
        POINT point{};
        const HWND window = Function<GetWindow>(0x86C6A0)(0);
        if (!window || !GetClientRect(window, &rect)) return ReplyError(state, "Cursor is outside game window");
        // A stock left drag can still own relative capture during OnMouseUp; use its saved click point.
        if (*reinterpret_cast<unsigned*>(0xD4156C) == 1) point = *reinterpret_cast<POINT*>(0xD413F8);
        else Function<GetCursor>(0x86A0D0)(&point.x, &point.y);
        g_cursorPick = action::CursorToViewport({point.x, point.y}, rect.right, rect.bottom);
        if (!g_cursorPick.valid) return ReplyError(state, "Cursor is outside game window");
        g_cursorInteractPending = true;
        g_cursorInteractPlate = GetTickCount() - g_plateTime <= 250 ? g_cursorPlate : 0;
        g_interactTime = GetTickCount();
        Function<PushBoolean>(0x84E4D0)(state, 1);
        return 1;
    }
    return ReplyError(state, "Unknown command");
}

bool SetHoverHook(bool enabled);
bool RestoreCursorWindow();

void UpdateSoftHighlight(Guid previous, Guid next) {
    action::UpdateHoverHighlight(previous, next,
        [](Guid guid) { return Function<Resolve>(0x4D4DB0)(guid, 1, "NozdorAction", 8); },
        [](void* object, bool enabled) {
            Function<HighlightObject>(enabled ? 0x743C70 : 0x743BC0)(object, 1);
        });
}

void ClearSoftHover() {
    const Guid owned = g_softHover;
    void* world = g_softWorld;
    g_softHover = 0;
    g_softWorld = nullptr;
    if (!owned || !world || world != *reinterpret_cast<void**>(0xB7436C)) return;
    UpdateSoftHighlight(owned, 0);
    if (*reinterpret_cast<Guid*>(0xBD07A0) == owned) {
        // Stock mouse look may already have cached zero while our hover stayed visible.
        const Guid clear[] = {0, owned};
        Function<HoverChange>(0x51FB60)(clear);
        Function<TrackObject>(0x4F5980)(world, 0);
    }
}

void SetSoftHover(void* world, Guid pick) {
    const Guid previous = g_softHover;
    g_softHover = pick;
    g_softWorld = pick ? world : nullptr;
    // UI unit frames can change mouseover without changing the world's last hover GUID.
    if (Field<Guid>(world, 0x2C8) == pick && *reinterpret_cast<Guid*>(0xBD07A0) != pick)
        Function<TrackObject>(0x4F5980)(world, 0);
    Function<TrackObject>(0x4F5980)(world, pick);
    // Native mouseover may skip visual highlighting during mouse look; own only its hover bit.
    UpdateSoftHighlight(previous, pick);
}

void ReleaseMouse(const char* reason) {
    if (!SetHoverHook(false)) Log("Could not restore stock hover CALL");
    ClearSoftHover();
    g_clickSelection = {};
    g_leftMouseConsumed = false;
    void* input = *reinterpret_cast<void**>(0xC24954);
    if (g_captured && input) Function<LuaFunction>(0x5FC890)(nullptr);
    g_captured = false;
    g_active = false;
    g_pick = 0;
    g_selection = {};
    g_status = reason;
    g_interactPending = false;
    g_cursorInteractPending = false;
    if (!g_cameraEnabled) RestoreCursorWindow();
}

bool EnsureApi(Lua* state) {
    const int top = Function<GetTop>(0x84DBD0)(state);
    Function<GetField>(0x84E590)(state, -10002, kApiName);
    const bool present = Function<Type>(0x84DEB0)(state, -1) == 6;
    Function<SetTop>(0x84DBF0)(state, top);
    if (present) return true;
    ReleaseMouse("waiting for addon");
    g_enabled = false;
    g_cameraEnabled = false;
    Function<Register>(0x817F90)(kApiName, &NativeApi);
    Log("Registered NozdorActionNative on the game thread");
    return false;
}

bool IsForeground() {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
}

void* NotifyHoverChange(const Guid* original, bool managed, Guid desired, Guid actual, HoverChange forward) {
    if (!managed) return forward(original);
    if (desired == actual) return nullptr;
    const Guid replacement[] = {desired, actual};
    return forward(replacement);
}

void* __cdecl HoverChangeHook(const Guid* change) {
    if (!g_cameraEnabled) return Function<HoverChange>(0x51FB60)(change);
    const bool managed = g_softTarget && g_autoTarget && g_active && g_enabled && g_cursorInputAllowed &&
        GetTickCount() - g_heartbeat <= 500 && IsForeground() && !CursorIsFree();
    return NotifyHoverChange(change, managed, g_softHover, *reinterpret_cast<Guid*>(0xBD07A0),
                             Function<HoverChange>(0x51FB60));
}

bool SetHoverHook(bool enabled) {
    return ToggleCall(g_hoverHookInstalled, enabled, reinterpret_cast<unsigned char*>(0x4F59C0),
                      reinterpret_cast<void*>(0x51FB60), reinterpret_cast<void*>(&HoverChangeHook));
}

bool CursorMouseMessage(UINT message, bool bound, bool allowed, bool overUI) {
    if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) {
        if (g_rightMouseConsumed && g_cursorMode.wasDown) return true;
        // A free cursor can release outside the window without delivering WM_RBUTTONUP.
        g_rightMouseConsumed = false;
        if (!bound || (overUI && (!allowed || (g_cursorToggle && g_cursorMode.latched)))) return false;
        g_rightMouseConsumed = true;
        if (allowed) g_cursorMode.Step(true, g_cursorToggle, g_cameraEnabled, true);
        return true;
    }
    if (message == WM_RBUTTONUP && g_rightMouseConsumed) {
        g_rightMouseConsumed = false;
        g_cursorMode.Step(false, g_cursorToggle, g_cameraEnabled, allowed);
        return true;
    }
    return false;
}

WPARAM CursorMouseFlags(UINT message, WPARAM flags, bool reservedRightMouse) {
    return reservedRightMouse && message >= WM_MOUSEFIRST && message <= WM_MOUSELAST ? flags & ~WPARAM(MK_RBUTTON) : flags;
}

bool MaskReservedRightMouse() {
    BYTE state[256];
    if (!GetKeyboardState(state)) return false;
    if (!(state[VK_RBUTTON] & 0x80)) return true;
    state[VK_RBUTTON] &= 0x7f;
    return SetKeyboardState(state) != 0;
}

bool SelectionMouseMessage(UINT message, bool allowed, DWORD now) {
    if (message == WM_LBUTTONUP && g_leftMouseConsumed) {
        g_leftMouseConsumed = false;
        return true;
    }
    if (message != WM_LBUTTONDOWN && message != WM_LBUTTONDBLCLK) return false;
    if (g_leftMouseConsumed) return true;
    if (!allowed) return false;
    g_leftMouseConsumed = true;
    // Rejected units and non-targetable objects are not empty terrain.
    if (g_pick || g_probe.kind == 0 || g_probe.kind == 1) g_clickSelection.Queue(g_pick, now);
    return true;
}

LRESULT CALLBACK CursorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    // Full OFF must pass even a gesture started before F6 to the stock client unchanged.
    if (!g_cameraEnabled) {
        ResetMouseGestures();
        return CallWindowProcW(g_previousWindowProc, window, message, wparam, lparam);
    }
    if (g_rightMouseConsumed) {
        // Only the client thread's logical RMB is hidden; physical polling and modifiers stay intact.
        if (!MaskReservedRightMouse()) Log("Could not mask reserved RMB state");
        wparam = CursorMouseFlags(message, wparam, true);
    }
    const DWORD now = GetTickCount();
    const bool freeCursor = CursorIsFree();
    if (freeCursor) { g_leftMouseConsumed = false; g_clickSelection = {}; }
    if (SelectionMouseMessage(message, g_softTarget && g_autoTarget && g_active && g_cameraEnabled &&
        g_cursorInputAllowed && !g_cursorOverUI && now - g_heartbeat <= 500 &&
        GetForegroundWindow() == window && !freeCursor, now)) return 0;
    if (message != WM_RBUTTONDOWN && message != WM_RBUTTONUP && message != WM_RBUTTONDBLCLK) {
        if (message == WM_KILLFOCUS) {
            ResetMouseGestures();
        }
        return CallWindowProcW(g_previousWindowProc, window, message, wparam, lparam);
    }
    const bool rightMouse = g_cursorKeys[0] == VK_RBUTTON || g_cursorKeys[1] == VK_RBUTTON;
    const bool live = g_cameraEnabled && GetTickCount() - g_heartbeat <= 500 && GetForegroundWindow() == window;
    const unsigned held = ((GetKeyState(VK_SHIFT) & 0x8000) ? 1u : 0u) |
        ((GetKeyState(VK_CONTROL) & 0x8000) ? 2u : 0u) |
        ((GetKeyState(VK_MENU) & 0x8000) ? 4u : 0u);
    bool bound = false;
    for (unsigned i = 0; i < 2; ++i)
        if (g_cursorKeys[i] == VK_RBUTTON && action::CursorBindingDown(VK_RBUTTON, g_cursorModifiers[i], held, true)) bound = true;
    if (CursorMouseMessage(message, live && bound, live && g_cursorInputAllowed, g_cursorOverUI)) {
        if (!MaskReservedRightMouse()) Log("Could not mask reserved RMB state");
        return 0;
    }
    // A bound RMB over the world stays blocked while UI is open, even with a different modifier held.
    if ((message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) && live && rightMouse &&
        !g_cursorInputAllowed && !g_cursorOverUI) {
        g_rightMouseConsumed = true;
        if (!MaskReservedRightMouse()) Log("Could not mask reserved RMB state");
        return 0;
    }
    return CallWindowProcW(g_previousWindowProc, window, message, wparam, lparam);
}

bool RestoreCursorWindow() {
    ResetMouseGestures();
    if (!g_cursorWindow) return true;
    if (IsWindow(g_cursorWindow)) {
        // A later addon may have chained our procedure; preserve that chain and pass through.
        if (GetWindowLongPtrW(g_cursorWindow, GWLP_WNDPROC) != reinterpret_cast<LONG_PTR>(&CursorWindowProc)) return true;
        if (!SetWindowLongPtrW(g_cursorWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_previousWindowProc))) return false;
    }
    g_cursorWindow = nullptr;
    g_previousWindowProc = nullptr;
    return true;
}

bool EnsureCursorWindow() {
    HWND window = Function<GetWindow>(0x86C6A0)(0);
    if (!window) return false;
    if (window == g_cursorWindow) return true;
    if (g_cursorWindow && IsWindow(g_cursorWindow) &&
        GetWindowLongPtrW(g_cursorWindow, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&CursorWindowProc))
        SetWindowLongPtrW(g_cursorWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_previousWindowProc));
    const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&CursorWindowProc)));
    if (!previous) return false;
    g_previousWindowProc = previous;
    g_cursorWindow = window;
    ResetMouseGestures();
    return true;
}

void ReturnFreeCursor() {
    if (!g_cursorReturn || *reinterpret_cast<unsigned*>(0xD4156C) != 0) return;
    RECT rect{};
    HWND window = Function<GetWindow>(0x86C6A0)(0);
    if (!window || !GetClientRect(window, &rect) || rect.right <= 0 || rect.bottom <= 0) return;
    if (!g_cursorCenter && !g_cursorSaved) { g_cursorReturn = false; return; }
    const auto point = action::CursorPosition(g_cursorCenter, g_centerX, g_centerY, rect.right, rect.bottom,
                                               {g_previousCursor.x, g_previousCursor.y});
    Function<SetCursor>(0x86A130)(point.x, point.y);
    g_cursorReturn = false;
}

void SetZoom(void* camera, float wanted, float duration = 0) {
    const float current = Field<float>(camera, 0x1E8);
    if (!std::isfinite(current) || current < 0 || current > 60 || std::fabs(current - wanted) < 0.01f) return;
    const auto address = current > wanted ? 0x5FF950 : 0x5FFA60;
    Function<Zoom>(address)(camera, std::fabs(current - wanted), *reinterpret_cast<DWORD*>(0xB499A4), duration);
}

void PrepareCamera(void* world) {
    g_cameraReady = false;
    g_cameraApplied = false;
    if (world != *reinterpret_cast<void**>(0xB7436C)) return;
    void* camera = Field<void*>(world, 0x7E20);
    if (!camera) return;
    const Guid player = Function<GetPlayer>(0x4D3790)();
    const bool ownsCamera = player && Field<Guid>(camera, 0x88) == player;
    const DWORD now = GetTickCount();
    const auto profile = action::SelectCameraProfile(g_mounted,
        {g_shoulder, g_height, g_distance, g_stockCamera},
        {g_mountShoulder, g_mountCameraHeight, g_mountCameraDistance, g_mountStockCamera},
        g_mountCustom, g_mountAdapt, g_mountDistance, g_mountHeight);
    const bool stock = profile.stock;
    bool wanted = g_cameraEnabled && !stock && ownsCamera && now - g_heartbeat <= 500 && IsForeground();
    if (wanted) {
        void* unit = Function<Resolve>(0x4D4DB0)(player, 0x10, "NozdorAction", 4);
        wanted = unit && !Function<UnitTest>(0x716490)(unit);
    }
    if (!wanted) {
        if (camera == g_cameraSession && ownsCamera && g_cameraOwner == player) SetZoom(camera, g_savedZoom, stock ? 0.4f : 0);
        g_cameraSession = nullptr;
        g_cameraOwner = 0;
        g_activeCameraSide = 0; g_activeCameraHeight = 0;
        g_profileZoom = {}; g_cameraTime = now;
        return;
    }
    if (camera != g_cameraSession || player != g_cameraOwner) {
        const float zoom = Field<float>(camera, 0x1E8);
        if (!std::isfinite(zoom) || zoom < 0 || zoom > 60) return;
        g_cameraSession = camera;
        g_cameraOwner = player;
        g_savedZoom = zoom;
        g_zoomPending = true;
        g_activeCameraSide = 0; g_activeCameraHeight = 0;
        g_profileZoom = {}; g_cameraTime = now;
    }
    const float zoomOffset = profile.distance - g_distance;
    const auto zoom = g_profileZoom.Step(Field<float>(camera, 0x1E8), g_distance, zoomOffset, now, g_zoomPending);
    if (zoom.change) { SetZoom(camera, zoom.distance, 0.4f); g_zoomPending = false; }
    const float seconds = static_cast<float>(now - g_cameraTime) / 1000;
    g_activeCameraSide = action::MountLift(g_activeCameraSide, profile.side, seconds);
    g_activeCameraHeight = action::MountLift(g_activeCameraHeight, profile.height, seconds);
    g_cameraTime = now;
    g_cameraReady = true;
}

int __attribute__((fastcall)) CameraCollisionHook(void* camera, void*, Vec3* focus,
    float* distance, float* height, Vec3* extra, float elevation, float* scale) {
    const auto original = Function<Collision>(0x605D60);
    const int vanilla = original(camera, focus, distance, height, extra, elevation, scale);
    if (!g_cameraReady || camera != g_cameraSession) return vanilla;
    Vec3 forward{}, up{};
    Function<Position>(0x600C20)(camera, &forward);
    Function<Position>(0x600D60)(camera, &up);
    Vec3 pivot = action::Add(*focus, *extra);
    pivot.z += *height;
    const Vec3 delta = action::ShoulderDelta(forward, up, g_activeCameraSide, g_activeCameraHeight, *distance);
    const Vec3 clipped = action::ClipShoulder(pivot, delta, action::Cross(forward, up), up,
        [](const Vec3& start, const Vec3& end, float& fraction) {
            return Function<TraceLine>(0x7A3B70)(&start, &end, nullptr, &fraction, 0x100171, nullptr);
        });
    if (action::Length(clipped) < 0.0001f) return vanilla;
    *focus = action::Add(*focus, clipped);
    g_cameraApplied = true;
    if (!g_cameraLogged) { Log("Shoulder focus adjusted before native camera collision"); g_cameraLogged = true; }
    // Re-run native boom collision with the shifted focus; final position and matrices use this same focus.
    return original(camera, focus, distance, height, extra, elevation, scale);
}

const char* RejectTarget(void* player, void* target, float& distance) {
    if (!target) return "GUID does not resolve to a unit";
    if (target == player) return "hit local player";
    void* descriptor = Field<void*>(target, 0xD0);
    if (!descriptor) return "missing unit descriptor";
    Vec3 source{}, destination{};
    Function<Position>(0x6E6EF0)(player, &source);
    Function<Position>(0x6E6EF0)(target, &destination);
    const float dx = source.x - destination.x;
    const float dy = source.y - destination.y;
    const float dz = source.z - destination.z;
    const float squared = dx * dx + dy * dy + dz * dz;
    if (!std::isfinite(squared)) return "invalid unit coordinates";
    distance = std::sqrt(squared);
    return squared <= g_range * g_range ? nullptr : "out of acquisition range";
}

Guid PickUnit(void* world, void* player, float cursorX = 0.5f, float cursorY = 0.5f) {
    g_probe = {};
    const float left = Field<float>(world, 0x324), right = Field<float>(world, 0x32C);
    const float bottom = Field<float>(world, 0x320), top = Field<float>(world, 0x328);
    g_probe.left = left; g_probe.right = right; g_probe.bottom = bottom; g_probe.top = top;
    if (!std::isfinite(left + right + bottom + top) || right <= left || top <= bottom) {
        g_probe.reason = "invalid viewport";
        return 0;
    }
    PickResult hit{};
    const auto head = Field<std::uintptr_t>(world, 0x2A4);
    g_probe.models = head && !(head & 1);
    const int kind = Function<Pick>(0x4F9DA0)(world, left + (right - left) * cursorX,
                                           bottom + (top - bottom) * cursorY, 0, &hit);
    g_probe.kind = kind;
    g_probe.raw = hit.guid;
    g_probe.ray = {hit.end.x - hit.start.x, hit.end.y - hit.start.y, hit.end.z - hit.start.z};
    g_probe.origin = hit.start;
    if (kind < 2 || !hit.guid) {
        g_probe.reason = kind == 0 ? "no hit under crosshair" :
            kind == 1 ? "terrain under crosshair" : kind == 3 ? "world object under crosshair" : "model hit without GUID";
        return 0;
    }
    void* target = Function<Resolve>(0x4D4DB0)(hit.guid, 1, "NozdorAction", 1);
    void* descriptor = target ? Field<void*>(target, 8) : nullptr;
    g_probe.objectMask = descriptor ? Field<unsigned>(descriptor, 8) : 0;
    if (!(g_probe.objectMask & 8)) {
        g_probe.reason = (g_probe.objectMask & 0x20) ? "world object under crosshair" : "GUID does not resolve to a unit";
        return 0;
    }
    const char* rejection = RejectTarget(player, target, g_probe.distance);
    g_probe.reason = rejection ? rejection : "eligible";
    return rejection ? 0 : hit.guid;
}

Guid PickNameplate(void* player, Guid guid, Guid fallback, DWORD now) {
    if (!guid || now - g_plateTime > 250 || !g_cursorInputAllowed || g_cursorOverUI) return fallback;
    g_probe.kind = 2;
    g_probe.raw = guid;
    void* target = Function<Resolve>(0x4D4DB0)(guid, 8, "NozdorAction", 10);
    void* descriptor = target ? Field<void*>(target, 8) : nullptr;
    g_probe.objectMask = descriptor ? Field<unsigned>(descriptor, 8) : 0;
    const char* rejection = RejectTarget(player, target, g_probe.distance);
    g_probe.reason = rejection ? rejection : "eligible nameplate";
    return rejection ? 0 : guid;
}

void PublishProbe(DWORD now) {
    if (g_probeTime && now - g_probeTime < 1000) return;
    g_probeTime = now;
    std::snprintf(g_targetDiagnostic, sizeof(g_targetDiagnostic),
        "Target probe: %s; kind=%d; mask=%X; GUID=%08X%08X; distance=%.1f/%.1f; models=%d; rect=%.3f,%.3f,%.3f,%.3f; ray=%.1f,%.1f,%.1f; origin=%.1f,%.1f,%.1f",
        g_probe.reason, g_probe.kind, g_probe.objectMask, static_cast<unsigned>(g_probe.raw >> 32),
        static_cast<unsigned>(g_probe.raw), g_probe.distance, g_range, static_cast<int>(g_probe.models),
        g_probe.left, g_probe.right, g_probe.bottom, g_probe.top,
        g_probe.ray.x, g_probe.ray.y, g_probe.ray.z,
        g_probe.origin.x, g_probe.origin.y, g_probe.origin.z);
    Log(g_targetDiagnostic);
}

void InteractUnderCrosshair(void* player, DWORD now, bool uiReady) {
    const char* reason = "no interactable object under crosshair";
    if (!uiReady) reason = "UI execution context not ready";
    else if (now - g_interactTime > 250) reason = "interaction request expired";
    else if ((g_probe.kind == 2 || g_probe.kind == 3) && g_probe.raw) {
        void* object = Function<Resolve>(0x4D4DB0)(g_probe.raw, 1, "NozdorAction", 5);
        void* descriptor = object ? Field<void*>(object, 8) : nullptr;
        const unsigned mask = descriptor ? Field<unsigned>(descriptor, 8) : 0;
        bool allowed = false;
        if ((mask & 8) && !(mask & 0x10) && object != player) {
            if (Function<UnitTest>(0x716490)(object))
                allowed = Function<Lootable>(0x717B60)(object, *reinterpret_cast<DWORD*>(0xCD76AC));
            else allowed = Function<UnitRelation>(0x729530)(player, object);
            reason = "unit has no NPC interaction or available loot";
        } else if (mask & 0x20) allowed = true;
        if (allowed) {
            reason = Function<InteractGuid>(0x5277B0)(g_probe.raw) ? "native interaction dispatched" : "native interaction refused";
        }
    }
    g_interactPending = false;
    g_pick = 0;
    g_selection = {};
    g_interactPause = now;
    g_probe.reason = reason;
    g_probeTime = 0;
    PublishProbe(now);
}

void Tick(void* world) {
    ++g_frames;
    Lua* state = Function<GetState>(0x817DB0)();
    if (!state) { ReleaseMouse("no UI"); return; }
    if (!EnsureApi(state)) return;
    const DWORD now = GetTickCount();
    const bool foreground = IsForeground();
    if (g_cameraEnabled && (g_softTarget || g_cursorKeys[0] == VK_RBUTTON || g_cursorKeys[1] == VK_RBUTTON) && !EnsureCursorWindow()) {
        ReleaseMouse("right mouse unavailable"); return;
    }
    const bool freeCursor = g_cursorMode.Step(CursorKeyHeld(), g_cursorToggle, g_cameraEnabled,
        g_cursorInputAllowed && foreground && now - g_heartbeat <= 500);
    if (now - g_heartbeat > 500) { ReleaseMouse("addon heartbeat expired"); return; }
    if (!foreground) { g_cursorReturn = false; ReleaseMouse("focus"); return; }
    if (freeCursor) {
        const bool owned = g_captured;
        const bool cursorRequest = g_cursorInteractPending;
        ReleaseMouse("cursor key");
        if (owned) g_cursorReturn = true;
        ReturnFreeCursor();
        if (cursorRequest && g_cursorPick.valid && g_cursorInputAllowed && now - g_interactTime <= 250 &&
            world == *reinterpret_cast<void**>(0xB7436C) && Field<void*>(world, 0x7E20) &&
            !*reinterpret_cast<void**>(0xD3F4E4)) {
            const Guid guid = Function<GetPlayer>(0x4D3790)();
            void* player = Function<Resolve>(0x4D4DB0)(guid, 0x10, "NozdorAction", 6);
            if (player && !Function<UnitTest>(0x716490)(player)) {
                const Guid body = PickUnit(world, player, g_cursorPick.x, g_cursorPick.y);
                PickNameplate(player, g_cursorInteractPlate, body, now);
                InteractUnderCrosshair(player, now, *reinterpret_cast<unsigned*>(0xD4139C) == 0);
            }
        }
        return;
    }
    g_cursorReturn = false;
    if (!g_enabled || !g_cursorInputAllowed) { ReleaseMouse("disabled"); return; }
    // Wait for interaction UI without releasing capture on failed/instant actions.
    if (g_interactPause && now - g_interactPause < 200) return;
    if (*reinterpret_cast<void**>(0xD3F4E4)) { ReleaseMouse("spell targeting"); return; }
    void* input = *reinterpret_cast<void**>(0xC24954);
    if (!input || world != *reinterpret_cast<void**>(0xB7436C) ||
        !Field<void*>(world, 0x7E20)) { ReleaseMouse("no world"); return; }
    const Guid guid = Function<GetPlayer>(0x4D3790)();
    void* player = Function<Resolve>(0x4D4DB0)(guid, 0x10, "NozdorAction", 2);
    if (!player || Function<UnitTest>(0x716490)(player)) { ReleaseMouse("no living player"); return; }
    if ((Field<unsigned>(input, 4) & 1) == 0) {
        if (*reinterpret_cast<unsigned*>(0xD4156C) == 0)
            Function<GetCursor>(0x86A0D0)(&g_previousCursor.x, &g_previousCursor.y);
        else g_previousCursor = *reinterpret_cast<POINT*>(0xD413F8);
        g_cursorSaved = true;
        Function<LuaFunction>(0x5FCC10)(nullptr);
        g_captured = true;
    }
    g_active = true;
    g_status = "active";
    if (!g_autoTarget && !g_interactPending) {
        SetHoverHook(false);
        ClearSoftHover(); g_clickSelection = {};
        g_pick = 0; g_selection = {}; g_probe = {};
        g_probe.reason = "automatic targeting disabled in addon"; PublishProbe(now); return;
    }
    if ((Field<unsigned>(input, 0x58) & 1) == 0) {
        SetHoverHook(false);
        ClearSoftHover(); g_clickSelection = {};
        g_pick = 0;
        g_selection = {};
        g_status = "picking unavailable";
        g_probe = {}; g_probe.reason = "native screen picking disabled by input mode"; PublishProbe(now);
        return;
    }
    const Guid body = PickUnit(world, player);
    g_pick = PickNameplate(player, g_aimPlate, body, now);
    if (g_softTarget && g_autoTarget) {
        if (!SetHoverHook(true)) { ReleaseMouse("hover hook unavailable"); return; }
        SetSoftHover(world, action::HoverGuid(g_probe.kind, g_probe.raw, g_pick, g_probe.objectMask));
    } else {
        SetHoverHook(false);
        ClearSoftHover();
    }
    if (g_interactPending) {
        g_clickSelection = {};
        InteractUnderCrosshair(player, now, *reinterpret_cast<unsigned*>(0xD4139C) == 0); return;
    }
    if (!g_autoTarget) { g_pick = 0; g_selection = {}; PublishProbe(now); return; }
    const Guid actual = *reinterpret_cast<Guid*>(0xBD07B0);
    if (g_softTarget) {
        g_selection = {};
        const auto clicked = g_clickSelection.Take(now);
        if (clicked.change && !clicked.target) {
            Function<SetTarget>(0x524BF0)(0);
            g_probe.reason = "target cleared by empty click";
        } else if (clicked.change && clicked.target != actual) {
            void* target = Function<Resolve>(0x4D4DB0)(clicked.target, 8, "NozdorAction", 7);
            if (const char* rejection = RejectTarget(player, target, g_probe.distance)) g_probe.reason = rejection;
            else {
                Function<SetTarget>(0x524BF0)(clicked.target);
                g_probe.reason = *reinterpret_cast<Guid*>(0xBD07B0) == clicked.target ?
                    "target selected by click" : "native Target did not assign requested GUID";
            }
        } else if (g_pick) g_probe.reason = "soft target: click LMB to select";
        PublishProbe(now);
        return;
    }
    g_clickSelection = {};
    const action::Decision decision = g_selection.Step(g_pick, actual, now, g_delay, g_keepTarget);
    if (!decision.change) {
        if (g_pick) g_probe.reason = g_pick == actual ? "target already selected" : "waiting for switch confirmation";
        PublishProbe(now);
        return;
    }
    if (decision.target) {
        void* target = Function<Resolve>(0x4D4DB0)(decision.target, 8, "NozdorAction", 3);
        if (const char* rejection = RejectTarget(player, target, g_probe.distance)) {
            g_probe.reason = rejection; PublishProbe(now); return;
        }
    }
    Function<SetTarget>(0x524BF0)(decision.target);
    if (*reinterpret_cast<Guid*>(0xBD07B0) == decision.target) {
        g_selection.owned = decision.target;
        g_probe.reason = decision.target ? "target selected" : "lost target cleared";
    } else g_probe.reason = "native Target did not assign requested GUID";
    PublishProbe(now);
}

int __attribute__((fastcall)) WorldUpdateHook(void* world, void*) {
    if (g_inFrame) return Function<UpdateWorld>(0x4FA5F0)(world);
    g_inFrame = true;
    // Native picking needs the last rendered model list; OnWorldUpdate clears it at 0x4FAA5A.
    Tick(world);
    PrepareCamera(world);
    const int result = Function<UpdateWorld>(0x4FA5F0)(world);
    g_inFrame = false;
    return result;
}

bool Install() {
    if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != 0x400000) {
        Log("REFUSED: executable image base is not 0x400000");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(0x400000);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x800) {
        Log("REFUSED: unsupported PE headers");
        return false;
    }
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(0x400000 + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    for (const CodeGuard& guard : kCodeGuards) {
        if (guard.address < 0x400000 || guard.address + guard.size >
            0x400000 + pe->OptionalHeader.SizeOfImage) {
            Log("REFUSED: code guard is outside this image");
            return false;
        }
        std::uintptr_t cursor = guard.address;
        while (cursor < guard.address + guard.size) {
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(reinterpret_cast<void*>(cursor), &page, sizeof(page)) ||
                page.State != MEM_COMMIT || (page.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                Log("REFUSED: unreadable runtime code range");
                return false;
            }
            cursor = reinterpret_cast<std::uintptr_t>(page.BaseAddress) + page.RegionSize;
        }
        const auto* bytes = reinterpret_cast<const unsigned char*>(guard.address);
        if (action::HashBytes(bytes, guard.size) != guard.hash) {
            char error[100];
            std::snprintf(error, sizeof(error), "REFUSED: runtime code differs at 0x%08X",
                          static_cast<unsigned>(guard.address));
            Log(error);
            return false;
        }
    }
    if (!ReplaceCall(reinterpret_cast<unsigned char*>(0x6074D5),
                     reinterpret_cast<void*>(0x605D60),
                     reinterpret_cast<void*>(&CameraCollisionHook))) {
        Log("REFUSED: could not replace camera collision CALL");
        return false;
    }
    if (!ReplaceCall(reinterpret_cast<unsigned char*>(0x4FB031),
                     reinterpret_cast<void*>(0x4FA5F0),
                     reinterpret_cast<void*>(&WorldUpdateHook))) {
        Log("REFUSED: could not replace the verified OnWorldUpdate CALL");
        ReplaceCall(reinterpret_cast<unsigned char*>(0x6074D5),
                    reinterpret_cast<void*>(&CameraCollisionHook), reinterpret_cast<void*>(0x605D60));
        return false;
    }
    Log("Installed frame and shoulder CALL hooks; mouse/hover hooks wait until mode ON");
    return true;
}
} // namespace

extern "C" __declspec(dllexport) unsigned __cdecl NozdorActionVersion() { return 9; }

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        OpenLog(self);
        Log("NozdorAction 0.3.20 / mounted profiles / stock camera / reticles / x86 / exact client code guards");
        if (!Install()) {
            if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
            g_log = INVALID_HANDLE_VALUE;
            return FALSE;
        }
        // The frame CALL must never point into an unloaded DLL.
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
    } else if (reason == DLL_PROCESS_DETACH && reserved && g_log != INVALID_HANDLE_VALUE) {
        CloseHandle(g_log);
    }
    return TRUE;
}
