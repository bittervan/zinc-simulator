#pragma once

#include <cstdint>
#include <variant>

constexpr std::uint32_t OPCODE_LUI =         0b0110111;
constexpr std::uint32_t OPCODE_AUIPC =       0b0010111;
constexpr std::uint32_t OPCODE_JAL =         0b1101111;
constexpr std::uint32_t OPCODE_JALR =        0b1100111;
constexpr std::uint32_t OPCODE_BRANCH =      0b1100011;
constexpr std::uint32_t OPCODE_LOAD =        0b0000011;
constexpr std::uint32_t OPCODE_STORE =       0b0100011;
constexpr std::uint32_t OPCODE_OP_IMM =      0b0010011;
constexpr std::uint32_t OPCODE_OP =          0b0110011;
constexpr std::uint32_t OPCODE_MISC_MEM =    0b0001111;
constexpr std::uint32_t OPCODE_SYSTEM =      0b1110011;
constexpr std::uint32_t OPCODE_OP_IMM_32 =   0b0011011;
constexpr std::uint32_t OPCODE_OP_32 =       0b0111011;

constexpr std::uint32_t INSN_MASK_OPCODE =   0b0000000'00000'00000'000'00000'1111111;

constexpr std::uint32_t INSN_U_IMM_MASK =    0b1111111'11111'11111'111'00000'0000000;

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
    Ecall,
    Ebreak,
    Csrrw,
    Csrrs,
    Csrrc,
    Csrrwi,
    Csrrsi,
    Csrrci,
};

enum class MiscMemInsnType {
    Fence,
    FenceI,
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
