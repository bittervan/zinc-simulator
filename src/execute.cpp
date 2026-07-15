#include <execute.h>
#include <variant>
#include <stdexcept>
#include <format>

std::optional<Commit> Executor::step(Core &core, Memory &mem, const DecodedInsn &insn) {
    StepResult step_result = std::visit(
        [&](const auto& concrete_insn) -> StepResult {
            return execute(core, mem, concrete_insn);
        },
        insn
    );

    if (const Exception* exception = std::get_if<Exception>(&step_result)) {
        core.take_exception(*exception);
        return std::nullopt;
    }

    NormalStep* normal_step = std::get_if<NormalStep>(&step_result);
    NormalStep& result = *normal_step;

    std::uint64_t pc = core.get_pc();
    std::uint32_t raw_insn = mem.get_32(pc);
    Privilege current_priv = core.get_priv();
    Commit ret;

    // Drop x0 write.
    std::erase_if(result.reg_writes, [](const RegWrite &write) {
        return write.type == RegType::X && write.num == 0;
    });

    bool writes_fpr = std::any_of(result.reg_writes.begin(), result.reg_writes.end(), [](const RegWrite& write) {
        return write.type == RegType::F;
    });

    if (writes_fpr) {
        std::uint64_t mstatus =
            core.get_csr(Csr::Mstatus);

        std::uint64_t new_mstatus =
            (mstatus & ~MSTATUS_FS_MASK) |
            MSTATUS_FS_DIRTY;

        result.reg_writes.emplace_back(
            RegType::Csr,
            static_cast<std::uint32_t>(Csr::Mstatus),
            new_mstatus
        );
    }

    core.set_pc(result.next_pc.value_or(core.get_pc() + 4));
    core.set_priv(result.next_privilege.value_or(core.get_priv()));
    
    for (RegWrite &reg_write : result.reg_writes) {
        if (reg_write.type == RegType::X) {
            core.set_gpr(reg_write.num, reg_write.value);
        } else if (reg_write.type == RegType::Csr) {
            reg_write.value = core.set_csr(static_cast<Csr>(reg_write.num), reg_write.value);
        } else if (reg_write.type == RegType::F) {
            core.set_fpr(reg_write.num, reg_write.value);
        } else {
            throw std::runtime_error(
                std::format("Unrecognized RegWrite type {}", static_cast<uint32_t>(reg_write.type))
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

    ret = Commit{
        .pc = pc,
        .insn = raw_insn,
        .priv = current_priv,
        .reg_writes = std::move(result.reg_writes),
        .mem_reads = std::move(result.mem_reads),
        .mem_writes = std::move(result.mem_writes),
    };

    return ret;
}

StepResult Executor::execute(const Core &, const Memory &, const LuiInsn& insn) {
    NormalStep ret;
    ret.reg_writes.emplace_back(RegType::X, insn.rd, static_cast<std::uint64_t>(insn.imm));
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const AuipcInsn& insn) {
    NormalStep ret;
    uint64_t result = core.get_pc() + insn.imm;
    ret.reg_writes.emplace_back(RegType::X, insn.rd, result);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory&, const JalInsn& insn) {
    NormalStep ret;
    ret.next_pc = static_cast<int64_t>(core.get_pc()) + insn.imm;
    ret.reg_writes.emplace_back(RegType::X, insn.rd, core.get_pc() + 4);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const JalrInsn& insn) {
    NormalStep ret;
    ret.next_pc = core.get_gpr(insn.rs1) + insn.imm;
    ret.reg_writes.emplace_back(RegType::X, insn.rd, core.get_pc() + 4);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const BranchInsn& insn) {
    NormalStep ret;
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

bool mem_access_aligned(uint64_t addr, MemAccessType type) {
    switch (type) {
        case MemAccessType::B :
        case MemAccessType::Bu :
            return true;
        case MemAccessType::H:
        case MemAccessType::Hu:
            if (addr % 2 == 0) {
                return true;
            } else {
                return false;
            }
        case MemAccessType::W:
        case MemAccessType::Wu:
            if (addr % 4 == 0) {
                return true;
            } else {
                return false;
            }
        case MemAccessType::D:
            if (addr % 8 == 0) {
                return true;
            } else {
                return false;
            }
        default:
            throw std::runtime_error("Not a valid memory access type");
    }
}

StepResult Executor::execute(const Core &core, const Memory &mem, const LoadInsn& insn) {
    NormalStep ret;
    std::uint64_t addr = core.get_gpr(insn.rs1) + insn.imm;
    std::uint64_t data;

    if (!mem_access_aligned(addr, insn.type)) {
        return Exception {
            .mcause = 4,
            .mtval = addr,
        };
    }
    
    switch (insn.type) {
        case MemAccessType::B : {
            std::int8_t raw = mem.get_8(addr);
            data = static_cast<std::int64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 1);
            break;
        }
        case MemAccessType::Bu : {
            std::uint8_t raw = mem.get_8(addr);
            data = static_cast<std::uint64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 1);
            break;
        }
        case MemAccessType::H : {
            std::int16_t raw = mem.get_16(addr);
            data = static_cast<std::int64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 2);
            break;
        }
        case MemAccessType::Hu : {
            std::uint16_t raw = mem.get_16(addr);
            data = static_cast<std::uint64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 2);
            break;
        }
        case MemAccessType::W : {
            std::int32_t raw = mem.get_32(addr);
            data = static_cast<std::int64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 4);
            break;
        }
        case MemAccessType::Wu : {
            std::uint32_t raw = mem.get_32(addr);
            data = static_cast<std::uint64_t>(raw);
            ret.mem_reads.emplace_back(addr, data, 4);
            break;
        }
        case MemAccessType::D : {
            data = mem.get_64(addr);
            ret.mem_reads.emplace_back(addr, data, 8);           
            break;
        }
    }

    ret.reg_writes.emplace_back(RegType::X, insn.rd, data);

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const StoreInsn& insn) {
    NormalStep ret;

    std::uint64_t addr = core.get_gpr(insn.rs1) + static_cast<uint64_t>(insn.imm);
    std::uint64_t val = core.get_gpr(insn.rs2);
    std::uint32_t size = 0;

    if (!mem_access_aligned(addr, insn.type)) {
        return Exception {
            .mcause = 6,
            .mtval = addr,
        };
    }

    switch (insn.type) {
        case MemAccessType::B: {
            val = val & 0xff;
            size = 1;
            break;
        }
        case MemAccessType::H: {
            val = val & 0xffff;
            size = 2;
            break;
        }
        case MemAccessType::W: {
            val = val & 0xffff'ffffULL;
            size = 4;
            break;
        }
        case MemAccessType::D: {
            size = 8;
            break;
        }
        default: {
            throw std::runtime_error(
                std::format(
                    "Invalid STORE type at {:016x}: {:03b}", core.get_pc(), static_cast<std::uint32_t>(insn.type)
                )
            );
        }
    }

    ret.mem_writes.emplace_back(addr, val, size);

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const OpImmInsn& insn) {
    NormalStep ret;
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

    ret.reg_writes.emplace_back(RegType::X, insn.rd, rd_val);

    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const OpInsn& insn) {
    NormalStep ret;
    std::uint64_t rs1_val = core.get_gpr(insn.rs1);
    std::uint64_t rs2_val = core.get_gpr(insn.rs2);
    std::uint64_t rd_val = 0;
    std::uint32_t shamt = static_cast<std::uint32_t>(rs2_val & 0x3f);

    switch (insn.type) {
        case OpType::Add: {
            rd_val = rs1_val + rs2_val;
            break;
        }
        case OpType::Sub: {
            rd_val = rs1_val - rs2_val;
            break;
        }
        case OpType::Slt: {
            if (static_cast<std::int64_t>(rs1_val) < static_cast<std::int64_t>(rs2_val)) {
                rd_val = 1;
            } else {
                rd_val = 0;
            }
            break;
        }
        case OpType::Sltu: {
            if (rs1_val < rs2_val) {
                rd_val = 1;
            } else {
                rd_val = 0;
            }
            break;
        }
        case OpType::Xor: {
            rd_val = rs1_val ^ rs2_val;
            break;
        }
        case OpType::Or: {
            rd_val = rs1_val | rs2_val;
            break;
        }
        case OpType::And: {
            rd_val = rs1_val & rs2_val;
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

    // ret.reg_writes.push_back(
    //     RegWrite{
    //         .type = "x",
    //         .num = insn.rd,
    //         .value = rd_val,
    //     }
    // );
    ret.reg_writes.emplace_back(RegType::X, insn.rd, rd_val);

    return ret;

}

StepResult Executor::execute(const Core &core, const Memory &, const MiscMemInsn& insn) {
    switch (insn.type) {
        case MiscMemInsnType::Fence: {
            break;
        }
        case MiscMemInsnType::FenceI: {
            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Unimplemented MiscMem Instruction: {:015b} at {:08x}", static_cast<uint32_t>(insn.type), core.get_pc())
            );
        }
    }
    NormalStep ret;
    return ret;
}

static inline bool is_csr_insn(const SystemInsn& insn) {
    switch (insn.type) {
        case SystemInsnType::Csrrc:
        case SystemInsnType::Csrrw:
        case SystemInsnType::Csrrs:
        case SystemInsnType::Csrrci:
        case SystemInsnType::Csrrwi:
        case SystemInsnType::Csrrsi:
            return true;
        default:
            return false;
    }
}

StepResult Executor::execute(const Core &core, const Memory &, const SystemInsn& insn) {
    NormalStep ret;
    if (is_csr_insn(insn) && !core.has_csr(insn.csr)) {
        return Exception{
            .mcause = 2,
            .mtval = 0,
        };
    }
    switch (insn.type) {
        case SystemInsnType::Csrrs: {
            std::uint64_t value = core.get_gpr(insn.rs1);
            std::uint64_t old_csr_val = core.get_csr(static_cast<Csr>(insn.csr));
            std::uint64_t new_csr_val = old_csr_val | value;
            
            ret.reg_writes.emplace_back(RegType::X, insn.rd, old_csr_val);

            if (insn.rs1) {
                ret.reg_writes.emplace_back(RegType::Csr, insn.csr, new_csr_val);
            }

            break;
        }
        case SystemInsnType::Csrrw: {
            std::uint64_t old_csr_val = core.get_csr(static_cast<Csr>(insn.csr));
            std::uint64_t new_csr_val = core.get_gpr(insn.rs1);
            
            ret.reg_writes.emplace_back(RegType::X, insn.rd, old_csr_val);
            ret.reg_writes.emplace_back(RegType::Csr, insn.csr, new_csr_val);

            break;
        }
        case SystemInsnType::Csrrwi: {
            std::uint64_t old_csr_val = core.get_csr(static_cast<Csr>(insn.csr));
            std::uint64_t new_csr_val = insn.uimm;
            
            ret.reg_writes.emplace_back(RegType::X, insn.rd, old_csr_val);

            ret.reg_writes.emplace_back(RegType::Csr, insn.csr, new_csr_val);

            break;
        }

        case SystemInsnType::Mret: {
            ret.next_pc = core.get_csr(Csr::Mepc);

            std::uint64_t old_mstatus = core.get_csr(Csr::Mstatus);
            std::uint64_t new_mstatus = old_mstatus;

            std::uint64_t mpp = (old_mstatus & MSTATUS_MPP_MASK) >> 11;
            if (old_mstatus & MSTATUS_MPIE_MASK) {
                new_mstatus |= MSTATUS_MIE_MASK;
            } else {
                new_mstatus &= ~MSTATUS_MIE_MASK;
            }

            new_mstatus |= MSTATUS_MPIE_MASK;
            new_mstatus &= ~MSTATUS_MPP_MASK;

            Privilege next_privilege = static_cast<Privilege>(mpp);

            if (next_privilege != Privilege::Machine) {
                new_mstatus &= ~MSTATUS_MPRV_MASK;
            }
            
            ret.next_privilege = static_cast<Privilege>(mpp);

            ret.reg_writes.emplace_back(RegType::Csr, static_cast<uint32_t>(Csr::Mstatus), new_mstatus);

            break;
        }
        case SystemInsnType::Ecall: {
            std::uint64_t cause;
            switch (core.get_priv()) {
                case Privilege::User: {
                    cause = 8;
                    break;
                }
                case Privilege::Supervisor: {
                    throw std::runtime_error("We have not implemented S mode");
                }
                case Privilege::Machine: {
                    cause = 11;
                    break;
                }
                default: {
                    throw std::runtime_error(
                        std::format("Ecall at unknown priviledge {}", static_cast<uint32_t>(core.get_priv()))
                    );
                }
            }

            return Exception {
                .mcause = cause,
                .mtval = 0, 
            };
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

    NormalStep ret;
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

    std::uint64_t new_val = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(rd_val)));

    ret.reg_writes.emplace_back(RegType::X, insn.rd, new_val);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const Op32Insn& insn) {
    NormalStep ret;
    std::uint32_t rs1_val = core.get_gpr(insn.rs1);
    std::uint32_t rs2_val = core.get_gpr(insn.rs2);
    std::int32_t rd_val = 0;
    std::uint32_t shamt = static_cast<std::uint32_t>(rs2_val) & 0x3f;

    switch (insn.type) {
        case OpType::Add: {
            rd_val = rs1_val + rs2_val;
            break;
        }
        case OpType::Sub: {
            rd_val = rs1_val - rs2_val;
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
            rd_val = static_cast<std::int32_t>(rs1_val) >> shamt;
            break;
        }
        default: {
            throw std::runtime_error(
                std::format("Not a valid OpType for OpImm32 instruction: {:08x}: {:010b}", core.get_pc(),static_cast<std::uint32_t>(insn.type))
            );
        }
    }

    std::uint64_t new_val = static_cast<std::uint64_t>(static_cast<std::int64_t>(rd_val));

    ret.reg_writes.emplace_back(RegType::X, insn.rd, new_val);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const LoadFpInsn& insn) {
    NormalStep ret;
    ret.reg_writes.emplace_back(RegType::F, insn.rd, mem.get_32(core.get_gpr(insn.rs1) + insn.imm));
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &, const StoreFpInsn& insn) {
    NormalStep ret;
    ret.mem_writes.emplace_back(core.get_gpr(insn.rs1) + insn.imm, core.get_fpr(insn.rs2), 4);
    return ret;
}

StepResult Executor::execute(const Core &core, const Memory &mem, const OpFpInsn& insn) {
    NormalStep ret;
    switch (insn.type) {
        case OpFpType::Add : {
            
        }
        default: {
            break;
        }
    }
}

StepResult Executor::execute(const Core &core, const Memory &mem, const InvalidInsn& insn) {
    (void)core;
    (void)mem;
    (void)insn;
    throw std::runtime_error(std::format("Execute for INVALID is not implemented at {:08x}", core.get_pc()));
}