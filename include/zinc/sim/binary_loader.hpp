#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace zinc::sim {

std::vector<std::uint8_t> load_binary(const std::filesystem::path& path);

}  // namespace zinc::sim
