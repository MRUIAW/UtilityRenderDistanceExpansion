#pragma once
#include "range/CommandValues.h"
#include "range/Config.h"
#include "range/Limits.h"
#include "range/Movement.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace urde::client {
struct Settings {
    float entityRange = 512, blockEntityRange = 512, terrainDetail = 512;
    float replicationMargin = 16;
    UrdeEntityShape entityShape = UrdeEntityShape::circle;
    movement::Settings movement{1};

    void validate() const {
        movement.validate();
        for (float value : {entityRange,blockEntityRange,terrainDetail}) validateRange(value,MaxBlocks);
        validateRange(replicationMargin,MaxBlocks);
        if (entityShape != UrdeEntityShape::circle && entityShape != UrdeEntityShape::square)
            throw std::invalid_argument("entity_shape must be circle or square");
        if (!replicationFits(entityRange,replicationMargin))
            throw std::invalid_argument("entity_range plus replication_margin exceeds the 34-chunk local replication limit");
    }
    void set(std::string const& key, std::string const& text) {
        if (key == "entity_range") entityRange = parseRange(text,MaxBlocks);
        else if (key == "block_entity_range") blockEntityRange = parseRange(text,MaxBlocks);
        else if (key == "terrain_detail") terrainDetail = parseRange(text,MaxBlocks);
        else if (key == "replication_margin") replicationMargin = parseMargin(text);
        else if (key == "entity_shape") {
            if (text == "circle") entityShape = UrdeEntityShape::circle;
            else if (text == "square") entityShape = UrdeEntityShape::square;
            else throw std::invalid_argument("entity_shape must be circle or square");
        } else if (movement::Settings::isKey(key)) movement.set(key,text);
        else throw std::invalid_argument("Unknown client setting: " + key);
        validate();
    }
    nlohmann::json encode() const {
        auto json = movement.encode();
        json.update({{"entity_range",encodeRange(entityRange)}, {"block_entity_range",encodeRange(blockEntityRange)},
                {"terrain_detail",encodeRange(terrainDetail)},
                {"entity_shape",entityShape == UrdeEntityShape::circle ? "circle" : "square"},
                {"replication_margin",replicationMargin}});
        return json;
    }
    static Settings decode(nlohmann::json json) {
        json = migrateKeys(std::move(json),{{"entity_range","entityRange"},{"block_entity_range","blockEntityRange"},{"terrain_detail","terrainDetail"}});
        if (json.size() < 3 || json.size() > 9) throw std::invalid_argument("Expected client range keys and optional shape, margin and movement settings");
        for (auto const& [key,value] : json.items()) {
            if (key != "entity_range" && key != "block_entity_range" && key != "terrain_detail"
                && key != "entity_shape" && key != "replication_margin" && !movement::Settings::isConfigKey(key))
                throw std::invalid_argument("Unknown client config key: " + key);
        }
        Settings result;
        result.movement = movement::Settings::decode(json,1);
        result.entityRange = readRange(json,"entity_range",MaxBlocks);
        result.blockEntityRange = readRange(json,"block_entity_range",MaxBlocks);
        result.terrainDetail = readRange(json,"terrain_detail",MaxBlocks);
        if (json.contains("entity_shape")) {
            auto const& shape = json.at("entity_shape");
            if (!shape.is_string()) throw std::invalid_argument("entity_shape: expected circle or square");
            auto text = shape.get<std::string>();
            if (text == "circle") result.entityShape = UrdeEntityShape::circle;
            else if (text == "square") result.entityShape = UrdeEntityShape::square;
            else throw std::invalid_argument("entity_shape: expected circle or square");
        }
        if (json.contains("replication_margin")) {
            auto const& field = json.at("replication_margin");
            if (!field.is_number()) throw std::invalid_argument("replication_margin: expected a number");
            double number = field.get<double>();
            if (!std::isfinite(number) || number < 0 || number > MaxBlocks)
                throw std::invalid_argument("replication_margin: outside allowed bounds");
            result.replicationMargin = static_cast<float>(number);
            if (number > 0 && result.replicationMargin == 0)
                throw std::invalid_argument("replication_margin: value is not representable as a positive float");
        }
        result.validate();
        return result;
    }
};
}
