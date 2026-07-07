#include <decode.h>

DecodedInsn Decoder::decode(std::uint32_t insn) {
    DecodedInsn ret = InvalidInsn{};
    switch (insn & INSN_MASK_OPCODE) {
        case OPCODE_JAL: {
            ret = JalInsn{
                .imm = get_j_type_imm(insn),
                .rd = get_rd(insn),
            };
        }
    }
    return ret;
}
