#define NUM_GPRS 32
#define NUM_FPRS 32

#include <cpu.h>
#include <decode.h>
#include <execute.h>
#include <format>

RegWrite::RegWrite(RegType type, std::uint32_t num, std::uint64_t value) : type(type), num(num), value(value) {}

// RegWrite::RegWrite(Csr csr, std::uint64_t value) : type(RegType::Csr), num(static_cast<std::uint32_t>(csr)), value(value) {}

MemAccess::MemAccess(std::uint64_t addr, std::uint64_t value, std::uint32_t size) : addr(addr), value(value), size(size) {}

Core::Core(std::uint64_t init_pc) : pc(init_pc), gprs(NUM_GPRS, 0), fprs(NUM_FPRS, 0) {
    this->csrs[static_cast<std::uint32_t>(Csr::Mstatus)] = MSTATUS_UXL_RV64;
}

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

std::uint64_t Core::get_fpr(std::uint32_t index) const {
    return this->fprs[index];
}

void Core::set_fpr(std::uint32_t index, std::uint32_t value) {
    this->fprs[index] = value;
}

std::uint64_t Core::get_csr(Csr csr) const {
    switch (csr) {
        case Csr::Fflags: {
            return this->csrs[static_cast<std::uint32_t>(Csr::Fcsr)] & 0x1f;
        }
        case Csr::Frm: {
            return this->csrs[static_cast<std::uint32_t>(Csr::Fcsr)] >> 5;
        }
        default: {
            return this->csrs[static_cast<std::uint32_t>(csr)];
        }
    }
}

std::uint64_t Core::set_csr(Csr csr, std::uint64_t value) {
    const std::uint32_t index = static_cast<std::uint32_t>(csr);
    std::uint64_t ret;

      switch (csr) {
        case Csr::Fflags: {
            std::uint64_t fcsr = csrs[static_cast<std::uint64_t>(Csr::Fcsr)];
            fcsr = (fcsr & ~0x1fULL) | (value & 0x1fULL);
            ret = fcsr & 0x1fULL;
            csrs[static_cast<std::uint64_t>(Csr::Fcsr)] = fcsr;
            break;
        }
        case Csr::Frm: {
            std::uint64_t fcsr = csrs[static_cast<std::uint64_t>(Csr::Fcsr)];
            fcsr = (fcsr & ~0xe0ULL) | ((value & 0x7ULL) << 5);
            ret = (fcsr >> 5) & 0x7ULL;
            csrs[static_cast<std::uint64_t>(Csr::Fcsr)] = fcsr;
            break;
        }
        case Csr::Fcsr: {
            ret = csrs[index] = value & 0xffUL;
            break;
        }
        case Csr::Mstatus: {
            std::uint64_t new_value = (csrs[index] & ~MSTATUS_WRITABLE_MASK) | (value & MSTATUS_WRITABLE_MASK);

            new_value &= ~MSTATUS_SD_MASK;

            if ((new_value & MSTATUS_FS_MASK) == MSTATUS_FS_DIRTY) {
                new_value |= MSTATUS_SD_MASK;
            }

            ret = csrs[index] = new_value;
            break;
        }

        case Csr::Mepc: {
            ret = csrs[index] = value & ~0b11ULL;
            break;
        }

        case Csr::Mtvec: {
            ret = csrs[index] = value & ~0b11ULL;
            break;
        }

        case Csr::Pmpcfg0:
        case Csr::Pmpaddr0:
        case Csr::Mie:
        case Csr::Mscratch:
        case Csr::Mcause:
        case Csr::Mtval:
            ret = csrs[index] = value;
            break;

        case Csr::Mhartid: {
            throw std::logic_error(
                "attempted to write read-only mhartid"
            );
        }

        default: {
            throw std::logic_error(
                std::format(
                    "attempted to write unsupported CSR 0x{:03x}",
                    index
                )
            );
        }
    }

    return ret;
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
            // r.value, // TODO: fix this
            0,
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

void Core::take_exception(const Exception& exception) {
    std::uint64_t mcause = exception.mcause;
    std::uint64_t mtval = exception.mtval;

    std::uint64_t mstatus = this->get_csr(Csr::Mstatus);
    if ((mstatus & MSTATUS_MIE_MASK) != 0) {
        mstatus |= MSTATUS_MPIE_MASK;
    } else {
        mstatus &= ~MSTATUS_MPIE_MASK;
    }

    if ((mstatus & MSTATUS_FS_MASK) == MSTATUS_FS_DIRTY) {
        mstatus |= MSTATUS_SD_MASK;
    } else {
        mstatus &= ~MSTATUS_SD_MASK;
    }

    mstatus &= ~MSTATUS_MIE_MASK;
    mstatus &= ~MSTATUS_MPP_MASK;
    mstatus |= static_cast<std::uint64_t>(this->get_priv()) << 11;

    this->set_csr(Csr::Mcause, mcause);
    this->set_csr(Csr::Mtval, mtval);
    this->set_csr(Csr::Mepc, this->get_pc());
    this->set_csr(Csr::Mstatus, mstatus);

    this->set_pc(this->get_csr(Csr::Mtvec) & ~0b11UL);
    this->set_priv(Privilege::Machine);
}

bool Core::has_csr(std::uint32_t csr) const {
    switch (static_cast<Csr>(csr)) {
        case Csr::Fcsr:
        case Csr::Fflags:
        case Csr::Frm:
        case Csr::Mstatus:
        case Csr::Mie:
        case Csr::Mtvec:
        case Csr::Mscratch:
        case Csr::Mepc:
        case Csr::Mcause:
        case Csr::Mtval:
        case Csr::Pmpcfg0:
        case Csr::Pmpaddr0:
        case Csr::Mhartid:
            return true;

        default:
            return false;
    }
}