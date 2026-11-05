#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include "nlohmann/json.hpp"

namespace urde::movement {
enum class Outcome { player, type, tag, capped, unchanged, unavailable, count };
struct Decision {
    std::optional<uint64_t> interval;
    Outcome outcome;
};
struct Settings {
    int updateInterval = 4; // zero represents vanilla only inside the policy
    bool fullRatePlayers = true;
    std::vector<std::string> fullRateTypes, fullRateTags;
    std::unordered_set<std::string> typeIds;

    explicit Settings(int interval = 4) : updateInterval(interval) {}
    static bool isKey(std::string const& key) {
        return key == "update_interval" || key == "full_rate_players"
            || key == "full_rate_types" || key == "full_rate_tags";
    }
    void validate() const {
        if (updateInterval < 0 || updateInterval > 10)
            throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
        for (auto const* list : {&fullRateTypes,&fullRateTags})
            for (auto const& text : *list)
                if (text.empty() || text.find('\0') != std::string::npos)
                    throw std::invalid_argument("full_rate_types/full_rate_tags: expected nonempty strings without NUL");
    }
    void rebuild() {
        validate();
        typeIds = {fullRateTypes.begin(),fullRateTypes.end()};
    }
    void set(std::string const& key, std::string const& text) {
        if (key == "update_interval") {
            if (text == "vanilla") updateInterval = 0;
            else {
                int value = 0;
                auto [end,error] = std::from_chars(text.data(),text.data()+text.size(),value);
                if (error != std::errc{} || end != text.data()+text.size() || value < 1 || value > 10)
                    throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
                updateInterval = value;
            }
        } else if (key == "full_rate_players") {
            if (text != "true" && text != "false")
                throw std::invalid_argument("full_rate_players: true or false");
            fullRatePlayers = text == "true";
        } else throw std::invalid_argument("Movement lists are edited in the config file; use reload");
        rebuild();
    }
    nlohmann::json encode() const {
        return {{"update_interval",updateInterval ? nlohmann::json(updateInterval) : nlohmann::json("vanilla")},
                {"full_rate_players",fullRatePlayers}, {"full_rate_types",fullRateTypes}, {"full_rate_tags",fullRateTags}};
    }
    static Settings decode(nlohmann::json const& json, int defaultInterval) {
        Settings result(defaultInterval);
        if (json.contains("update_interval")) {
            auto const& field = json.at("update_interval");
            if (field == "vanilla") result.updateInterval = 0;
            else if (field.is_number_integer() && field >= 1 && field <= 10)
                result.updateInterval = field.get<int>();
            else throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
        }
        if (json.contains("full_rate_players")) {
            if (!json.at("full_rate_players").is_boolean())
                throw std::invalid_argument("full_rate_players: expected boolean");
            result.fullRatePlayers = json.at("full_rate_players").get<bool>();
        }
        for (auto [key,list] : {std::pair{"full_rate_types",&result.fullRateTypes},
                               std::pair{"full_rate_tags",&result.fullRateTags}}) {
            if (!json.contains(key)) continue;
            auto const& field = json.at(key);
            if (!field.is_array()) throw std::invalid_argument(std::string(key)+": expected string list");
            for (auto const& item : field) {
                if (!item.is_string()) throw std::invalid_argument(std::string(key)+": expected string list");
                list->push_back(item.get<std::string>());
            }
        }
        result.rebuild();
        return result;
    }
    int minimumInterval() const {
        return updateInterval && (fullRatePlayers || !typeIds.empty() || !fullRateTags.empty()) ? 1 : updateInterval;
    }
    std::string describe() const {
        return "update_interval=" + (updateInterval ? std::to_string(updateInterval) : std::string("vanilla"))
             + " full_rate_players=" + (fullRatePlayers ? "true" : "false")
             + " full_rate_types=" + std::to_string(fullRateTypes.size())
             + " full_rate_tags=" + std::to_string(fullRateTags.size());
    }
};

// Callbacks are lazy: classification happens only when it can lower the native
// interval. Empty optionals stay empty; native 0/1 and vanilla need no actor query.
template <class IsPlayer, class TypeName, class HasTag>
Decision decide(std::optional<uint64_t> native, Settings const& settings, bool available,
                IsPlayer&& isPlayer, TypeName&& typeName, HasTag&& hasTag) {
    if (!native) return {native,Outcome::unavailable};
    if (!settings.updateInterval || *native <= uint64_t(settings.minimumInterval()))
        return {native,Outcome::unchanged};
    if (!available) return {native,Outcome::unavailable};
    if (settings.fullRatePlayers && isPlayer()) return {1,Outcome::player};
    if (!settings.typeIds.empty() && settings.typeIds.contains(typeName())) return {1,Outcome::type};
    for (auto const& tag : settings.fullRateTags)
        if (hasTag(tag)) return {1,Outcome::tag};
    auto interval = std::min(*native,uint64_t(settings.updateInterval));
    return {interval,interval < *native ? Outcome::capped : Outcome::unchanged};
}
}
