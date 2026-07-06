#include <decode.h>

DecodedInsn Decoder::decode(std::uint32_t insn) {
    DecodedInsn ret = InvalidInsn{};
    switch (insn & INSN_MASK_OPCODE) {
        case OPCODE_LUI: {

        }
        default: {
            InvalidInsn
        }
    }
    return ret;
}
