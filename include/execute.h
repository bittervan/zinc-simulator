#pragma once

#include <decode.h>
#include <cpu.h>
#include <memory.h>

struct StepResult {
    std::uint64_t next_pc;
    std::vector<RegWrite> reg_writes;
    std::vector<MemAccess> mem_reads;
    std::vector<MemAccess> mem_writes;
};

class Executor {
public:
    static Commit step(Core &core, Memory &mem, const DecodedInsn &insn);

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

    static StepResult execute(const Core &core, const Memory &mem, const InvalidInsn& insn);

};