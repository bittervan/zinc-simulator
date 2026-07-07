#define NUM_GPRS 32

#include <cpu.h>
#include <decode.h>
#include <stdexcept>
#include <fstream>
#include <format>

Cpu::Cpu(std::uint64_t init_pc) : pc(init_pc), gprs(NUM_GPRS, 0) {}

Commit Cpu::step(Memory &mem) {
    std::uint32_t insn = mem.get_32(pc);
    Commit ret {
        .pc = pc,
        .insn = insn,
        .reg_writes = {},
        .mem_reads = {},
        .mem_writes = {},
    };

    DecodedInsn decoded = Decoder::decode(insn);

    if (std::holds_alternative<InvalidInsn>(decoded)) {
        throw std::runtime_error(
            std::format("Invalid instruction at {:016x}: {:08x}", pc, insn)
        );
    }

    this->pc += 4;

    return ret;
}