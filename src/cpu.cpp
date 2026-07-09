#define NUM_GPRS 32

#include <cpu.h>
#include <decode.h>
#include <execute.h>

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

