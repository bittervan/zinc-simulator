#include <decode.h>

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
                if (csr) {
                    type = SystemInsnType::Ebreak;
                } else {
                    type = SystemInsnType::Ecall;
                }
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
            if (funct3 == 0b101) {
                type = static_cast<OpType>(get_funct7(insn) << 3 | funct3);
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
