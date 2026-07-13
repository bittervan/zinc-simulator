#pragma once

#include <cstdint>
#include <vector>
#include <variant>

class Memory {
public:
    Memory(std::uint64_t base, std::uint64_t size);
    void load(std::uint64_t addr, const std::vector<std::uint8_t> &data);
    void clear(std::uint64_t addr, std::uint64_t size);

    std::uint8_t get_8(std::uint64_t addr) const;
    std::uint16_t get_16(std::uint64_t addr) const;
    std::uint32_t get_32(std::uint64_t addr) const;
    std::uint64_t get_64(std::uint64_t addr) const;

    void set_8(std::uint64_t addr, std::uint8_t data);
    void set_16(std::uint64_t addr, std::uint16_t data);
    void set_32(std::uint64_t addr, std::uint32_t data);
    void set_64(std::uint64_t addr, std::uint64_t data);

private:
    std::uint64_t base;
    std::vector<std::uint8_t> data;
};