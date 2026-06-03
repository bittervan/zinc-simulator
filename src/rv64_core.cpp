#include "zinc/sim/rv64_core.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

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

std::uint64_t sign_extend_u(std::uint64_t value, unsigned bits) {
    return static_cast<std::uint64_t>(sign_extend(value, bits));
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

std::string op3(std::string_view name, std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2) {
    std::ostringstream out;
    out << name << ' ' << xname(rd) << ", " << xname(rs1) << ", " << xname(rs2);
    return out.str();
}

std::string op_imm(std::string_view name, std::uint8_t rd, std::uint8_t rs1, std::int64_t imm) {
    std::ostringstream out;
    out << name << ' ' << xname(rd) << ", " << xname(rs1) << ", " << std::dec << imm;
    return out.str();
}

std::string branch_text(std::string_view name, std::uint8_t rs1, std::uint8_t rs2, std::int64_t imm) {
    std::ostringstream out;
    out << name << ' ' << xname(rs1) << ", " << xname(rs2) << ", " << std::dec << imm;
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
    const auto rs2 = bits(trace.instruction, 24, 20);
    const auto funct7 = bits(trace.instruction, 31, 25);

    switch (opcode) {
        case 0x37: {  // LUI
            const auto imm = trace.instruction & 0xfffff000U;
            write_x(rd, sign_extend_u(imm, 32), trace);
            pc_ += 4;

            std::ostringstream out;
            out << "lui " << xname(rd) << ", 0x" << std::hex << (imm >> 12U);
            trace.disassembly = out.str();
            break;
        }
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
            const auto imm = sign_extend(trace.instruction >> 20U, 12);
            const auto shamt = bits(trace.instruction, 25, 20);
            switch (funct3) {
                case 0x0:
                    write_x(rd, x_[rs1] + static_cast<std::uint64_t>(imm), trace);
                    trace.disassembly = op_imm("addi", rd, rs1, imm);
                    break;
                case 0x2:
                    write_x(rd, static_cast<std::int64_t>(x_[rs1]) < imm ? 1 : 0, trace);
                    trace.disassembly = op_imm("slti", rd, rs1, imm);
                    break;
                case 0x3:
                    write_x(rd, x_[rs1] < static_cast<std::uint64_t>(imm) ? 1 : 0, trace);
                    trace.disassembly = op_imm("sltiu", rd, rs1, imm);
                    break;
                case 0x4:
                    write_x(rd, x_[rs1] ^ static_cast<std::uint64_t>(imm), trace);
                    trace.disassembly = op_imm("xori", rd, rs1, imm);
                    break;
                case 0x6:
                    write_x(rd, x_[rs1] | static_cast<std::uint64_t>(imm), trace);
                    trace.disassembly = op_imm("ori", rd, rs1, imm);
                    break;
                case 0x7:
                    write_x(rd, x_[rs1] & static_cast<std::uint64_t>(imm), trace);
                    trace.disassembly = op_imm("andi", rd, rs1, imm);
                    break;
                case 0x1:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLLI instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] << shamt, trace);
                    trace.disassembly = op_imm("slli", rd, rs1, shamt);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        write_x(rd, x_[rs1] >> shamt, trace);
                        trace.disassembly = op_imm("srli", rd, rs1, shamt);
                    } else if (funct7 == 0x20) {
                        write_x(rd, static_cast<std::uint64_t>(static_cast<std::int64_t>(x_[rs1]) >> shamt), trace);
                        trace.disassembly = op_imm("srai", rd, rs1, shamt);
                    } else {
                        throw std::runtime_error("unsupported shift-immediate instruction " +
                                                 hex_instruction(trace.instruction));
                    }
                    break;
                default:
                    throw std::runtime_error("unsupported OP-IMM instruction " + hex_instruction(trace.instruction));
            }
            pc_ += 4;
            break;
        }
        case 0x1b: {  // OP-IMM-32
            const auto imm = sign_extend(trace.instruction >> 20U, 12);
            const auto shamt = bits(trace.instruction, 24, 20);
            switch (funct3) {
                case 0x0: {
                    const auto result = static_cast<std::uint32_t>(x_[rs1]) + static_cast<std::uint32_t>(imm);
                    write_x(rd, sign_extend_u(result, 32), trace);
                    trace.disassembly = op_imm("addiw", rd, rs1, imm);
                    break;
                }
                case 0x1:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLLIW instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, sign_extend_u(static_cast<std::uint32_t>(x_[rs1]) << shamt, 32), trace);
                    trace.disassembly = op_imm("slliw", rd, rs1, shamt);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        write_x(rd, sign_extend_u(static_cast<std::uint32_t>(x_[rs1]) >> shamt, 32), trace);
                        trace.disassembly = op_imm("srliw", rd, rs1, shamt);
                    } else if (funct7 == 0x20) {
                        const auto result = static_cast<std::uint32_t>(static_cast<std::int32_t>(x_[rs1]) >> shamt);
                        write_x(rd, sign_extend_u(result, 32), trace);
                        trace.disassembly = op_imm("sraiw", rd, rs1, shamt);
                    } else {
                        throw std::runtime_error("unsupported word shift-immediate instruction " +
                                                 hex_instruction(trace.instruction));
                    }
                    break;
                default:
                    throw std::runtime_error("unsupported OP-IMM-32 instruction " + hex_instruction(trace.instruction));
            }
            pc_ += 4;
            break;
        }
        case 0x33: {  // OP
            switch (funct3) {
                case 0x0:
                    if (funct7 == 0x00) {
                        write_x(rd, x_[rs1] + x_[rs2], trace);
                        trace.disassembly = op3("add", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        write_x(rd, x_[rs1] - x_[rs2], trace);
                        trace.disassembly = op3("sub", rd, rs1, rs2);
                    } else {
                        throw std::runtime_error("unsupported ADD/SUB instruction " + hex_instruction(trace.instruction));
                    }
                    break;
                case 0x1:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLL instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] << (x_[rs2] & 0x3fU), trace);
                    trace.disassembly = op3("sll", rd, rs1, rs2);
                    break;
                case 0x2:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLT instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, static_cast<std::int64_t>(x_[rs1]) < static_cast<std::int64_t>(x_[rs2]) ? 1 : 0, trace);
                    trace.disassembly = op3("slt", rd, rs1, rs2);
                    break;
                case 0x3:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLTU instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] < x_[rs2] ? 1 : 0, trace);
                    trace.disassembly = op3("sltu", rd, rs1, rs2);
                    break;
                case 0x4:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported XOR instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] ^ x_[rs2], trace);
                    trace.disassembly = op3("xor", rd, rs1, rs2);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        write_x(rd, x_[rs1] >> (x_[rs2] & 0x3fU), trace);
                        trace.disassembly = op3("srl", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        write_x(rd,
                                static_cast<std::uint64_t>(static_cast<std::int64_t>(x_[rs1]) >> (x_[rs2] & 0x3fU)),
                                trace);
                        trace.disassembly = op3("sra", rd, rs1, rs2);
                    } else {
                        throw std::runtime_error("unsupported SRL/SRA instruction " + hex_instruction(trace.instruction));
                    }
                    break;
                case 0x6:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported OR instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] | x_[rs2], trace);
                    trace.disassembly = op3("or", rd, rs1, rs2);
                    break;
                case 0x7:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported AND instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, x_[rs1] & x_[rs2], trace);
                    trace.disassembly = op3("and", rd, rs1, rs2);
                    break;
                default:
                    throw std::runtime_error("unsupported OP instruction " + hex_instruction(trace.instruction));
            }
            pc_ += 4;
            break;
        }
        case 0x3b: {  // OP-32
            const auto lhs = static_cast<std::uint32_t>(x_[rs1]);
            const auto rhs = static_cast<std::uint32_t>(x_[rs2]);
            switch (funct3) {
                case 0x0:
                    if (funct7 == 0x00) {
                        write_x(rd, sign_extend_u(lhs + rhs, 32), trace);
                        trace.disassembly = op3("addw", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        write_x(rd, sign_extend_u(lhs - rhs, 32), trace);
                        trace.disassembly = op3("subw", rd, rs1, rs2);
                    } else {
                        throw std::runtime_error("unsupported ADDW/SUBW instruction " +
                                                 hex_instruction(trace.instruction));
                    }
                    break;
                case 0x1:
                    if (funct7 != 0x00) {
                        throw std::runtime_error("unsupported SLLW instruction " + hex_instruction(trace.instruction));
                    }
                    write_x(rd, sign_extend_u(lhs << (x_[rs2] & 0x1fU), 32), trace);
                    trace.disassembly = op3("sllw", rd, rs1, rs2);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        write_x(rd, sign_extend_u(lhs >> (x_[rs2] & 0x1fU), 32), trace);
                        trace.disassembly = op3("srlw", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        const auto result = static_cast<std::uint32_t>(static_cast<std::int32_t>(lhs) >> (x_[rs2] & 0x1fU));
                        write_x(rd, sign_extend_u(result, 32), trace);
                        trace.disassembly = op3("sraw", rd, rs1, rs2);
                    } else {
                        throw std::runtime_error("unsupported SRLW/SRAW instruction " +
                                                 hex_instruction(trace.instruction));
                    }
                    break;
                default:
                    throw std::runtime_error("unsupported OP-32 instruction " + hex_instruction(trace.instruction));
            }
            pc_ += 4;
            break;
        }
        case 0x03: {  // LOAD
            const auto imm = sign_extend(trace.instruction >> 20U, 12);
            const auto address = x_[rs1] + static_cast<std::uint64_t>(imm);
            switch (funct3) {
                case 0x0:
                    write_x(rd, sign_extend_u(memory_.read_u8(address), 8), trace);
                    trace.disassembly = op_imm("lb", rd, rs1, imm);
                    break;
                case 0x1:
                    write_x(rd, sign_extend_u(memory_.read_u16(address), 16), trace);
                    trace.disassembly = op_imm("lh", rd, rs1, imm);
                    break;
                case 0x2:
                    write_x(rd, sign_extend_u(memory_.read_u32(address), 32), trace);
                    trace.disassembly = op_imm("lw", rd, rs1, imm);
                    break;
                case 0x3:
                    write_x(rd, memory_.read_u64(address), trace);
                    trace.disassembly = op_imm("ld", rd, rs1, imm);
                    break;
                case 0x4:
                    write_x(rd, memory_.read_u8(address), trace);
                    trace.disassembly = op_imm("lbu", rd, rs1, imm);
                    break;
                case 0x5:
                    write_x(rd, memory_.read_u16(address), trace);
                    trace.disassembly = op_imm("lhu", rd, rs1, imm);
                    break;
                case 0x6:
                    write_x(rd, memory_.read_u32(address), trace);
                    trace.disassembly = op_imm("lwu", rd, rs1, imm);
                    break;
                default:
                    throw std::runtime_error("unsupported LOAD instruction " + hex_instruction(trace.instruction));
            }
            pc_ += 4;
            break;
        }
        case 0x23: {  // STORE
            const auto imm = sign_extend(((trace.instruction >> 25U) << 5U) | rd, 12);
            const auto address = x_[rs1] + static_cast<std::uint64_t>(imm);
            std::uint8_t size = 0;
            std::string_view name;
            switch (funct3) {
                case 0x0:
                    size = 1;
                    name = "sb";
                    break;
                case 0x1:
                    size = 2;
                    name = "sh";
                    break;
                case 0x2:
                    size = 4;
                    name = "sw";
                    break;
                case 0x3:
                    size = 8;
                    name = "sd";
                    break;
                default:
                    throw std::runtime_error("unsupported STORE instruction " + hex_instruction(trace.instruction));
            }
            memory_.write(address, x_[rs2], size);
            const auto mask = size == 8 ? ~std::uint64_t{0} : ((std::uint64_t{1} << (size * 8U)) - 1U);
            trace.memory_writes.push_back(MemoryWrite{address, size, x_[rs2] & mask});
            pc_ += 4;

            std::ostringstream out;
            out << name << ' ' << xname(rs2) << ", " << std::dec << imm << '(' << xname(rs1) << ')';
            trace.disassembly = out.str();
            break;
        }
        case 0x63: {  // BRANCH
            const auto imm = sign_extend((((trace.instruction >> 31U) & 0x1U) << 12U) |
                                             (((trace.instruction >> 7U) & 0x1U) << 11U) |
                                             (((trace.instruction >> 25U) & 0x3fU) << 5U) |
                                             (((trace.instruction >> 8U) & 0xfU) << 1U),
                                         13);
            bool taken = false;
            std::string_view name;
            switch (funct3) {
                case 0x0:
                    taken = x_[rs1] == x_[rs2];
                    name = "beq";
                    break;
                case 0x1:
                    taken = x_[rs1] != x_[rs2];
                    name = "bne";
                    break;
                case 0x4:
                    taken = static_cast<std::int64_t>(x_[rs1]) < static_cast<std::int64_t>(x_[rs2]);
                    name = "blt";
                    break;
                case 0x5:
                    taken = static_cast<std::int64_t>(x_[rs1]) >= static_cast<std::int64_t>(x_[rs2]);
                    name = "bge";
                    break;
                case 0x6:
                    taken = x_[rs1] < x_[rs2];
                    name = "bltu";
                    break;
                case 0x7:
                    taken = x_[rs1] >= x_[rs2];
                    name = "bgeu";
                    break;
                default:
                    throw std::runtime_error("unsupported BRANCH instruction " + hex_instruction(trace.instruction));
            }
            pc_ = taken ? pc_ + static_cast<std::uint64_t>(imm) : pc_ + 4;
            trace.disassembly = branch_text(name, rs1, rs2, imm);
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
