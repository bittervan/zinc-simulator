#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <ram.h>
#include <array>

class RegWrite {
public:
    std::string type;
    std::uint32_t num;
    std::uint64_t value;
};

class MemAccess {
public:
    std::uint64_t addr;
    std::uint64_t value;
    std::uint32_t size;
};

class Commit {
public:
    std::uint64_t pc;
    std::uint32_t insn;
    std::vector<RegWrite> reg_writes;
    std::vector<MemAccess> mem_reads;
    std::vector<MemAccess> mem_writes;

    std::string to_string() const;
};

enum class Privilege {
    Machine =       0b00,
    Supervisor =    0b01,
    User =          0b11,
};

class Core {
public:
    Core(std::uint64_t init_pc);
    // Commit step(Memory &mem);

    std::uint64_t get_pc() const;
    void set_pc(std::uint64_t new_pc);

    std::uint64_t get_gpr(uint32_t index) const;
    void set_gpr(uint32_t index, uint64_t value);

    std::uint64_t get_csr(uint32_t csr) const;
    void set_csr(uint32_t csr, uint64_t value);

private:
    Privilege priv = Privilege::Machine;
    std::uint64_t pc;
    std::vector<std::uint64_t> gprs;
    std::array<std::uint64_t, 4096> csrs;
};