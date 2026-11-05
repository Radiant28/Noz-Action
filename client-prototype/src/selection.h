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

inline std::uint64_t HoverGuid(int kind, std::uint64_t raw, std::uint64_t unit, unsigned objectMask) {
    if (kind < 2) return 0;
    if (objectMask & 8) return unit;
    return (objectMask & 0x20) ? raw : 0;
}

template<class Lookup, class Change>
void UpdateHoverHighlight(std::uint64_t previous, std::uint64_t next, Lookup lookup, Change change) {
    if (previous && previous != next)
        if (auto* object = lookup(previous)) change(object, false);
    if (next)
        if (auto* object = lookup(next)) change(object, true);
}

struct ClickSelection {
    std::uint64_t pending = 0;
    std::uint32_t clickedAt = 0;
    bool queued = false;

    void Queue(std::uint64_t hover, std::uint32_t now) { pending = hover; clickedAt = now; queued = true; }
    Decision Take(std::uint32_t now) {
        const Decision result = {queued && now - clickedAt <= 250, pending};
        *this = {};
        return result;
    }
};

inline bool ParseGuid(const char* text, std::size_t length, std::uint64_t& guid) {
    guid = 0;
    if (length == 0) return true;
    if (length != 18 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) return false;
    for (std::size_t i = 2; i < length; ++i) {
        const char c = text[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return false;
        guid = (guid << 4) | static_cast<unsigned>(digit);
    }
    return true;
}

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
