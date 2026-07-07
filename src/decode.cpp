#include <decode.h>

DecodedInsn Decoder::decode(std::uint32_t insn) {
    DecodedInsn ret = InvalidInsn{};
    switch (insn & INSN_MASK_OPCODE) {
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
    }
    return ret;
}
