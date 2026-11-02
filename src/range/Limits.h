#pragma once
#include <cmath>
#include <stdexcept>
#include <string>

namespace urde {
inline constexpr int MaxChunks = 34; // accommodates 512-block configs plus the default margin
inline constexpr int MaxServerChunks = 32;
inline constexpr float MaxBlocks = 512;
// Chunk centers use integer chunk coordinates. A player's position can approach
// any edge of its chunk, so the minimum center distance per axis is
// max(16*abs(offset)-8,0). Include equality to cover closed circle boundaries.
constexpr int requiredLocalChunks(float entityRange, float margin) {
    if (entityRange <= 0) return 0;
    double range = double(entityRange) + double(margin);
    int farthestSquared = 0;
    int maxOffset = int(range / 16.0) + 2;
    for (int x = 0; x <= maxOffset; ++x) for (int z = 0; z <= maxOffset; ++z) {
        int gapX = x*16 > 8 ? x*16-8 : 0;
        int gapZ = z*16 > 8 ? z*16-8 : 0;
        bool intersects = double(gapX*gapX + gapZ*gapZ) <= range*range;
        int squared = x*x + z*z;
        if (intersects && squared > farthestSquared) farthestSquared = squared;
    }
    int radius = 0;
    while (radius*radius < farthestSquared) ++radius;
    return radius;
}
constexpr int localReplicationChunks(float entityRange, float margin) {
    int radius = requiredLocalChunks(entityRange,margin);
    // Native57 is x*x+z*z <=17. Radius4 misses (4,1); radius5 is a superset.
    return radius >= 5 ? radius : 0;
}
constexpr bool replicationFits(float entityRange, float margin) {
    return requiredLocalChunks(entityRange,margin) <= MaxChunks;
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
static_assert(localReplicationChunks(128,16) == 10);
static_assert(diskCount(localReplicationChunks(128,16)) == 317);
static_assert(localReplicationChunks(128,0) == 9);
static_assert(localReplicationChunks(16,16) == 0);
static_assert(requiredLocalChunks(32,0) == 3);
static_assert(requiredLocalChunks(32.0001f,0) == 3);
static_assert(localReplicationChunks(512,16) == MaxChunks);
static_assert(replicationFits(512,16));
static_assert(!replicationFits(512,512));
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
inline float parseMargin(std::string const& text) {
    std::size_t end = 0;
    float value = std::stof(text,&end);
    if (end != text.size() || !std::isfinite(value) || value < 0 || value > MaxBlocks)
        throw std::invalid_argument("replication_margin: expected a finite nonnegative number <= 512");
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
