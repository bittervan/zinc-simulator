#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace zinc::sim {

class Memory {
public:
    Memory(std::uint64_t base, std::uint64_t size);

    [[nodiscard]] std::uint64_t base() const { return base_; }
    [[nodiscard]] std::uint64_t size() const { return bytes_.size(); }

    void load(std::uint64_t address, std::span<const std::uint8_t> data);
    [[nodiscard]] std::uint8_t read_u8(std::uint64_t address) const;
    [[nodiscard]] std::uint16_t read_u16(std::uint64_t address) const;
    [[nodiscard]] std::uint32_t read_u32(std::uint64_t address) const;
    [[nodiscard]] std::uint64_t read_u64(std::uint64_t address) const;
    void write(std::uint64_t address, std::uint64_t value, std::uint8_t size);

private:
    [[nodiscard]] std::uint64_t offset_of(std::uint64_t address, std::uint64_t size) const;

    std::uint64_t base_;
    std::vector<std::uint8_t> bytes_;
};

}  // namespace zinc::sim
