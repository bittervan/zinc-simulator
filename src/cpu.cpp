#define NUM_GPRS 32

#include <cpu.h>
#include <decode.h>
#include <execute.h>
#include <format>

RegWrite::RegWrite(RegType type, std::uint32_t num, std::uint64_t value) : type(type), num(num), value(value) {}

RegWrite::RegWrite(Csr csr, std::uint64_t value) : type(RegType::Csr), num(static_cast<std::uint32_t>(csr)), value(value) {}

MemAccess::MemAccess(std::uint64_t addr, std::uint64_t value, std::uint32_t size) : addr(addr), value(value), size(size) {}

Core::Core(std::uint64_t init_pc) : pc(init_pc), gprs(NUM_GPRS, 0) {}

std::uint64_t Core::get_pc() const {
    return this->pc;
}

void Core::set_pc(std::uint64_t new_pc) {
    this->pc = new_pc;
}

std::uint64_t Core::get_gpr(std::uint32_t index) const {
    return this->gprs[index];
}

void Core::set_gpr(std::uint32_t index, std::uint64_t value) {
    if (!index) return;
    this->gprs[index] = value;
}

std::uint64_t Core::get_csr(Csr csr) const {
    return this->csrs[static_cast<std::uint32_t>(csr)];
}

void Core::set_csr(Csr csr, std::uint64_t value) {
    this->csrs[static_cast<std::uint32_t>(csr)] = value;
}

Privilege Core::get_priv() const {
    return this->priv;
}

void Core::set_priv(Privilege priv) {
    this->priv = priv;
}

std::string reg_write_type_to_string(RegType type) {
    std::string ret;
    switch (type) {
        case RegType::Csr : {
            ret = "c";
            break;
        }
        case RegType::F : {
            ret = "f";
            break;
        }
        case RegType::X : {
            ret = "x";
            break;
        }
    }
    return ret;
}

std::string Commit::to_string() const {


    std::string out = std::format(
        "{{\"pc\":\"0x{:016x}\","
        "\"insn\":\"0x{:08x}\","
        "\"priv\":{},",
        pc,
        insn,
        static_cast<std::uint32_t>(priv)
    );

    out += "\"reg_writes\":[";

    for (std::size_t i = 0; i < reg_writes.size(); ++i) {
        const auto& w = reg_writes[i];
        if (i != 0) {
            out += ",";
        }

        out += std::format(
            "{{\"type\":\"{}\",\"num\":{},\"value\":{}}}",
            reg_write_type_to_string(w.type),
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