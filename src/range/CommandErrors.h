#pragma once
#include <string_view>
#include <vector>

namespace urde {
// Recognize only the old misuse cases. Native parsing owns every other error.
inline std::string_view commandMisuse(std::string_view line, bool client) {
    std::vector<std::string_view> words;
    while (!line.empty()) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == line.npos) break;
        line.remove_prefix(start);
        auto end = line.find_first_of(" \t\r\n");
        words.push_back(line.substr(0,end));
        if (end == line.npos) break;
        line.remove_prefix(end);
    }
    if (words.empty()) return {};
    auto root = words[0];
    if (root.starts_with('/')) root.remove_prefix(1);
    if (root != (client ? "urdec" : "urde") || words.size() < 2) return {};
    if ((words[1] == "status" || words[1] == "reload") && words.size() > 2)
        return "status/reload take no key or value";
    if (words[1] != "set") return {};
    if (words.size() == 2) return "set requires a key and value: set <key> <value>";
    if (client && words.size() == 3 && words[2] == "entity_shape")
        return "set requires a value: set <key> <value>";
    if (words.size() != 4) return {};
    if (client && words[2] == "entity_shape") {
        if (words[3] == "vanilla") return "vanilla is not supported for entity_shape";
        if (words[3] != "circle" && words[3] != "square") return "entity_shape requires circle or square";
    }
    if (client && (words[3] == "circle" || words[3] == "square") && words[2] != "entity_shape")
        return "circle/square require entity_shape";
    return {};
}
}
