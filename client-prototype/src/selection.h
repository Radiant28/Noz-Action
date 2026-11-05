#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace action {

inline bool ValidSettings(double range, double delay) {
    return std::isfinite(range) && std::isfinite(delay) &&
           range >= 5 && range <= 300 && delay >= 0 && delay <= 500;
}

struct Decision {
    bool change = false;
    std::uint64_t target = 0;
};

struct ClickSelection {
    std::uint64_t pending = 0;
    std::uint32_t clickedAt = 0;

    void Queue(std::uint64_t hover, std::uint32_t now) { pending = hover; clickedAt = now; }
    std::uint64_t Take(std::uint32_t now) {
        const auto result = now - clickedAt <= 250 ? pending : 0;
        pending = 0;
        return result;
    }
};

struct Selection {
    std::uint64_t pending = 0;
    std::uint64_t owned = 0;
    std::uint32_t pendingSince = 0;
    std::uint32_t lastHit = 0;

    Decision Step(std::uint64_t pick, std::uint64_t actual,
                  std::uint32_t now, std::uint32_t delay, bool keepTarget = false) {
        if (actual != owned) owned = 0;
        if (!pick) {
            pending = 0;
            return {!keepTarget && owned != 0 && now - lastHit >= 250, 0};
        }
        lastHit = now;
        if (pending != pick) {
            pending = pick;
            pendingSince = now;
        }
        return {pick != actual && now - pendingSince >= delay, pick};
    }
};

inline std::uint32_t HashBytes(const unsigned char* bytes, std::size_t count) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

} // namespace action
