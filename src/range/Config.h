#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include "nlohmann/json.hpp"

namespace urde {
nlohmann::json readJson(std::filesystem::path const& file);
void writeJson(std::filesystem::path const& file, nlohmann::json const& json);
// Commands only publish requested, game-pointer-free data. The side's owner
// thread explicitly accepts it into effective; each native cycle pins that copy.
template <class T>
class ConfigStore {
    std::filesystem::path file;
    mutable std::mutex mutex;
    std::atomic<std::shared_ptr<T const>> requested;
    std::atomic<std::shared_ptr<T const>> effective;
    std::chrono::steady_clock::time_point lastChange{};
    std::function<void()> legacyNotice;
    mutable bool legacyReported = false;

    std::shared_ptr<T const> read() const {
        auto json = readJson(file);
        auto result = std::make_shared<T const>(T::decode(json));
        if (json.contains("full_rate_players") && !legacyReported && legacyNotice) {
            legacyNotice();
            legacyReported = true;
        }
        return result;
    }
    void persist(T const& value) { writeJson(file, value.encode()); }
public:
    explicit ConfigStore(T initial = {}) : requested(std::make_shared<T const>(std::move(initial))) {}
    void load(std::filesystem::path path, std::function<void()> notice = {}) {
        std::lock_guard lock(mutex);
        file = std::move(path);
        legacyNotice = std::move(notice);
        if (!std::filesystem::exists(file)) persist(*requested.load());
        requested.store(read());
    }
    std::string change(std::string const& key, std::string const& value) {
        return update([&](T const& old) { auto next = old; next.set(key, value); return next; });
    }
    std::string reload() { return update([&](T const&) { return *read(); }); }
    std::shared_ptr<T const> desired() const { return requested.load(); }
    std::shared_ptr<T const> active() const { return effective.load(); }
    std::shared_ptr<T const> accept() {
        auto next = requested.load();
        effective.store(next);
        return next;
    }
    void clearEffective() { effective.store(nullptr); }
private:
    std::string update(std::function<T(T const&)> const& make) {
        std::lock_guard lock(mutex);
        auto now = std::chrono::steady_clock::now();
        if (now - lastChange < std::chrono::seconds(1)) throw std::invalid_argument("Wait one second between changes");
        T next = make(*requested.load());
        next.validate(); // the entire candidate, not just the edited field
        auto snapshot = std::make_shared<T const>(std::move(next));
        persist(*snapshot); // failure retains both published snapshots
        requested.store(std::move(snapshot));
        lastChange = now;
        return "Saved; requested snapshot queued for the next owner-thread tick. ";
    }
};
float readRange(nlohmann::json const& json, char const* key, float max, bool integral = false);
// Normalize legacy aliases before schema validation; reject ambiguous pairs.
nlohmann::json migrateKeys(nlohmann::json json, std::initializer_list<std::pair<char const*,char const*>> aliases);
nlohmann::json encodeRange(float value);
}
