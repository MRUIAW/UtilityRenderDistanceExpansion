#pragma once
#include <charconv>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace urde {
// External linkage for LL's reflected parameters and hard-enum registration.
enum class UrdeClientKey { entity_range, block_entity_range, terrain_detail, replication_margin, update_interval, entity_shape };
enum class UrdeEntityShape { circle, square };
enum class UrdeServerKey { actor_interest_chunks, update_interval };
enum class UrdeVanilla { vanilla };
enum class UrdeAction { status, set, reload };

// Keep the existing string-based validation/persistence path. to_chars uses a
// locale-independent round-trip representation, unlike to_string's six decimals
// (which would round a valid small positive float to zero).
template <class Number>
std::string commandNumberText(Number value) {
    static_assert(std::is_same_v<Number,float> || std::is_same_v<Number,int>);
    char buffer[64];
    auto [end,error] = std::to_chars(buffer,buffer+sizeof(buffer),value);
    if (error != std::errc{}) throw std::invalid_argument("Cannot format command value");
    return {buffer,end};
}
}
