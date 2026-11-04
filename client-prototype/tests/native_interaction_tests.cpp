#include "../src/native.cpp"
#include <cassert>

// Reserve the client-global range in zero-filled image memory before CRT/heap allocation.
unsigned char testGlobals[0xA00000];

int main() {
    assert(reinterpret_cast<std::uintptr_t>(testGlobals) <= 0xC24954);
    assert(reinterpret_cast<std::uintptr_t>(testGlobals + sizeof(testGlobals)) > 0xD4139C);
    for (int scenario = 0; scenario < 3; ++scenario) {
        *reinterpret_cast<unsigned*>(0xD4139C) = scenario == 2 ? 1 : 0;
        g_probe = {};
        g_interactTime = scenario == 1 ? 1 : 1000;
        g_interactPending = true;
        g_captured = true;
        g_active = true;
        g_pick = 42;
        g_selection.owned = 42;
        InteractUnderCrosshair(nullptr, 1000);
        assert(!g_interactPending && !g_pick && !g_selection.owned);
        assert(g_captured && g_active);
        assert(g_interactPause == 1000);
    }
    std::puts("PASS: interaction consumes one request without releasing mouse capture");
}
