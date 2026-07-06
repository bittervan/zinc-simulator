#define NUM_GPRS 32

#include <cpu.h>
#include <decode.h>
#include <stdexcept>

Cpu::Cpu(std::uint64_t init_pc) : pc(init_pc), gprs(NUM_GPRS, 0) {}

Commit Cpu::step(std::uint64_t pc, Memory &mem) {
    std::uint32_t insn = mem.get_32(pc);
    Commit ret {
        .pc = pc,
        .insn = insn,
        .reg_writes = {},
        .mem_reads = {},
        .mem_writes = {},
    };

    const std::uint32_t opcode = insn & 0x7f;

    switch (opcode) {
        case 0b0110111:
        default: {
            throw std::runtime_error("Not a valid opcode");
        }
    }

    return ret;
}