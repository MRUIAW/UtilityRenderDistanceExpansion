#pragma once
#include <cmath>
#include <stdexcept>
#include <string>

namespace urde {
inline constexpr int MaxChunks = 32;
inline constexpr float MaxBlocks = 512;
inline int localReplicationChunks(float blocks) {
    int radius = int(std::ceil(double(blocks) / 16.0));
    // Native 57 offsets satisfy x*x+z*z <=17; radius4 misses (4,1).
    // Only replace with a superset. No extra margin for collector cube corners.
    return radius >= 5 ? radius : 0; // zero leaves native offsets untouched
}
constexpr int diskCount(int radius) {
    int count = 0;
    for (int x = -radius; x <= radius; ++x)
        for (int z = -radius; z <= radius; ++z)
            if (x*x + z*z <= radius*radius) ++count;
    return count;
}
static_assert(diskCount(8) == 197);
static_assert(diskCount(4) == 49); // vanilla is 57, not radius 4
static_assert(MaxChunks*MaxChunks*2 < 2147483647);
static_assert(int(MaxBlocks)*int(MaxBlocks) < 2147483647);
inline float parseRange(std::string const& text, float max, bool integral = false) {
    if (text == "vanilla") return 0; // internal sentinel only; JSON uses a string
    std::size_t end = 0;
    float value = std::stof(text, &end);
    if (end != text.size() || !std::isfinite(value) || value <= 0 || value > max
        || (integral && std::floor(value) != value))
        throw std::invalid_argument("Expected vanilla or a finite positive value <= " + std::to_string(max)
                                    + (integral ? " (whole chunks)" : " blocks"));
    return value;
}
inline void validateRange(float value, float max, bool integral = false) {
    if (!std::isfinite(value) || value < 0 || value > max || (integral && std::floor(value) != value))
        throw std::invalid_argument("Invalid range in candidate config");
}
inline std::string rangeText(float value) {
    if (value == 0) return "vanilla";
    return std::to_string(value);
}
}
