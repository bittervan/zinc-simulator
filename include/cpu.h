#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <ram.h>
#include <array>

constexpr std::uint64_t MSTATUS_MIE_MASK      = 1ULL << 3;
constexpr std::uint64_t MSTATUS_MPIE_MASK     = 1ULL << 7;
constexpr std::uint64_t MSTATUS_MPP_MASK = 0b11ULL << 11;
constexpr std::uint64_t MSTATUS_MPRV_MASK     = 1ULL << 17;
constexpr std::uint64_t MSTATUS_UXL_MASK =
    0b11ULL << 32;

constexpr std::uint64_t MSTATUS_UXL_RV64 =
    0b10ULL << 32;

constexpr std::uint64_t MSTATUS_WRITABLE_MASK =
    MSTATUS_MIE_MASK |
    MSTATUS_MPIE_MASK |
    MSTATUS_MPP_MASK |
    MSTATUS_MPRV_MASK;

enum class RegType: std::uint32_t {
    X,
    Csr,
    F,
};

enum class Csr : std::uint32_t {
    Satp     = 0x180,
    Mstatus  = 0x300,
    Medeleg  = 0x302,
    Mideleg  = 0x303,
    Mie      = 0x304,
    Mtvec    = 0x305,
    Mscratch = 0x340,
    Mepc     = 0x341,
    Mcause   = 0x342,
    Mtval    = 0x343,
    Pmpcfg0  = 0x3a0,
    Pmpaddr0 = 0x3b0,
    Mhartid  = 0xf14,
};

enum class Privilege : std::uint32_t {
    User       = 0b00,
    Supervisor = 0b01,
    Machine    = 0b11,
};

class Exception {
public:
    std::uint64_t mcause;
    std::uint64_t mtval;
};

class RegWrite {
public:
    RegType type;
    std::uint32_t num;
    std::uint64_t value;

    RegWrite(RegType type, std::uint32_t num, std::uint64_t value);
    // RegWrite(Csr csr, std::uint64_t value);
};

class MemAccess {
public:
    std::uint64_t addr;
    std::uint64_t value;
    std::uint32_t size;

    MemAccess(std::uint64_t addr, std::uint64_t value, std::uint32_t size);
};

class Commit {
public:
    std::uint64_t pc;
    std::uint32_t insn;
    Privilege priv;
    std::vector<RegWrite> reg_writes;
    std::vector<MemAccess> mem_reads;
    std::vector<MemAccess> mem_writes;

    std::string to_string() const;
};

class Core {
public:
    Core(std::uint64_t init_pc);
    // Commit step(Memory &mem);

    std::uint64_t get_pc() const;
    void set_pc(std::uint64_t new_pc);


    std::uint64_t get_gpr(std::uint32_t index) const;
    void set_gpr(std::uint32_t index, std::uint64_t value);

    bool has_csr(uint32_t csr) const;
    std::uint64_t get_csr(Csr csr) const;
    std::uint64_t set_csr(Csr csr, std::uint64_t value);

    Privilege get_priv() const;
    void set_priv(Privilege priv);

    void take_exception(const Exception& exception);

private:
    Privilege priv = Privilege::Machine;
    std::uint64_t pc;
    std::vector<std::uint64_t> gprs;
    std::array<std::uint64_t, 4096> csrs;
};