#include "range/Config.h"
#include "range/Limits.h"
#include <fstream>
#include <windows.h>

namespace urde {
nlohmann::json readJson(std::filesystem::path const& file) {
    std::ifstream stream(file);
    if (!stream) throw std::runtime_error("Cannot read config: " + file.string());
    return nlohmann::json::parse(stream);
}
void writeJson(std::filesystem::path const& file, nlohmann::json const& json) {
    std::filesystem::create_directories(file.parent_path());
    auto temp = file;
    temp += L".tmp";
    {
        std::ofstream stream(temp, std::ios::trunc);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        stream << json.dump(4) << '\n';
        stream.close();
    }
    if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace config (Windows error " + std::to_string(GetLastError()) + ")");
}
nlohmann::json migrateKeys(nlohmann::json json, std::initializer_list<std::pair<char const*,char const*>> aliases) {
    if (!json.is_object()) throw std::invalid_argument("Expected config object");
    for (auto [key,legacy] : aliases) {
        if (!json.contains(legacy)) continue;
        if (json.contains(key))
            throw std::invalid_argument(std::string("Ambiguous config: both ")+key+" and legacy "+legacy+" are present");
        json[key] = json.at(legacy);
        json.erase(legacy);
    }
    return json;
}
float readRange(nlohmann::json const& json, char const* key, float max, bool integral) {
    auto const& field = json.at(key);
    if (field.is_string() && field.get<std::string>() == "vanilla") return 0;
    if (!field.is_number()) throw std::invalid_argument(std::string(key) + ": expected number or vanilla");
    double value = field.get<double>();
    if (!std::isfinite(value) || value <= 0 || value > max || (integral && std::floor(value) != value))
        throw std::invalid_argument(std::string(key) + ": range outside allowed bounds");
    float result = static_cast<float>(value);
    if (!std::isfinite(result) || result <= 0)
        throw std::invalid_argument(std::string(key) + ": value is not representable as a positive float");
    return result;
}
nlohmann::json encodeRange(float value) {
    if (value == 0) return "vanilla";
    return value;
}
}
