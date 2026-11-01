#pragma once
#include <filesystem>

namespace urde::side {
bool load(std::filesystem::path const& configDir);
bool enable();
bool disable();
}
