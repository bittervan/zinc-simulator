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

    core.set_pc(result.next_pc.value_or(core.get_pc() + 4));
    
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

StepResult Executor::execute(const Core &, const Memory &, const LuiInsn& insn) {
    StepResult ret;
    ret.reg_writes.push_back(
        RegWrite{
            .type = "x",
            .num = insn.rd,
            .value = static_cast<uint64_t>(insn.imm)
        }
    );
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const AuipcInsn& insn) {
    StepResult ret;
    uint64_t result = core.get_pc() + insn.imm;

    ret.reg_writes.push_back(
        RegWrite{
            .type = "x",
            .num = insn.rd,
            .value = result
        }
    );

    return ret;
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

StepResult Executor::execute(const Core &core, const Memory &, const BranchInsn& insn) {
    StepResult ret;
    bool should_branch = false;
    std::uint64_t rs1_val = core.get_gpr(insn.rs1);
    std::uint64_t rs2_val = core.get_gpr(insn.rs2);

    switch (insn.type) {
        case BranchType::Eq: {
            should_branch = rs1_val == rs2_val;
            break;
        }
        case BranchType::Ge: {
            should_branch = static_cast<std::int64_t>(rs1_val) >= static_cast<std::int64_t>(rs2_val);
            break;
        }
        case BranchType::Geu: {
            should_branch = rs1_val >= rs2_val;
            break;
        }
        case BranchType::Lt: {
            should_branch = static_cast<std::int64_t>(rs1_val) < static_cast<std::int64_t>(rs2_val);
            break;
        }
        case BranchType::Ltu: {
            should_branch = rs1_val < rs2_val;
            break;
        }
        case BranchType::Ne: {
            should_branch = rs1_val != rs2_val;
            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Not a valid BranchType for Branch instruction: {:08x}: {:010b}", core.get_pc(),static_cast<std::uint32_t>(insn.type))
            );
        }
    }

    if (should_branch) {
        ret.next_pc = core.get_pc() + insn.imm;
    }

    return ret;
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
    std::uint64_t imm_val = insn.imm;
    std::uint64_t rs1_val = core.get_gpr(insn.rs1);
    std::uint64_t rd_val = 0;
    std::uint32_t shamt = insn.imm & 0x3f;

    switch (insn.type) {
        case OpType::Add: {
            rd_val = imm_val + rs1_val;
            break;
        }
        case OpType::Slt: {
            if (static_cast<std::int64_t>(rs1_val) < static_cast<std::int64_t>(imm_val)) {
                rd_val = 1;
            } else {
                rd_val = 0;
            }
            break;
        }
        case OpType::Sltu: {
            if (rs1_val < imm_val) {
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
        case OpType::And: {
            rd_val = rs1_val & imm_val;
            break;
        }
        case OpType::Sll: {
            rd_val = rs1_val << shamt;
            break;
        }
        case OpType::Srl: {
            rd_val = rs1_val >> shamt;
            break;
        }
        case OpType::Sra: {
            rd_val = static_cast<int64_t>(rs1_val) >> shamt;
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
            .value = rd_val,
        }
    );

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
            std::uint64_t value = core.get_gpr(insn.rs1);
            std::uint64_t old_csr_val = core.get_csr(insn.csr);
            std::uint64_t new_csr_val = old_csr_val | value;
            
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
        case SystemInsnType::Csrrw: {
            std::uint64_t old_csr_val = core.get_csr(insn.csr);
            std::uint64_t new_csr_val = core.get_gpr(insn.rs1);
            
            ret.reg_writes.push_back(
                RegWrite{
                    .type = "x",
                    .num = insn.rd,
                    .value = old_csr_val
                }
            );

            ret.reg_writes.push_back(
                RegWrite{
                    .type = "csr",
                    .num = insn.csr,
                    .value = new_csr_val
                }
            );

            break;
        }
        case SystemInsnType::Csrrwi: {
            std::uint64_t old_csr_val = core.get_csr(insn.csr);
            std::uint64_t new_csr_val = core.get_gpr(insn.uimm);
            
            ret.reg_writes.push_back(
                RegWrite{
                    .type = "x",
                    .num = insn.rd,
                    .value = old_csr_val
                }
            );

            if (new_csr_val) {
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

        case SystemInsnType::Mret: {

        }
        default: {
            throw std::runtime_error(
                std::format("Unimplemented System Instruction: {:015b} at {:08x}", static_cast<uint32_t>(insn.type), core.get_pc())
            );
        }
    }

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const OpImm32Insn& insn) {

    StepResult ret;
    std::uint32_t imm_val = insn.imm;
    std::uint32_t rs1_val = core.get_gpr(insn.rs1);
    std::uint32_t rd_val = 0;
    std::uint32_t shamt = static_cast<std::uint32_t>(insn.imm) & 0x1f;


    switch (insn.type) {
        case OpType::Add: {
            rd_val = imm_val + rs1_val;
            break;
        }
        case OpType::Sll: {
            rd_val = rs1_val << shamt;
            break;
        }
        case OpType::Srl: {
            rd_val = rs1_val >> shamt;
            break;
        }
        case OpType::Sra: {
            rd_val = static_cast<std::uint32_t>(static_cast<std::int32_t>(rs1_val) >> shamt);
            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Not a valid OpType for OpImm32 instruction: {:08x}: {:010b}", core.get_pc(),static_cast<std::uint32_t>(insn.type))
            );
        }
    }

    ret.reg_writes.push_back(
        RegWrite{
            .type = "x",
            .num = insn.rd,
            .value = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(rd_val))),
        }
    );

    return ret;
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