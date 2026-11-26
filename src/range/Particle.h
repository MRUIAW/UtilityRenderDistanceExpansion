#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "nlohmann/json.hpp"

namespace urde::particle {
inline constexpr float DefaultRange = 512.0f;
inline constexpr uint64_t DefaultCapacity = 16384;
inline constexpr uint64_t MaxCapacity = 65536; // fits exactly in IEEE-754 float

constexpr float nativeRadius(int viewChunks, float particleViewDistance, bool isGlobal) {
    float halfView = float(viewChunks) * 8.0f;
    float clamped = halfView < 40.0f ? 40.0f : (halfView > 64.0f ? 64.0f : halfView);
    float radius = 32.0f + (clamped - 32.0f) * particleViewDistance;
    return isGlobal ? radius * 5.0f : radius;
}

constexpr float effectiveRadius(float configuredRange, int viewChunks, float particleViewDistance, bool isGlobal) {
    float native = nativeRadius(viewChunks, particleViewDistance, isGlobal);
    if (configuredRange <= 0.0f) return native;
    return isGlobal ? (configuredRange > native ? configuredRange : native) : configuredRange;
}

constexpr bool withinRadius(float distSqr, float radius) {
    return double(distSqr) <= double(radius) * double(radius);
}

constexpr uint64_t effectiveManualCapacity(uint64_t nativeAllowed, uint64_t configuredCapacity) {
    if (configuredCapacity == 0 || nativeAllowed == 0) return nativeAllowed;
    return configuredCapacity;
}

inline void validateCapacity(uint64_t value) {
    if (value > MaxCapacity)
        throw std::invalid_argument("particle_capacity: whole particles 1..65536 or vanilla");
}

inline uint64_t parseCapacity(std::string const& text) {
    if (text == "vanilla") return 0;
    std::size_t end = 0;
    double value;
    try { value = std::stod(text, &end); }
    catch (std::exception const&) {
        throw std::invalid_argument("particle_capacity: whole particles 1..65536 or vanilla");
    }
    if (end != text.size() || !std::isfinite(value) || value < 1.0 || value > double(MaxCapacity)
        || value != std::floor(value))
        throw std::invalid_argument("particle_capacity: whole particles 1..65536 or vanilla");
    return static_cast<uint64_t>(value);
}

inline uint64_t readCapacity(nlohmann::json const& json, char const* key) {
    auto const& field = json.at(key);
    if (field.is_string() && field.get<std::string>() == "vanilla") return 0;
    if (!field.is_number_integer() || field.is_boolean())
        throw std::invalid_argument(std::string(key) + ": expected whole integer 1..65536 or vanilla");
    int64_t value = field.get<int64_t>();
    if (value < 1 || value > int64_t(MaxCapacity))
        throw std::invalid_argument(std::string(key) + ": outside allowed bounds");
    return static_cast<uint64_t>(value);
}

inline nlohmann::json encodeCapacity(uint64_t value) {
    return value == 0 ? nlohmann::json("vanilla") : nlohmann::json(value);
}

inline std::string capacityText(uint64_t value) {
    return value == 0 ? std::string("vanilla") : std::to_string(value);
}
}
