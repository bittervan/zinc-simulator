#define NUM_GPRS 32

#include <cpu.h>
#include <decode.h>
#include <execute.h>
#include <format>

Core::Core(std::uint64_t init_pc) : pc(init_pc), gprs(NUM_GPRS, 0) {}

std::uint64_t Core::get_pc() const {
    return this->pc;
}

void Core::set_pc(std::uint64_t new_pc) {
    this->pc = new_pc;
}

std::uint64_t Core::get_gpr(uint32_t index) const {
    return this->gprs[index];
}

void Core::set_gpr(uint32_t index, uint64_t value) {
    if (!index) return;
    this->gprs[index] = value;
}

std::uint64_t Core::get_csr(uint32_t csr) const {
    return this->csrs[csr];
}

void Core::set_csr(uint32_t csr, uint64_t value) {
    this->csrs[csr] = value;
}


std::string Commit::to_string() const {

    std::string out = std::format(
        "{{\"pc\":\"0x{:016x}\",\"insn\":\"0x{:08x}\",",
        pc,
        insn
    );

    out += "\"reg_writes\":[";

    for (std::size_t i = 0; i < reg_writes.size(); ++i) {
        const auto& w = reg_writes[i];
        if (i != 0) {
            out += ",";
        }

        out += std::format(
            "{{\"type\":\"{}\",\"num\":{},\"value\":{}}}",
            w.type,
            w.num,
            w.value
        );
    }
    out += "],";

    out += "\"mem_reads\":[";
    for (std::size_t i = 0; i < mem_reads.size(); ++i) {
        const auto& r = mem_reads[i];
        if (i != 0) {
            out += ",";
        }

        out += std::format(
            "{{\"addr\":{},\"value\":{},\"size\":{}}}",
            r.addr,
            r.value,
            r.size
        );
    }
    out += "],";

    out += "\"mem_writes\":[";
    for (std::size_t i = 0; i < mem_writes.size(); ++i) {
        const auto& w = mem_writes[i];
        if (i != 0) {
            out += ",";
        }

        out += std::format(
            "{{\"addr\":{},\"value\":{},\"size\":{}}}",
            w.addr,
            w.value,
            w.size
        );
    }
    out += "]}";

    return out;
}