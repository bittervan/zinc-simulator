#include "zinc/sim/binary_loader.hpp"

#include <fstream>
#include <ios>
#include <stdexcept>

namespace zinc::sim {

std::vector<std::uint8_t> load_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("failed to open binary: " + path.string());
    }

    const auto size = input.tellg();
    if (size < 0) {
        throw std::runtime_error("failed to determine binary size: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("failed to read binary: " + path.string());
    }

    return bytes;
}

}  // namespace zinc::sim
