#include "../src/native.cpp"
#include <cassert>

WPARAM forwardedFlags = 0;
unsigned hoverNotifications = 0;
Guid hoverNew = 0, hoverOld = 0;
void* __cdecl TestHoverChange(const Guid* pair) {
    ++hoverNotifications; hoverNew = pair[0]; hoverOld = pair[1];
    return reinterpret_cast<void*>(123);
}
LRESULT CALLBACK TestWindowProc(HWND, UINT, WPARAM flags, LPARAM) {
    forwardedFlags = flags;
    assert(!(GetKeyState(VK_RBUTTON) & 0x8000));
    assert(GetKeyState(VK_CONTROL) & 0x8000);
    assert(GetKeyState(VK_SHIFT) & 0x8000);
    assert(GetKeyState(VK_MENU) & 0x8000);
    return 123;
}

int main() {
    const Guid nativeClear[] = {0, 42}, nativeOther[] = {99, 0};
    assert(!NotifyHoverChange(nativeClear, true, 42, 42, TestHoverChange));
    assert(!NotifyHoverChange(nativeOther, true, 42, 42, TestHoverChange));
    assert(hoverNotifications == 0); // Native mouse look cannot clear/re-highlight the same soft target.
    assert(NotifyHoverChange(nativeOther, true, 43, 42, TestHoverChange));
    assert(hoverNotifications == 1 && hoverNew == 43 && hoverOld == 42);
    assert(NotifyHoverChange(nativeOther, true, 0, 43, TestHoverChange));
    assert(hoverNotifications == 2 && hoverNew == 0 && hoverOld == 43);
    assert(NotifyHoverChange(nativeOther, false, 42, 42, TestHoverChange));
    assert(hoverNotifications == 3 && hoverNew == 99 && hoverOld == 0);
    g_pick = 42;
    assert(!SelectionMouseMessage(WM_LBUTTONDOWN, false, 1000));
    assert(g_clickSelection.Take(1000) == 0);
    assert(SelectionMouseMessage(WM_LBUTTONDOWN, true, 1001));
    g_pick = 43;
    assert(SelectionMouseMessage(WM_LBUTTONDOWN, true, 1002));
    assert(g_clickSelection.Take(1003) == 42); // Keep the clicked unit when the camera moves.
    assert(SelectionMouseMessage(WM_LBUTTONUP, false, 1004));
    assert(!SelectionMouseMessage(WM_LBUTTONUP, true, 1005));
    assert(!SelectionMouseMessage(WM_RBUTTONDOWN, true, 1006));
    assert(SelectionMouseMessage(WM_LBUTTONDBLCLK, true, 1007));
    assert(g_clickSelection.Take(1008) == 43);
    assert(SelectionMouseMessage(WM_LBUTTONUP, true, 1009));
    g_pick = 0;
    assert(SelectionMouseMessage(WM_LBUTTONDOWN, true, 1010));
    assert(g_clickSelection.Take(1011) == 0);
    assert(SelectionMouseMessage(WM_LBUTTONUP, false, 1012));
    for (int scenario = 0; scenario < 3; ++scenario) {
        g_probe = {};
        g_interactTime = scenario == 1 ? 1 : 1000;
        g_interactPending = true;
        g_captured = true;
        g_active = true;
        g_pick = 42;
        g_selection.owned = 42;
        InteractUnderCrosshair(nullptr, 1000, scenario != 2);
        assert(!g_interactPending && !g_pick && !g_selection.owned);
        assert(g_captured && g_active);
        assert(g_interactPause == 1000);
    }
    g_cameraEnabled = true;
    g_cursorToggle = true;
    g_cursorMode = {};
    g_cursorMode.Step(false, true, true, true);
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, true, false));
    assert(g_cursorMode.latched);
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, true, false));
    assert(g_cursorMode.latched);
    assert(CursorMouseMessage(WM_RBUTTONUP, true, true, false));
    assert(g_cursorMode.latched);
    assert(CursorMouseMessage(WM_RBUTTONDBLCLK, true, true, false));
    assert(!g_cursorMode.latched);
    assert(CursorMouseMessage(WM_RBUTTONUP, true, true, false));
    // Open bags/options: world clicks are swallowed without toggling or native mouse look.
    g_cursorMode.Step(false, true, true, false);
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, false, false));
    assert(CursorMouseMessage(WM_RBUTTONUP, true, false, false));
    assert(!g_cursorMode.latched);
    // Right clicks on the actual UI are passed through.
    assert(!CursorMouseMessage(WM_RBUTTONDOWN, true, false, true));
    assert(!CursorMouseMessage(WM_RBUTTONUP, true, false, true));
    assert(!g_cursorMode.latched);
    g_cursorMode.latched = true;
    assert(!CursorMouseMessage(WM_RBUTTONDOWN, true, false, true));
    assert(g_cursorMode.latched);
    // Finish an intercepted gesture even if a window opens between down and up.
    g_cursorMode.Step(false, true, true, true);
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, true, false));
    const bool beforeUI = g_cursorMode.latched;
    assert(CursorMouseMessage(WM_RBUTTONUP, true, false, true));
    assert(g_cursorMode.latched == beforeUI);
    assert(!CursorMouseMessage(WM_RBUTTONDOWN, false, true, false));
    assert(!CursorMouseMessage(WM_LBUTTONDOWN, true, true, false));
    g_cursorMode.Step(false, true, true, true);
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, true, false));
    g_cursorMode.Step(false, true, true, true); // Physical release outside the window.
    const bool beforeReturn = g_cursorMode.latched;
    assert(CursorMouseMessage(WM_RBUTTONDOWN, true, true, false));
    assert(g_cursorMode.latched != beforeReturn);
    assert(CursorMouseMessage(WM_RBUTTONUP, true, true, false));
    g_cursorMode.latched = true;
    assert(!CursorMouseMessage(WM_RBUTTONDOWN, true, true, true));
    assert(!g_rightMouseConsumed && g_cursorMode.latched);
    const WPARAM leftCtrl = MK_LBUTTON | MK_CONTROL;
    assert(CursorMouseFlags(WM_LBUTTONDOWN, leftCtrl | MK_RBUTTON, true) == leftCtrl);
    assert(CursorMouseFlags(WM_LBUTTONUP, MK_CONTROL | MK_RBUTTON, true) == MK_CONTROL);
    assert(CursorMouseFlags(WM_MOUSEMOVE, MK_SHIFT | MK_RBUTTON, true) == MK_SHIFT);
    assert(CursorMouseFlags(WM_XBUTTONDOWN, (WPARAM(XBUTTON1) << 16) | MK_RBUTTON, true) == (WPARAM(XBUTTON1) << 16));
    assert(CursorMouseFlags(WM_KEYDOWN, 'B', true) == 'B');
    assert(CursorMouseFlags(WM_LBUTTONDOWN, leftCtrl | MK_RBUTTON, false) == (leftCtrl | MK_RBUTTON));
    BYTE original[256], held[256];
    assert(GetKeyboardState(original));
    std::memcpy(held, original, sizeof(held));
    held[VK_RBUTTON] = held[VK_CONTROL] = held[VK_SHIFT] = held[VK_MENU] = 0x80;
    assert(SetKeyboardState(held));
    g_rightMouseConsumed = true;
    g_previousWindowProc = TestWindowProc;
    assert(CursorWindowProc(nullptr, WM_LBUTTONDOWN, leftCtrl | MK_RBUTTON, 0) == 123);
    assert(forwardedFlags == leftCtrl);
    assert(SetKeyboardState(original));
    g_rightMouseConsumed = false;
    std::puts("PASS: interaction consumes one request without releasing mouse capture");
    std::puts("PASS: RMB toggles once per click; open UI blocks mouse look and preserves UI clicks");
    std::puts("PASS: forwarded Ctrl+LMB and client thread state exclude reserved RMB and preserve modifiers");
}
