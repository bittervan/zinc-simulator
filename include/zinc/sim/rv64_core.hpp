#pragma once

#include "zinc/sim/memory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zinc::sim {

struct RegisterWrite {
    std::uint8_t reg = 0;
    std::uint64_t value = 0;
};

struct MemoryWrite {
    std::uint64_t address = 0;
    std::uint8_t size = 0;
    std::uint64_t value = 0;
};

struct CommitTrace {
    std::uint64_t pc = 0;
    std::uint32_t instruction = 0;
    std::optional<RegisterWrite> write;
    std::vector<MemoryWrite> memory_writes;
    std::string disassembly;
};

class Rv64Core {
public:
    Rv64Core(Memory& memory, std::uint64_t entry);

    [[nodiscard]] std::uint64_t pc() const { return pc_; }
    [[nodiscard]] std::uint64_t x(std::uint8_t reg) const { return x_[reg]; }

    CommitTrace step();

private:
    void write_x(std::uint8_t reg, std::uint64_t value, CommitTrace& trace);

    Memory& memory_;
    std::uint64_t pc_;
    std::array<std::uint64_t, 32> x_{};
};

}  // namespace zinc::sim
