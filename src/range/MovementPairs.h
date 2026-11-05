#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace urde::movement {
// Fixed four-way buckets: a single hash lookup, bounded comparisons, no node
// allocations or rehashes. A saturated bucket falls back to native, never evicts
// a live pair and accidentally reopens its gate. IDs alone survive hook scopes.
template <size_t Buckets = 16384>
class PairTable {
    static_assert(Buckets && (Buckets & (Buckets-1)) == 0);
public:
    static constexpr uint64_t RetentionTicks = 200; // ~10 seconds at 20 TPS
    struct Record {
        int64_t sender{}, recipient{};
        uint64_t seen{}, last{};
        bool used = false, engaged = false;
        std::optional<uint64_t> lastEngaged() const { return engaged ? std::optional(last) : std::nullopt; }
        void sent(uint64_t tick) { last = tick; engaged = true; }
    };
private:
    std::array<std::array<Record,4>,Buckets> buckets{};
    static constexpr uint64_t mix(uint64_t value) {
        value = (value ^ (value >> 30))*0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27))*0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    static bool stale(Record const& record, uint64_t now) {
        return now < record.seen || now-record.seen >= RetentionTicks;
    }
public:
    Record* touch(int64_t sender, int64_t recipient, uint64_t now) {
        auto& bucket = buckets[(mix(uint64_t(sender)) ^ mix(uint64_t(recipient)+0x9e3779b97f4a7c15ULL)) & (Buckets-1)];
        Record* free = nullptr;
        for (auto& record : bucket) {
            if (record.used && record.sender == sender && record.recipient == recipient) {
                if (stale(record,now)) record.engaged = false;
                record.seen = now;
                return &record;
            }
            if (!free && (!record.used || stale(record,now))) free = &record;
        }
        if (free) *free = Record{sender,recipient,now,0,true,false};
        return free;
    }
    void prune(uint64_t now) {
        for (auto& bucket : buckets) for (auto& record : bucket)
            if (record.used && stale(record,now)) record = {};
    }
    void clear() { for (auto& bucket : buckets) for (auto& record : bucket) record = {}; }
    size_t size() const {
        size_t result = 0;
        for (auto const& bucket : buckets) for (auto const& record : bucket) result += record.used;
        return result;
    }
};
}
