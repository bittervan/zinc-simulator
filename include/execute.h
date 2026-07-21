#pragma once

#include <decode.h>
#include <cpu.h>
#include <memory.h>
#include <optional>

struct NormalStep {
    // std::optional<Exception> exception;
    std::optional<std::uint64_t> next_pc;       // Default value will be pc + 4
    std::optional<Privilege> next_privilege;    // Default value, will be current privilege
    std::vector<RegWrite> reg_writes;
    std::vector<MemAccess> mem_reads;
    std::vector<MemAccess> mem_writes;
};

using StepResult = std::variant<NormalStep, Exception>;

class Executor {
public:
    static std::optional<Commit> step(Core &core, Memory &mem, const DecodedInsn &insn);

    static StepResult execute(const Core &core, const Memory &mem, const LuiInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const AuipcInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const JalInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const JalrInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const BranchInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const LoadInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const StoreInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const OpImmInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const OpInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const MiscMemInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const SystemInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const OpImm32Insn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const Op32Insn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const LoadFpInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const StoreFpInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const OpFpInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const FmaFpInsn& insn);

    static StepResult execute(const Core &core, const Memory &mem, const InvalidInsn& insn);
};
