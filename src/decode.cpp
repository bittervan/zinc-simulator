#include <decode.h>
#include <format>
#include <stdexcept>

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

constexpr std::uint32_t OPCODE_LOAD_FP =    0b0000111;
constexpr std::uint32_t OPCODE_STORE_FP =   0b0100111;
constexpr std::uint32_t OPCODE_MADD_FP  =   0b1000011;
constexpr std::uint32_t OPCODE_MSUB_FP  =   0b1000111;
constexpr std::uint32_t OPCODE_NMSUB_FP =   0b1001011;
constexpr std::uint32_t OPCODE_NMADD_FP =   0b1001111;
constexpr std::uint32_t OPCODE_OP_FP =      0b1010011;

constexpr std::uint32_t OP_FP_FUNCT7_ADD_S =                    0b0000000;
constexpr std::uint32_t OP_FP_FUNCT7_SUB_S =                    0b0000100;
constexpr std::uint32_t OP_FP_FUNCT7_MUL_S =                    0b0001000;
constexpr std::uint32_t OP_FP_FUNCT7_DIV_S =                    0b0001100;
constexpr std::uint32_t OP_FP_FUNCT7_SGNJ_S =                   0b0010000;
constexpr std::uint32_t OP_FP_FUNCT7_MIN_MAX_S =                0b0010100;
constexpr std::uint32_t OP_FP_FUNCT7_SQRT_S =                   0b0101100;
constexpr std::uint32_t OP_FP_FUNCT7_COMPARE_S =                0b1010000;
constexpr std::uint32_t OP_FP_FUNCT7_CVT_TO_INT_S =             0b1100000;
constexpr std::uint32_t OP_FP_FUNCT7_CVT_FROM_INT_S =           0b1101000;
constexpr std::uint32_t OP_FP_FUNCT7_MOVE_TO_INT_OR_CLASS_S =   0b1110000;
constexpr std::uint32_t OP_FP_FUNCT7_MOVE_FROM_INT_S =          0b1111000;

constexpr std::uint32_t OP_SGNJ_FUNCT3_INJECT = 0b000;
constexpr std::uint32_t OP_SGNJ_FUNCT3_INJECT_NEGATED = 0b001;
constexpr std::uint32_t OP_SGNJ_FUNCT3_INJECT_XOR = 0b010;

constexpr std::uint32_t OP_FMV_FUNCT3 = 0b000;
constexpr std::uint32_t OP_FCLASS_FUNCT3 = 0b001;
constexpr std::uint32_t OP_EQ_FUNCT3 = 0b010;
constexpr std::uint32_t OP_LT_FUNCT3 = 0b001;
constexpr std::uint32_t OP_LE_FUNCT3 = 0b000;

constexpr std::uint32_t OP_FCVT_RS2_W = 0b00000;
constexpr std::uint32_t OP_FCVT_RS2_WU = 0b00001;
constexpr std::uint32_t OP_FCVT_RS2_L = 0b00010;
constexpr std::uint32_t OP_FCVT_RS2_LU = 0b00011;

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
    return 0x3f & (insn >> 20);
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
                type = static_cast<OpType>((((insn >> 26) & 0x3f) << 4) | funct3);
                imm = (insn >> 20) & 0x3f;
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
            if (funct3 == 0b101 || funct3 == 0b001) {
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
        case OPCODE_OP_32: {
            ret = Op32Insn {
                .type = static_cast<OpType>(get_funct7(insn) << 3 | get_funct3(insn)),
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn),
                .rd = get_rd(insn)
            };
            break;
        }
        case OPCODE_LOAD_FP: {
            ret = LoadFpInsn {
                .imm = get_i_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rd = get_rd(insn)
            };
            break;
        }
        case OPCODE_STORE_FP: {
            ret = StoreFpInsn {
                .imm = get_s_type_imm(insn),
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn),
            };
            break;
        }
        case OPCODE_OP_FP: {
            std::uint32_t funct7 = get_funct7(insn);
            std::uint32_t funct3 = get_funct3(insn);
            OpFpType type;
            
            switch (funct7) {
                case OP_FP_FUNCT7_ADD_S: {
                    type = OpFpType::Add;
                    break;
                }
                case OP_FP_FUNCT7_SUB_S: {
                    type = OpFpType::Sub;
                    break;
                }
                case OP_FP_FUNCT7_MUL_S: {
                    type = OpFpType::Mul;
                    break;
                }
                case OP_FP_FUNCT7_DIV_S: {
                    type = OpFpType::Div;
                    break;
                }
                case OP_FP_FUNCT7_SGNJ_S: {
                    switch (funct3) {
                        case OP_SGNJ_FUNCT3_INJECT: {
                            type = OpFpType::Sgnj;
                            break;
                        }
                        case OP_SGNJ_FUNCT3_INJECT_NEGATED: {
                            type = OpFpType::Sgnjn;
                            break;
                        }
                        case OP_SGNJ_FUNCT3_INJECT_XOR: {
                            type = OpFpType::Sgnjx;
                            break;
                        }
                        default: {
                            throw std::runtime_error(
                                std::format("Invalid funct3 {:03b} for Sign Injection instruction.", funct3)
                            );
                        }
                    }
                    break;
                }
                case OP_FP_FUNCT7_MOVE_TO_INT_OR_CLASS_S: {
                    if (get_rs2(insn) != 0) {
                        throw std::runtime_error(
                            std::format("Invalid FMV.X.W/FCLASS.S instruction {:08x}", insn)
                        );
                    }

                    switch (funct3) {
                        case OP_FMV_FUNCT3: {
                            type = OpFpType::MoveToX;
                            break;
                        }
                        case OP_FCLASS_FUNCT3: {
                            type = OpFpType::Class;
                            break;
                        }
                        default: {
                            throw std::runtime_error(
                                std::format("Invalid funct3 {:03b} for FMV.X.W/FCLASS.S instruction", funct3)
                            );
                        }
                    }
                    break;
                }
                case OP_FP_FUNCT7_MOVE_FROM_INT_S: {
                    if (funct3 != OP_FMV_FUNCT3 || get_rs2(insn) != 0) {
                        throw std::runtime_error(
                            std::format("Invalid FMV.W.X instruction {:08x}", insn)
                        );
                    }
                    type = OpFpType::MoveFromX;
                    break;
                }
                case OP_FP_FUNCT7_COMPARE_S: {
                    switch (funct3) {
                        case OP_EQ_FUNCT3: {
                            type = OpFpType::Eq;
                            break;
                        }
                        case OP_LT_FUNCT3: {
                            type = OpFpType::Lt;
                            break;
                        }
                        case OP_LE_FUNCT3: {
                            type = OpFpType::Le;
                            break;
                        }
                        default: {
                            throw std::runtime_error(
                                std::format("Invalid funct3 {:03b} for FP compare instruction", funct3)
                            );
                        }
                    }
                    break;
                }
                case OP_FP_FUNCT7_CVT_TO_INT_S: {
                    switch (get_rs2(insn)) {
                        case OP_FCVT_RS2_W: {
                            type = OpFpType::CvtWFromS;
                            break;
                        }
                        case OP_FCVT_RS2_WU: {
                            type = OpFpType::CvtWuFromS;
                            break;
                        }
                        case OP_FCVT_RS2_L: {
                            type = OpFpType::CvtLFromS;
                            break;
                        }
                        case OP_FCVT_RS2_LU: {
                            type = OpFpType::CvtLuFromS;
                            break;
                        }
                        default: {
                            throw std::runtime_error(
                                std::format("Invalid rs2 {:05b} for FP-to-integer conversion", get_rs2(insn))
                            );
                        }
                    }
                    break;
                }
                case OP_FP_FUNCT7_CVT_FROM_INT_S: {
                    switch (get_rs2(insn)) {
                        case OP_FCVT_RS2_W: {
                            type = OpFpType::CvtSToW;
                            break;
                        }
                        case OP_FCVT_RS2_WU: {
                            type = OpFpType::CvtSToWu;
                            break;
                        }
                        case OP_FCVT_RS2_L: {
                            type = OpFpType::CvtSToL;
                            break;
                        }
                        case OP_FCVT_RS2_LU: {
                            type = OpFpType::CvtSToLu;
                            break;
                        }
                        default: {
                            throw std::runtime_error(
                                std::format("Invalid rs2 {:05b} for integer-to-FP conversion", get_rs2(insn))
                            );
                        }
                    }
                    break;
                }
                default: {
                    throw std::runtime_error(
                        std::format("Funct7 {:07b} for OpFpInsn not implemented, cannot decode", funct7)
                    );
                }
            }

            ret = OpFpInsn {
                // .funct7 = get_funct7(insn),
                .type = type,
                .rm = static_cast<RoundingMode>(get_funct3(insn)),
                .rs1 = get_rs1(insn),
                .rs2 = get_rs2(insn),
                // .funct3 = get_funct3(insn),
                .rd = get_rd(insn)
            };
            break;
        }
    }
    return ret;
}
