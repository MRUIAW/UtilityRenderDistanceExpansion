#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>

// BEGIN temporary movement diagnostics (0.3.0); no game pointers or pair state.
namespace urde::movement::diagnostics {
// Index order: nullopt, 0, 1, 2, 3, 4, 5-6, 7, 8-9, 10, >10.
constexpr size_t valueBucket(std::optional<uint64_t> native) {
    if (!native) return 0;
    auto value = *native;
    if (value <= 4) return size_t(value)+1;
    if (value <= 6) return 6;
    if (value == 7) return 7;
    if (value <= 9) return 8;
    if (value == 10) return 9;
    return 10;
}
inline std::optional<size_t> distanceBucket(double x, double y, double z) {
    auto squared = x*x+y*y+z*z;
    if (!std::isfinite(squared)) return std::nullopt;
    return squared < 64.0*64.0 ? 0 : squared < 128.0*128.0 ? 1 : 2;
}
struct Counters {
    // sender player/other, distance <64/[64,128)/>=128, native value bucket
    std::array<std::array<std::array<std::atomic<uint64_t>,11>,3>,2> intervals{};
    decltype(intervals) finalIntervals{};
    std::array<std::atomic<uint64_t>,2> sendCalls{};
    std::atomic<uint64_t> ticks{}, skippedInterval{}, skippedSend{};
    std::atomic<uint64_t> leverOpened{}, leverCapped{}, leverFullRate{}, pairOverflow{};
    void reset() {
        for (auto& sender : intervals) for (auto& distance : sender)
            for (auto& value : distance) value.store(0,std::memory_order_relaxed);
        for (auto& sender : finalIntervals) for (auto& distance : sender)
            for (auto& value : distance) value.store(0,std::memory_order_relaxed);
        leverOpened.store(0,std::memory_order_relaxed);
        leverCapped.store(0,std::memory_order_relaxed);
        leverFullRate.store(0,std::memory_order_relaxed);
        pairOverflow.store(0,std::memory_order_relaxed);
        for (auto& value : sendCalls) value.store(0,std::memory_order_relaxed);
        ticks.store(0,std::memory_order_relaxed);
        skippedInterval.store(0,std::memory_order_relaxed);
        skippedSend.store(0,std::memory_order_relaxed);
    }
};
static_assert(std::atomic<uint64_t>::is_always_lock_free);
}
// END temporary movement diagnostics
