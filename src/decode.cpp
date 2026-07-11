#include <decode.h>

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

static inline std::uint32_t get_opcode(std::uint32_t insn) {
    return insn & 0b0000000'00000'00000'000'00000'1111111;
}

static inline std::int64_t get_u_type_imm(std::uint32_t insn) {
    return static_cast<int64_t>(
        static_cast<int32_t>(insn & 0b1111111'11111'11111'111'00000'0000000)
    );
}

static inline std::int64_t sign_extend(std::uint64_t value, int n_bits) {
    int n_shift = 64 - n_bits;
    return static_cast<std::int64_t>(value << n_shift) >> n_shift;
}

static inline std::int64_t get_j_type_imm(std::uint32_t insn) {
    std::uint32_t u_imm =   (((insn >> 31) & 0x1) << 20) |
                            (((insn >> 21) & 0x3ff) << 1) |
                            (((insn >> 20) & 0x1) << 11) |
                            (((insn >> 12) & 0xff) << 12);
    
    return sign_extend(u_imm, 21);
}

static inline std::uint32_t get_rd(std::uint32_t insn) {
    return 0x1f & (insn >> 7);
}

static inline std::uint32_t get_funct3(std::uint32_t insn) {
    return 0x7 & (insn >> 12);
}

static inline std::uint32_t get_funct7(std::uint32_t insn) {
    return 0x7f & (insn >> 25);
}

static inline std::uint32_t get_shamt(std::uint32_t insn) {
    return 0x1f & (insn >> 20);
}

static inline std::uint32_t get_rs2(std::uint32_t insn) {
    return 0x1f & (insn >> 20);
}

static inline std::uint32_t get_zicsr_csr(std::uint32_t insn) {
    return 0xfff & (insn >> 20);
}

static inline std::uint32_t get_rs1(std::uint32_t insn) {
    return 0x1f & (insn >> 15);
}

static inline std::uint32_t get_zicsr_uimm(std::uint32_t insn) {
    return 0x1f & (insn >> 15);
}

static inline std::int64_t get_i_type_imm(std::uint32_t insn) {
    return sign_extend(0xfff & (insn >> 20), 12);
}

static inline std::int64_t get_s_type_imm(std::uint32_t insn) {
    std::uint32_t u_imm =   (((insn >> 25) & 0x7f) << 5) |
                            (((insn >> 7) & 0x1f));

    return sign_extend(u_imm, 12);
}

static inline std::int64_t get_b_type_imm(std::uint32_t insn) {
    std::uint32_t u_imm =   (((insn >> 31) & 0x1) << 12) |
                            (((insn >> 25) & 0x3f) << 5) |
                            (((insn >> 8) & 0xf) << 1)   |
                            (((insn >> 7) & 0x1) << 11);

    return sign_extend(u_imm, 13);
}

DecodedInsn Decoder::decode(std::uint32_t insn) {
    DecodedInsn ret = InvalidInsn{};
    switch (get_opcode(insn)) {
        case OPCODE_JAL: {
            ret = JalInsn{
                .imm = get_j_type_imm(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_SYSTEM: {
            SystemInsnType type;
            uint32_t funct3 = get_funct3(insn);
            uint32_t csr = get_zicsr_csr(insn);

            if (!funct3) {
                type = static_cast<SystemInsnType>(get_i_type_imm(insn) << 3 | funct3);
            } else {
                type = static_cast<SystemInsnType>(funct3);
            }

            ret = SystemInsn{
                .type = type,
                .csr = csr,
                .rs1 = get_rs1(insn),
                .uimm = get_zicsr_uimm(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_OP_IMM: {
            OpType type;
            uint32_t funct3 = get_funct3(insn);
            int64_t imm;
            if (funct3 == 0b101 || funct3 == 0b001) {
                type = static_cast<OpType>(get_i_type_imm(insn) >> 7 | funct3);
                imm = get_shamt(insn);
            } else {
                type = static_cast<OpType>(funct3);
                imm = get_i_type_imm(insn);
            }

            ret = OpImmInsn{
                .type = type,
                .imm = imm,
                .rs1 = get_rs1(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_BRANCH: {
            BranchType type = static_cast<BranchType>(get_funct3(insn));
            
            ret = BranchInsn{
                .type = type,
                .imm = get_b_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn)
            };
            break;
        }
        case OPCODE_JALR: {
            ret = JalrInsn{
                .imm = get_i_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_AUIPC: {
            ret = AuipcInsn{
                .imm = get_u_type_imm(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_STORE: {
            MemAccessType type = static_cast<MemAccessType>(get_funct3(insn));
            ret = StoreInsn{
                .type = type,
                .imm = get_s_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn)
            };
            break;
        }
        case OPCODE_LOAD: {
            MemAccessType type = static_cast<MemAccessType>(get_funct3(insn));
            ret = LoadInsn{
                .type = type,
                .imm = get_i_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_OP_IMM_32: {
            OpType type;
            uint32_t funct3 = get_funct3(insn);
            int64_t imm;
            if (funct3 == 0b101) {
                type = static_cast<OpType>(get_funct7(insn) << 3 | funct3);
                imm = get_shamt(insn);
            } else {
                type = static_cast<OpType>(funct3);
                imm = get_i_type_imm(insn);
            }

            ret = OpImm32Insn{
                .type = type,
                .imm = imm,
                .rs1 = get_rs1(insn),
                .rd = get_rd(insn)
            };

            break;
        }
        case OPCODE_MISC_MEM: {
            ret = MiscMemInsn{
                .type = static_cast<MiscMemInsnType>(get_funct3(insn)),
            };
            break;
        }
        case OPCODE_LUI: {
            ret = LuiInsn{
                .imm = get_u_type_imm(insn),
                .rd = get_rd(insn),
            };
            break;
        }
        case OPCODE_OP: {
            uint32_t funct3 = get_funct3(insn);
            OpType type = static_cast<OpType>(get_funct7(insn) << 3 | funct3);

            ret =  OpInsn{
                .type = type,
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn),
                .rd = get_rd(insn),
            };
            break;
        }
    }
    return ret;
}
