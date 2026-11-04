#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace action {
struct Vec3 { float x, y, z; };
struct CursorPixel { long x, y; };
inline bool ValidCursorKey(double key) {
    return std::isfinite(key) && key >= 1 && key <= 254 && std::floor(key) == key;
}
inline bool ValidCursorBinding(double key, double modifiers) {
    return (key == 0 || ValidCursorKey(key)) && std::isfinite(modifiers) &&
        modifiers >= 0 && modifiers <= 7 && std::floor(modifiers) == modifiers && (key != 0 || modifiers == 0);
}
inline bool CursorBindingDown(int key, unsigned required, unsigned held, bool down) {
    if (!key || !down) return false;
    // Standalone modifiers retain the legacy Alt/Shift/Ctrl cursor behavior.
    if (!required && (key == 16 || key == 17 || key == 18 || (key >= 160 && key <= 165))) return true;
    return required == held;
}
struct CursorMode {
    bool latched = false, wasDown = false, wasAllowed = false;
    bool Step(bool down, bool toggle, bool enabled, bool allowed) {
        if (!enabled || !toggle) latched = false;
        else if (allowed && wasAllowed && down && !wasDown) latched = !latched;
        wasDown = down;
        wasAllowed = enabled && allowed;
        return enabled && (toggle ? latched : down);
    }
};
inline CursorPixel CursorPosition(bool center, float x, float y, long width, long height, CursorPixel previous) {
    const CursorPixel wanted = center ? CursorPixel{static_cast<long>(x * width), static_cast<long>((1 - y) * height)} : previous;
    return {std::clamp(wanted.x, 0L, width - 1), std::clamp(wanted.y, 0L, height - 1)};
}
inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Scale(Vec3 v, float scale) { return {v.x * scale, v.y * scale, v.z * scale}; }
inline float Length(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline bool Normalize(Vec3& v) {
    const float length = Length(v);
    if (!std::isfinite(length) || length < 0.0001f) return false;
    v = Scale(v, 1 / length);
    return true;
}
inline bool ValidCamera(double side, double height, double distance) {
    return std::isfinite(side) && std::isfinite(height) && std::isfinite(distance) &&
           side >= -3 && side <= 3 && height >= -1 && height <= 3 && distance >= 3 && distance <= 25;
}
inline bool ValidMountedCamera(double side, double height, double distance) {
    return std::isfinite(side) && std::isfinite(height) && std::isfinite(distance) &&
           side >= -5 && side <= 5 && height >= -1 && height <= 8 && distance >= 3 && distance <= 40;
}
struct CameraProfile { float side, height, distance; bool stock; };
struct ZoomRequest { bool change; float distance; };
struct ProfileZoom {
    float applied = 0, requested = 0, base = 0;
    std::uint32_t started = 0;
    bool pending = false;
    ZoomRequest Step(float current, float footDistance, float offset, std::uint32_t now, bool reset = false) {
        if (!std::isfinite(current) || current < 0 || current > 60) return {false, 0};
        // Native zoom is asynchronous: measure its actual capped delta only after the 400 ms channel settles.
        if (pending && now - started >= 500) {
            applied = current - base;
            pending = false;
        }
        if (!reset && offset == requested) return {false, 0};
        if (reset) base = footDistance;
        else if (!pending) base = current - applied;
        requested = offset;
        started = now;
        pending = true;
        return {true, std::clamp(base + offset, 0.0f, 60.0f)};
    }
};
inline CameraProfile SelectCameraProfile(bool mounted, CameraProfile foot, CameraProfile mount,
                                         bool custom, bool adapt, float extraDistance, float extraHeight) {
    if (!mounted) return foot;
    if (mount.stock || custom) return mount;
    foot.stock = false;
    if (adapt) { foot.distance += extraDistance; foot.height += extraHeight; }
    return foot;
}
inline bool ValidCursorMount(double x, double y, double distance, double height) {
    return std::isfinite(x) && std::isfinite(y) && x >= 0 && x <= 1 && y >= 0 && y <= 1 &&
        std::isfinite(distance) && std::isfinite(height) && distance >= 0 && distance <= 15 && height >= 0 && height <= 5;
}
inline float MountLift(float current, float target, float seconds) {
    return current + (target - current) * (1 - std::exp(-8 * std::clamp(seconds, 0.0f, 0.1f)));
}
inline Vec3 ShoulderDelta(Vec3 forward, Vec3 up, float side, float height, float boom) {
    Vec3 right = Cross(forward, up); // Same horizontal basis as native view builder 0x6BFE60.
    if (!Normalize(right) || !std::isfinite(boom)) return {};
    const float fade = std::clamp(boom - 1.0f, 0.0f, 1.0f);
    return Add(Scale(right, side * fade), {0, 0, height * fade});
}

template<class Trace>
Vec3 ClipShoulder(Vec3 pivot, Vec3 delta, Vec3 right, Vec3 up, Trace trace) {
    const float length = Length(delta);
    if (!std::isfinite(length) || length < 0.0001f || !Normalize(right) || !Normalize(up)) return {};
    const Vec3 offsets[] = {{}, Scale(right, 0.2f), Scale(right, -0.2f),
                           Scale(up, 0.2f), Scale(up, -0.2f)};
    float allowed = 1;
    for (Vec3 offset : offsets) {
        const Vec3 start = Add(pivot, offset), end = Add(start, delta);
        float fraction = 1;
        if (!trace(start, end, fraction)) continue;
        if (!std::isfinite(fraction) || fraction < 0 || fraction > 1) return {};
        allowed = std::min(allowed, std::clamp(fraction - 0.1f / length, 0.0f, 1.0f));
    }
    return Scale(delta, allowed);
}
} // namespace action
