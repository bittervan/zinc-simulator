#include "zinc/sim/rv64_core.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace zinc::sim {
namespace {

std::uint8_t bits(std::uint32_t value, unsigned high, unsigned low) {
    const auto width = high - low + 1;
    const auto mask = (1U << width) - 1U;
    return static_cast<std::uint8_t>((value >> low) & mask);
}

std::int64_t sign_extend(std::uint64_t value, unsigned bits) {
    const auto shift = 64U - bits;
    return static_cast<std::int64_t>(value << shift) >> shift;
}

std::string xname(std::uint8_t reg) {
    return "x" + std::to_string(reg);
}

std::string hex_instruction(std::uint32_t instruction) {
    std::ostringstream out;
    out << "0x" << std::hex;
    out.width(8);
    out.fill('0');
    out << instruction;
    return out.str();
}

}  // namespace

Rv64Core::Rv64Core(Memory& memory, std::uint64_t entry) : memory_(memory), pc_(entry) {}

CommitTrace Rv64Core::step() {
    CommitTrace trace;
    trace.pc = pc_;
    trace.instruction = memory_.read_u32(pc_);

    const auto opcode = bits(trace.instruction, 6, 0);
    const auto rd = bits(trace.instruction, 11, 7);
    const auto funct3 = bits(trace.instruction, 14, 12);
    const auto rs1 = bits(trace.instruction, 19, 15);

    switch (opcode) {
        case 0x17: {  // AUIPC
            const auto imm = static_cast<std::uint64_t>(trace.instruction & 0xfffff000U);
            write_x(rd, pc_ + imm, trace);
            pc_ += 4;

            std::ostringstream out;
            out << "auipc " << xname(rd) << ", 0x" << std::hex << (imm >> 12U);
            trace.disassembly = out.str();
            break;
        }
        case 0x13: {  // OP-IMM
            if (funct3 != 0x0) {
                throw std::runtime_error("unsupported OP-IMM instruction " + hex_instruction(trace.instruction));
            }

            const auto imm = sign_extend(trace.instruction >> 20U, 12);
            write_x(rd, x_[rs1] + static_cast<std::uint64_t>(imm), trace);
            pc_ += 4;

            std::ostringstream out;
            out << "addi " << xname(rd) << ", " << xname(rs1) << ", " << std::dec << imm;
            trace.disassembly = out.str();
            break;
        }
        case 0x67: {  // JALR
            if (funct3 != 0x0) {
                throw std::runtime_error("unsupported JALR instruction " + hex_instruction(trace.instruction));
            }

            const auto imm = sign_extend(trace.instruction >> 20U, 12);
            const auto next_pc = pc_ + 4;
            const auto target = (x_[rs1] + static_cast<std::uint64_t>(imm)) & ~std::uint64_t{1};
            write_x(rd, next_pc, trace);
            pc_ = target;

            std::ostringstream out;
            out << "jalr " << xname(rd) << ", " << xname(rs1) << ", " << std::dec << imm;
            trace.disassembly = out.str();
            break;
        }
        case 0x6f: {  // JAL
            const auto imm20 = (trace.instruction >> 31U) & 0x1U;
            const auto imm10_1 = (trace.instruction >> 21U) & 0x3ffU;
            const auto imm11 = (trace.instruction >> 20U) & 0x1U;
            const auto imm19_12 = (trace.instruction >> 12U) & 0xffU;
            const auto imm = sign_extend((imm20 << 20U) | (imm19_12 << 12U) | (imm11 << 11U) |
                                             (imm10_1 << 1U),
                                         21);
            write_x(rd, pc_ + 4, trace);
            pc_ = pc_ + static_cast<std::uint64_t>(imm);

            std::ostringstream out;
            out << "jal " << xname(rd) << ", " << std::dec << imm;
            trace.disassembly = out.str();
            break;
        }
        default:
            throw std::runtime_error("unsupported instruction " + hex_instruction(trace.instruction));
    }

    x_[0] = 0;
    return trace;
}

void Rv64Core::write_x(std::uint8_t reg, std::uint64_t value, CommitTrace& trace) {
    if (reg == 0) {
        return;
    }

    x_[reg] = value;
    trace.write = RegisterWrite{reg, value};
}

}  // namespace zinc::sim
