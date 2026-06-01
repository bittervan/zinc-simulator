#include "zinc/sim/memory.hpp"

#include <algorithm>
#include <stdexcept>

namespace zinc::sim {

Memory::Memory(std::uint64_t base, std::uint64_t size) : base_(base), bytes_(size) {}

void Memory::load(std::uint64_t address, std::span<const std::uint8_t> data) {
    const auto offset = offset_of(address, data.size());
    std::copy(data.begin(), data.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::uint32_t Memory::read_u32(std::uint64_t address) const {
    const auto offset = offset_of(address, 4);
    return static_cast<std::uint32_t>(bytes_[offset]) |
           (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24U);
}

std::uint64_t Memory::offset_of(std::uint64_t address, std::uint64_t size) const {
    if (address < base_) {
        throw std::out_of_range("memory access below base address");
    }

    const auto offset = address - base_;
    if (offset > bytes_.size() || size > bytes_.size() - offset) {
        throw std::out_of_range("memory access outside memory");
    }

    return offset;
}

}  // namespace zinc::sim
