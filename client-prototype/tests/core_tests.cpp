#include <cassert>
#include <cstdio>
#include <limits>
#include "../src/selection.h"
#include "../src/call_hook.h"
#include "../src/camera_math.h"

int main(int argc, char** argv) {
    assert(action::HoverGuid(2, 42, 42, 0x19) == 42); // Player or NPC.
    assert(action::HoverGuid(2, 42, 0, 0x19) == 0); // Rejected unit stays rejected.
    assert(action::HoverGuid(3, 43, 0, 0x21) == 43); // Mailbox / ore game object.
    assert(action::HoverGuid(2, 43, 0, 0x21) == 43); // Dynamic models also return kind 2 for game objects.
    assert(action::HoverGuid(3, 43, 0, 0x19) == 0);
    assert(action::HoverGuid(1, 43, 0, 0x21) == 0);
    assert(action::HoverGuid(0, 0, 0, 0) == 0);
    unsigned highlighted[2] = {}, cleared[2] = {};
    int objects[2] = {0, 1};
    auto lookup = [&](std::uint64_t guid) -> int* { return guid == 42 ? &objects[0] : guid == 43 ? &objects[1] : nullptr; };
    auto highlight = [&](int* object, bool enabled) { ++(enabled ? highlighted[*object] : cleared[*object]); };
    action::UpdateHoverHighlight(0, 42, lookup, highlight);
    action::UpdateHoverHighlight(42, 42, lookup, highlight);
    assert(highlighted[0] == 2 && cleared[0] == 0);
    action::UpdateHoverHighlight(42, 43, lookup, highlight);
    assert(cleared[0] == 1 && highlighted[1] == 1);
    action::UpdateHoverHighlight(43, 0, lookup, highlight);
    assert(cleared[1] == 1);
    action::UpdateHoverHighlight(99, 99, lookup, highlight); // Despawned objects are not dereferenced.
    action::ClickSelection click;
    assert(!click.Take(1000).change);
    click.Queue(42, 1000);
    auto chosen = click.Take(1010);
    assert(chosen.change && chosen.target == 42 && !click.Take(1010).change);
    click.Queue(43, 1000);
    assert(!click.Take(1251).change);
    click.Queue(44, 0xfffffff0u);
    chosen = click.Take(10);
    assert(chosen.change && chosen.target == 44);
    click.Queue(45, 1000); click.Queue(0, 1001);
    chosen = click.Take(1002);
    assert(chosen.change && !chosen.target && !click.Take(1002).change);
    click.Queue(0, 1000);
    assert(!click.Take(1251).change); // An expired empty click cannot clear a target.
    std::uint64_t guid = 0;
    assert(action::ParseGuid("0xF1301234ABCDEF01", 18, guid) && guid == 0xF1301234ABCDEF01ull);
    assert(action::ParseGuid("0Xf1301234abcdef01", 18, guid) && guid == 0xF1301234ABCDEF01ull);
    assert(action::ParseGuid("", 0, guid) && !guid);
    assert(!action::ParseGuid("0x123", 5, guid));
    assert(!action::ParseGuid("0xF1301234ABCDEFO1", 18, guid));
    assert(!action::ParseGuid("0xF1301234ABCDEF01tail", 22, guid));
    assert(!action::ParseGuid("0xF1301234ABCDE\0" "01", 18, guid));
    if (argc == 2) {
        SetLastError(0);
        assert(LoadLibraryA(argv[1]) == nullptr);
        assert(GetLastError() == ERROR_DLL_INIT_FAILED);
        std::puts("PASS: production DLL entered its initializer and refused the foreign host");
        return 0;
    }
    assert(action::ValidSettings(40, 100));
    assert(action::ValidSettings(5, 0));
    assert(action::ValidSettings(200, 100));
    assert(action::ValidSettings(300, 500));
    assert(!action::ValidSettings(0, 100));
    assert(!action::ValidSettings(40, -1));
    assert(!action::ValidSettings(301, 100));
    assert(!action::ValidSettings(std::numeric_limits<double>::infinity(), 100));
    assert(!action::ValidSettings(std::numeric_limits<double>::quiet_NaN(), 100));
    assert(!action::ValidSettings(40, std::numeric_limits<double>::infinity()));
    action::Selection state;
    assert(!state.Step(11, 0, 100, 100).change);
    assert(!state.Step(11, 0, 199, 100).change);
    auto decision = state.Step(11, 0, 200, 100);
    assert(decision.change && decision.target == 11);
    state.owned = 11;
    assert(!state.Step(11, 11, 210, 100).change);
    assert(!state.Step(0, 11, 459, 100).change);
    decision = state.Step(0, 11, 460, 100);
    assert(decision.change && decision.target == 0);
    assert(!state.Step(0, 22, 1000, 100).change);
    assert(state.owned == 0); // Never clear somebody else's/manual target.
    assert(!state.Step(33, 22, 1001, 100).change);
    assert(!state.Step(44, 22, 1099, 100).change);
    assert(!state.Step(44, 22, 1198, 100).change);
    assert(state.Step(44, 22, 1199, 100).change);
    state = {};
    assert(state.Step(55, 0, 0, 0).change);
    state = {};
    assert(!state.Step(66, 0, 0xFFFFFFF0u, 25).change);
    assert(state.Step(66, 0, 0x10u, 25).change);
    state = {}; state.owned = 11; state.lastHit = 100;
    assert(!state.Step(0, 11, 5000, 100, true).change);
    assert(!state.Step(22, 11, 5100, 100, true).change);
    decision = state.Step(22, 11, 5200, 100, true);
    assert(decision.change && decision.target == 22);
    state.owned = 22;
    assert(!state.Step(0, 22, 6000, 100, true).change);
    assert(state.Step(0, 22, 6001, 100, false).change);
    assert(!state.Step(0, 33, 7000, 100, true).change && state.owned == 0);
    assert(action::HashBytes(reinterpret_cast<const unsigned char*>("hello"), 5) == 0x4F9F2CABu);

    assert(action::ValidCamera(1.25, 0.65, 6));
    assert(!action::ValidCamera(4, 0, 6));
    assert(!action::ValidCamera(1, 4, 6));
    assert(!action::ValidCamera(1, 0, 1));
    assert(!action::ValidCamera(1, 0, std::numeric_limits<double>::quiet_NaN()));
    assert(action::ValidCursorMount(0.5, 0.5, 3, 1));
    assert(!action::ValidCursorMount(-0.1, 0.5, 3, 1));
    assert(!action::ValidCursorMount(0.5, 1.1, 3, 1));
    assert(!action::ValidCursorMount(0.5, 0.5, 16, 1));
    assert(!action::ValidCursorMount(0.5, 0.5, 3, -1));
    assert(!action::ValidCursorMount(std::numeric_limits<double>::quiet_NaN(), 0.5, 3, 1));
    assert(!action::ValidCursorMount(0.5, 0.5, 3, std::numeric_limits<double>::infinity()));
    assert(action::ValidCursorKey(VK_MENU) && action::ValidCursorKey(VK_F8) && action::ValidCursorKey(VK_XBUTTON1));
    assert(!action::ValidCursorKey(0) && !action::ValidCursorKey(255) && !action::ValidCursorKey(-1));
    assert(!action::ValidCursorKey(18.5));
    assert(!action::ValidCursorKey(std::numeric_limits<double>::quiet_NaN()));
    assert(!action::ValidCursorKey(std::numeric_limits<double>::infinity()));
    assert(action::ValidMountedCamera(-5, 8, 40));
    assert(!action::ValidMountedCamera(5.1, 1, 10));
    assert(!action::ValidMountedCamera(1, -2, 10));
    assert(!action::ValidMountedCamera(1, 1, 41));
    assert(!action::ValidMountedCamera(1, std::numeric_limits<double>::quiet_NaN(), 10));
    const action::CameraProfile foot{1.25f, 0.65f, 6, true}, mount{-2, 2, 12, false};
    auto profile = action::SelectCameraProfile(false, foot, mount, true, true, 3, 1);
    assert(profile.stock && profile.distance == 6 && profile.side == 1.25f);
    profile = action::SelectCameraProfile(true, foot, mount, true, true, 3, 1);
    assert(!profile.stock && profile.distance == 12 && profile.side == -2 && profile.height == 2);
    profile = action::SelectCameraProfile(true, foot, mount, false, true, 3, 1);
    assert(!profile.stock && profile.distance == 9 && std::fabs(profile.height - 1.65f) < 0.0001f);
    profile = action::SelectCameraProfile(true, foot, mount, false, false, 3, 1);
    assert(!profile.stock && profile.distance == 6 && profile.side == foot.side);
    profile = action::SelectCameraProfile(true, foot, {-2, 2, 12, true}, false, true, 3, 1);
    assert(profile.stock && profile.distance == 12);
    action::ProfileZoom zoom;
    auto request = zoom.Step(18, 6, 0, 0, true);
    assert(request.change && request.distance == 6);
    assert(!zoom.Step(12, 6, 0, 200).change);
    assert(!zoom.Step(6, 6, 0, 500).change);
    request = zoom.Step(6, 6, 3, 600);
    assert(request.change && request.distance == 9);
    // The zoom call leaves the field unchanged; completion happens on later frames.
    assert(!zoom.Step(6, 6, 3, 601).change);
    assert(!zoom.Step(9, 6, 3, 1100).change);
    request = zoom.Step(9, 6, 0, 1200);
    assert(request.change && request.distance == 6);
    assert(!zoom.Step(6, 6, 0, 1700).change);
    // Wheel zoom on foot is retained through repeated mount/dismount cycles.
    request = zoom.Step(8, 6, 6, 1800);
    assert(request.change && request.distance == 14);
    assert(!zoom.Step(14, 6, 6, 2300).change);
    request = zoom.Step(14, 6, 0, 2400);
    assert(request.change && request.distance == 8);
    assert(!zoom.Step(8, 6, 0, 2900).change);
    // Reverse during animation using the original baseline, not an incomplete delta.
    assert(zoom.Step(8, 6, 3, 3000).distance == 11);
    request = zoom.Step(9, 6, 0, 3100);
    assert(request.change && request.distance == 8);
    assert(!zoom.Step(8, 6, 0, 3600).change);
    // Native max-distance clamping must not cause an overshoot when returning.
    assert(zoom.Step(8, 6, 15, 3700).distance == 23);
    assert(!zoom.Step(10, 6, 15, 4200).change);
    request = zoom.Step(10, 6, 0, 4300);
    assert(request.change && request.distance == 8);
    assert(!zoom.Step(8, 6, 0, 4800).change);
    assert(zoom.Step(8, 6, -3, 4900).distance == 5);
    assert(!zoom.Step(5, 6, -3, 5400).change);
    assert(zoom.Step(5, 6, 0, 5500).distance == 8);
    assert(!zoom.Step(std::numeric_limits<float>::quiet_NaN(), 6, 0, 5600).change);
    assert(action::ValidCursorBinding(0, 0) && action::ValidCursorBinding(VK_XBUTTON1, 7));
    assert(!action::ValidCursorBinding(0, 1) && !action::ValidCursorBinding(VK_F8, 8));
    assert(!action::ValidCursorBinding(VK_F8, 1.5));
    assert(!action::ValidCursorBinding(VK_F8, std::numeric_limits<double>::quiet_NaN()));
    assert(action::CursorBindingDown(VK_XBUTTON1, 0, 0, true));
    assert(action::ValidCursorBinding(VK_RBUTTON, 0));
    assert(action::CursorBindingDown(VK_RBUTTON, 0, 0, true));
    assert(action::CursorBindingDown(VK_RBUTTON, 0, 1, true));
    assert(action::CursorBindingDown(VK_RBUTTON, 0, 2, true));
    assert(action::CursorBindingDown(VK_RBUTTON, 0, 4, true));
    assert(action::CursorBindingDown(VK_RBUTTON, 2, 2, true));
    assert(!action::CursorBindingDown(VK_RBUTTON, 2, 3, true));
    for (unsigned modifiers : {0u, 1u, 2u, 4u, 7u}) {
        assert(action::CursorGestureDown(VK_RBUTTON, 0, modifiers, true, true));
        assert(!action::CursorGestureDown(VK_RBUTTON, 0, modifiers, false, true));
        assert(!action::CursorGestureDown(VK_RBUTTON, 0, modifiers, true, false));
    }
    action::CursorMode rmb;
    rmb.Step(false, true, true, true);
    assert(rmb.Step(true, true, true, true));
    for (unsigned modifiers : {1u, 0u, 2u, 0u, 4u, 0u})
        assert(rmb.Step(action::CursorGestureDown(VK_RBUTTON, 0, modifiers, true, true), true, true, true));
    assert(rmb.Step(false, true, true, true));
    assert(rmb.Step(action::CursorGestureDown(VK_RBUTTON, 0, 0, true, true), false, true, true));
    assert(rmb.Step(action::CursorGestureDown(VK_RBUTTON, 0, 2, true, true), false, true, true));
    assert(!rmb.Step(false, false, true, true));
    auto pickPoint = action::CursorToViewport({320, 180}, 1280, 720);
    assert(pickPoint.valid && pickPoint.x == 0.25f && pickPoint.y == 0.75f);
    assert(action::CursorToViewport({0, 0}, 1920, 1080).y == 1);
    assert(!action::CursorToViewport({1280, 100}, 1280, 720).valid);
    assert(!action::CursorToViewport({20, -1}, 1280, 720).valid);
    assert(!action::CursorToViewport({20, 720}, 1280, 720).valid);
    assert(!action::CursorToViewport({0, 0}, 0, 720).valid);
    assert(!action::CursorBindingDown(VK_XBUTTON1, 0, 1, true));
    assert(action::CursorBindingDown(VK_XBUTTON2, 3, 3, true));
    assert(!action::CursorBindingDown(VK_XBUTTON2, 3, 2, true));
    assert(!action::CursorBindingDown(VK_XBUTTON2, 3, 3, false));
    assert(!action::CursorBindingDown(0, 0, 0, true));
    assert(action::CursorBindingDown(VK_MENU, 0, 7, true));
    action::CursorMode cursorMode;
    assert(!cursorMode.Step(false, true, true, true));
    assert(cursorMode.Step(true, true, true, true));
    assert(cursorMode.Step(true, true, true, true)); // Key repeat cannot flip twice.
    assert(cursorMode.Step(false, true, true, true)); // Release keeps the cursor free.
    assert(cursorMode.Step(false, true, true, false)); // Menu/focus pause preserves choice.
    assert(cursorMode.Step(true, true, true, false)); // Input in menus cannot flip it.
    assert(cursorMode.Step(true, true, true, true)); // Held while returning is ignored.
    assert(cursorMode.Step(false, true, true, true));
    assert(!cursorMode.Step(true, true, true, true)); // Next intentional press recaptures.
    assert(!cursorMode.Step(false, true, false, false)); // Mode off resets the latch.
    assert(!cursorMode.latched);
    assert(cursorMode.Step(true, false, true, true));
    assert(!cursorMode.Step(false, false, true, true)); // Legacy hold behavior.
    auto cursor = action::CursorPosition(true, 0.5f, 0.5f, 1280, 720, {30, 40});
    assert(cursor.x == 640 && cursor.y == 360);
    cursor = action::CursorPosition(true, 0.25f, 0.75f, 1920, 1080, {30, 40});
    assert(cursor.x == 480 && cursor.y == 270); // Bottom-left UI coordinates become top-left client pixels.
    cursor = action::CursorPosition(false, 0.5f, 0.5f, 1280, 720, {30, 40});
    assert(cursor.x == 30 && cursor.y == 40);
    cursor = action::CursorPosition(false, 0, 0, 640, 480, {900, -30});
    assert(cursor.x == 639 && cursor.y == 0); // Restore remains in the resized client area.
    cursor = action::CursorPosition(true, 1, 0, 640, 480, {});
    assert(cursor.x == 639 && cursor.y == 479);
    float lift = 0;
    for (int i = 0; i < 60; ++i) lift = action::MountLift(lift, 1, 1.0f / 60);
    assert(lift > 0.999f && lift < 1);
    float slowLift = 0;
    for (int i = 0; i < 30; ++i) slowLift = action::MountLift(slowLift, 1, 1.0f / 30);
    assert(std::fabs(lift - slowLift) < 0.00001f);
    for (int i = 0; i < 60; ++i) lift = action::MountLift(lift, 0, 1.0f / 60);
    assert(lift >= 0 && lift < 0.001f);
    const action::Vec3 forward{1, 0, 0}, up{0, 0, 1};
    const auto right = action::Cross(forward, up);
    const auto delta = action::ShoulderDelta(forward, up, 1.25f, 0.65f, 6);
    assert(std::fabs(delta.y + 1.25f) < 0.0001f && std::fabs(delta.z - 0.65f) < 0.0001f);
    // Screen-right dot(player - shiftedCamera) is negative: player moves LEFT of the reticle.
    assert((-delta.y) * right.y < 0);
    const auto yawed = action::ShoulderDelta({0, 1, 0}, up, 1.25f, 0.65f, 6);
    assert(std::fabs(yawed.x - 1.25f) < 0.0001f);
    assert(action::Length(action::ShoulderDelta(forward, up, 1.25f, 0.65f, 1)) == 0);
    assert(action::Length(action::ShoulderDelta({}, up, 1, 1, 6)) == 0);
    const auto faded = action::ShoulderDelta(forward, up, 1.25f, 0.65f, 1.5f);
    assert(std::fabs(faded.y + 0.625f) < 0.0001f);
    int traces = 0;
    auto clear = action::ClipShoulder({}, delta, right, up,
        [&traces](action::Vec3, action::Vec3, float&) { ++traces; return false; });
    assert(traces == 5 && std::fabs(clear.y - delta.y) < 0.0001f);
    auto clipped = action::ClipShoulder({}, delta, right, up,
        [](action::Vec3 start, action::Vec3 end, float& fraction) {
            if (end.y >= -0.8f) return false;
            fraction = (-0.8f - start.y) / (end.y - start.y);
            return true;
        });
    assert(clipped.y >= -0.7f && clipped.y < 0 && clipped.z >= 0);
    auto invalid = action::ClipShoulder({}, delta, right, up,
        [](action::Vec3, action::Vec3, float& fraction) {
            fraction = std::numeric_limits<float>::quiet_NaN(); return true;
        });
    assert(action::Length(invalid) == 0);

    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(code);
    code[0] = 0xE8;
    const std::uint32_t displacement = 32 - 5;
    std::memcpy(code + 1, &displacement, 4);
    code[5] = 0xC3;
    const unsigned char original[] = {0xB8, 11, 0, 0, 0, 0xC3};
    const unsigned char replacement[] = {0xB8, 22, 0, 0, 0, 0xC3};
    std::memcpy(code + 32, original, sizeof(original));
    std::memcpy(code + 64, replacement, sizeof(replacement));
    DWORD old;
    assert(VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &old));
    assert(FlushInstructionCache(GetCurrentProcess(), code, 4096));
    const auto run = reinterpret_cast<int (__cdecl*)()>(code);
    assert(run() == 11);
    assert(!ReplaceCall(code, code + 16, code + 64));
    assert(run() == 11);
    assert(ReplaceCall(code, code + 32, code + 64));
    assert(run() == 22);
    assert(ReplaceCall(code, code + 64, code + 32));
    assert(run() == 11);
    assert(!ReplaceCall(code + 5, code + 32, code + 64));
    bool installed = false;
    assert(ToggleCall(installed, false, nullptr, nullptr, nullptr));
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        assert(ToggleCall(installed, true, code, code + 32, code + 64));
        assert(installed && run() == 22);
        assert(ToggleCall(installed, true, code, code + 32, code + 64));
        assert(ToggleCall(installed, false, code, code + 32, code + 64));
        assert(!installed && run() == 11);
    }
    assert(!ToggleCall(installed, true, code, code + 16, code + 64));
    assert(!installed && run() == 11);
    assert(VirtualFree(code, 0, MEM_RELEASE));
    std::puts("PASS: selection, shoulder/cursor geometry, mount smoothing, invalid settings, executed x86 CALL hook and rollback");
}
