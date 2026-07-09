#pragma once

#include <cstdint>
#include <variant>
#include <cpu.h>

enum class BranchType : std::uint32_t {
    Eq =    0b000,
    Ne =    0b001,
    Lt =    0b100,
    Ge =    0b101,
    Ltu =   0b110,
    Geu =   0b111,
};

enum class MemAccessType : std::uint32_t {
    B =     0b000,
    H =     0b001,
    W =     0b010,
    D =     0b011,
    Bu =    0b100,
    Hu =    0b101,
    Wu =    0b110,
};

enum class OpType : std::uint32_t {
    Add    = 0b0000000'000,
    Sub    = 0b0100000'000,
    Sll    = 0b0000000'001,
    Slt    = 0b0000000'010,
    Sltu   = 0b0000000'011,
    Xor    = 0b0000000'100,
    Srl    = 0b0000000'101,
    Sra    = 0b0100000'101,
    Or     = 0b0000000'110,
    And    = 0b0000000'111,
};

enum class SystemInsnType {
    Ecall =     0b000,
    Csrrw =     0b001,
    Csrrs =     0b010,
    Csrrc =     0b011,
    Ebreak =    0b100,
    Csrrwi =    0b101,
    Csrrsi =    0b110,
    Csrrci =    0b111,
};

enum class MiscMemInsnType {
    Fence =     0b000,
    FenceI =    0b001,
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

struct LuiInsn {
    std::int64_t imm;
    std::uint32_t rd;
};

struct AuipcInsn {
    std::int64_t imm;
    std::uint32_t rd;
};

struct JalInsn {
    std::int64_t imm;
    std::uint32_t rd;
};

struct JalrInsn {
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rd;
};

struct BranchInsn {
    BranchType type;
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rs2;
};

struct LoadInsn {
    MemAccessType type;
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rd;
};

struct StoreInsn {
    MemAccessType type;
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rs2;
};

struct OpImmInsn {
    OpType type;
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rd;
};

struct OpInsn {
    OpType type;
    std::uint32_t rs1;
    std::uint32_t rs2;
    std::uint32_t rd;
};

struct MiscMemInsn {
    MiscMemInsnType type;
};

struct SystemInsn {
    SystemInsnType type;
    std::uint32_t csr;
    std::uint32_t rs1;
    std::uint32_t uimm;
    std::uint32_t rd;
};

struct OpImm32Insn {
    OpType type;
    std::int64_t imm;
    std::uint32_t rs1;
    std::uint32_t rd;
};

struct Op32Insn {
    OpType type;
    std::uint32_t rs1;
    std::uint32_t rs2;
    std::uint32_t rd;
};

struct InvalidInsn {};

using DecodedInsn = std::variant<
    LuiInsn,
    AuipcInsn,
    JalInsn,
    JalrInsn,
    BranchInsn,
    LoadInsn,
    StoreInsn,
    OpImmInsn,
    OpInsn,
    MiscMemInsn,
    SystemInsn,
    OpImm32Insn,
    Op32Insn,
    InvalidInsn
>;

class Decoder {
public:
    static DecodedInsn decode(std::uint32_t insn);
};
