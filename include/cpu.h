#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <ram.h>

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
};

class Cpu {
public:
    Cpu(std::uint64_t init_pc);
    Commit step(Memory &mem);

private:
    std::uint64_t pc;
    std::vector<std::uint64_t> gprs;
};