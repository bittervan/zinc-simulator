#include <execute.h>
#include <variant>
#include <stdexcept>
#include <format>

Commit Executor::step(Core &core, Memory &mem, const DecodedInsn &insn) {
    StepResult result = std::visit(
        [&](const auto& concrete_insn) -> StepResult {
            return execute(core, mem, concrete_insn);
        },
        insn
    );

    std::uint64_t pc = core.get_pc();
    std::uint32_t raw_insn = mem.get_32(pc);

    // Drop x0 write.
    std::erase_if(result.reg_writes, [](const RegWrite &write) {
        return write.type == "x" && write.num == 0;
    });

    core.set_pc(result.next_pc);
    
    for (const RegWrite &reg_write : result.reg_writes) {
        if (reg_write.type == "x") {
            core.set_gpr(reg_write.num, reg_write.value);
        } else if (reg_write.type == "csr") {
            core.set_csr(reg_write.num, reg_write.value);
        } else {
            throw std::runtime_error(
                std::format("Unrecognized RegWrite type {}", reg_write.type)
            );
        }
    }

    for (const MemAccess &mem_write : result.mem_writes) {
        switch (mem_write.size) {
            case 1: {
                mem.set_8(mem_write.addr, mem_write.value);
                break;
            }
            case 2: {
                mem.set_16(mem_write.addr, mem_write.value);
                break;
            }
            case 4: {
                mem.set_32(mem_write.addr, mem_write.value);
                break;
            }
            case 8: {
                mem.set_64(mem_write.addr, mem_write.value);
                break;
            }
            default: {
                throw std::runtime_error(
                    std::format("Unsupported memory write size: {}", mem_write.size)
                );
            }
        }
    }

    Commit ret {
        .pc = pc,
        .insn = raw_insn,
        .reg_writes = std::move(result.reg_writes),
        .mem_reads = std::move(result.mem_reads),
        .mem_writes = std::move(result.mem_writes),
    };

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const LuiInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for LUI is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const AuipcInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for AUIPC is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory&, const JalInsn& insn) {
    StepResult ret;
    uint64_t new_pc = static_cast<int64_t>(core.get_pc()) + insn.imm;
    uint64_t pc_plus_4 = core.get_pc() + 4;

    ret.reg_writes.push_back(
        RegWrite{
            .type = "x",
            .num = insn.rd,
            .value = pc_plus_4,
        }
    );

    ret.next_pc = new_pc;

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const JalrInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for JALR is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const BranchInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for BRANCH is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const LoadInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for LOAD is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const StoreInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for STORE is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &, const OpImmInsn& insn) {
    StepResult ret;
    std::int64_t imm_val = insn.imm;
    std::int64_t rs1_val = core.get_gpr(insn.rs1);
    std::int64_t rd_val = 0;

    switch (insn.type) {
        case OpType::Add: {
            rd_val = imm_val + rs1_val;
            break;
        }
        case OpType::Slt: {
            if (rs1_val < imm_val) {
                rd_val = 1;
            } else {
                rd_val = 0;
            }
            break;
        }
        case OpType::Sltu: {
            if (static_cast<std::uint64_t>(rs1_val) < static_cast<std::uint64_t>(imm_val)) {
                rd_val = 1;
            } else {
                rd_val = 0;
            }
            break;
        }
        case OpType::Xor: {
            rd_val = rs1_val ^ imm_val;
            break;
        }
        case OpType::Or: {
            rd_val = rs1_val | imm_val;
            break;
        }
        case OpType::Sll: {
            rd_val = rs1_val << imm_val;
            break;
        }
        case OpType::Srl: {
            rd_val = static_cast<std::uint64_t>(rs1_val) >> imm_val;
            break;
        }
        case OpType::Sra: {
            rd_val = rs1_val >> imm_val;
            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Not a valid OpType for OpImm instruction: {:08x}: {:010b}", core.get_pc(),static_cast<std::uint32_t>(insn.type))
            );
        }
    }

    ret.reg_writes.push_back(
        RegWrite{
            .type = "x",
            .num = insn.rd,
            .value = static_cast<uint64_t>(rd_val),
        }
    );

    ret.next_pc = core.get_pc() + 4;

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const OpInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for OP is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const MiscMemInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for MISC-MEM is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &, const SystemInsn& insn) {
    StepResult ret;
    switch (insn.type) {
        case SystemInsnType::Csrrs: {
            uint64_t value = core.get_gpr(insn.rs1);
            uint64_t old_csr_val = core.get_csr(insn.csr);
            uint64_t new_csr_val = old_csr_val | value;
            
            ret.reg_writes.push_back(
                RegWrite{
                    .type = "x",
                    .num = insn.rd,
                    .value = old_csr_val
                }
            );

            if (insn.rs1) {
                ret.reg_writes.push_back(
                    RegWrite{
                        .type = "csr",
                        .num = insn.csr,
                        .value = new_csr_val
                    }
                );
            }

            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Unimplemented System Instruction: {:03b}", static_cast<uint32_t>(insn.type))
            );
        }
    }

    ret.next_pc = core.get_pc() + 4;
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const OpImm32Insn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for OP-IMM-32 is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const Op32Insn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for OP-32 is not implemented at {:08x}", core.get_pc()));
}

StepResult Executor::execute(const Core &core, const Memory &mem, const InvalidInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for INVALID is not implemented at {:08x}", core.get_pc()));
}