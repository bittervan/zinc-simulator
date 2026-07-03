#pragma once

#include <cstdint>
#include <vector>

class Memory {
public:
    Memory(std::uint64_t base, std::uint64_t size);
    void load(std::uint64_t addr, const std::vector<std::uint8_t> &data);
    void clear(std::uint64_t addr, std::uint64_t size);

private:
    std::uint64_t base;
    std::vector<std::uint8_t> data;
};