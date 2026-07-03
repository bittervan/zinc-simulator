#include <ram.h>
#include <algorithm>

Memory::Memory(std::uint64_t base, uint64_t size) : base(base) {
    data.resize(size);
}

void Memory::load(std::uint64_t addr, const std::vector<std::uint8_t> &data) {
    auto offset = addr - base;
    std::copy(data.begin(), data.end(), this->data.begin() + offset);
}

void Memory::clear(std::uint64_t addr, std::uint64_t size) {
    auto offset = addr - base;
    std::fill(data.begin() + offset, data.begin() + size, 0);
}