#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include "nlohmann/json.hpp"

namespace urde::movement {
inline constexpr char PlayerType[] = "minecraft:player";
enum class Outcome { unchanged, opened, capped };
struct Decision {
    std::optional<uint64_t> interval;
    Outcome outcome = Outcome::unchanged;
};
// Pure post-origin lever. last is our last FINAL engaged result, never the
// opaque native last-send record. Native faster values always survive.
constexpr Decision decide(std::optional<uint64_t> native, uint64_t now,
                          std::optional<uint64_t> last, unsigned interval) {
    if (!interval) return {native};
    if (native) return {*native > interval ? interval : *native,
                        *native > interval ? Outcome::capped : Outcome::unchanged};
    if (!last || (now >= *last && now-*last >= interval)) return {interval,Outcome::opened};
    return {native};
}

struct Settings {
    int updateInterval = 4; // zero is the internal vanilla sentinel
    std::vector<std::string> fullRateTypes{PlayerType}, fullRateTags;
    std::unordered_set<std::string> typeIds{PlayerType};
    explicit Settings(int interval = 4) : updateInterval(interval) {}
    static bool isKey(std::string const& key) {
        return key == "update_interval" || key == "full_rate_types" || key == "full_rate_tags";
    }
    static bool isConfigKey(std::string const& key) { return isKey(key) || key == "full_rate_players"; }
    void validate() const {
        if (updateInterval < 0 || updateInterval > 10)
            throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
        for (auto const* list : {&fullRateTypes,&fullRateTags})
            for (auto const& text : *list)
                if (text.empty() || text.find('\0') != std::string::npos)
                    throw std::invalid_argument("full_rate_types/full_rate_tags: expected nonempty strings without NUL");
    }
    void rebuild() { validate(); typeIds = {fullRateTypes.begin(),fullRateTypes.end()}; }
    void set(std::string const& key, std::string const& text) {
        if (key != "update_interval") throw std::invalid_argument("Movement lists are edited in the config file; use reload");
        if (text == "vanilla") updateInterval = 0;
        else {
            // Client uses one float parser for all numeric keys; accept 4.0,
            // reject fractions/nonfinite/zero without silently rounding.
            size_t end = 0;
            double value;
            try { value = std::stod(text,&end); }
            catch (std::exception const&) {
                throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
            }
            if (end != text.size() || !std::isfinite(value) || value < 1 || value > 10 || value != std::floor(value))
                throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
            updateInterval = int(value);
        }
        rebuild();
    }
    nlohmann::json encode() const {
        return {{"update_interval",updateInterval ? nlohmann::json(updateInterval) : nlohmann::json("vanilla")},
                {"full_rate_types",fullRateTypes}, {"full_rate_tags",fullRateTags}};
    }
    static Settings decode(nlohmann::json const& json, int defaultInterval) {
        Settings result(defaultInterval);
        if (json.contains("update_interval")) {
            auto const& field = json.at("update_interval");
            if (field == "vanilla") result.updateInterval = 0;
            else if (field.is_number_integer() && field >= 1 && field <= 10) result.updateInterval = field.get<int>();
            else throw std::invalid_argument("update_interval: whole ticks 1..10 or vanilla");
        }
        // Legacy full_rate_players is deliberately ignored, regardless of value.
        for (auto [key,list] : {std::pair{"full_rate_types",&result.fullRateTypes},
                               std::pair{"full_rate_tags",&result.fullRateTags}}) {
            if (!json.contains(key)) continue;
            auto const& field = json.at(key);
            if (!field.is_array()) throw std::invalid_argument(std::string(key)+": expected string list");
            list->clear(); // explicit empty/nonempty lists replace the default
            for (auto const& item : field) {
                if (!item.is_string()) throw std::invalid_argument(std::string(key)+": expected string list");
                list->push_back(item.get<std::string>());
            }
        }
        result.rebuild();
        return result;
    }
    struct Selection { unsigned interval; bool fullRate; };
    template <class TypeName, class HasTag>
    Selection select(TypeName&& typeName, HasTag&& hasTag) const {
        if (!updateInterval) return {0,false};
        if (!typeIds.empty() && typeIds.contains(typeName())) return {1,true};
        for (auto const& tag : fullRateTags) if (hasTag(tag)) return {1,true};
        return {unsigned(updateInterval),false};
    }
    std::string describe() const {
        return "update_interval=" + (updateInterval ? std::to_string(updateInterval) : std::string("vanilla"))
             + " full_rate_types=" + std::to_string(fullRateTypes.size())
             + " full_rate_tags=" + std::to_string(fullRateTags.size());
    }
};
}
