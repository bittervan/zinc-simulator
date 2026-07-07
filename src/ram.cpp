#include <ram.h>
#include <algorithm>
#include <cassert>

Memory::Memory(std::uint64_t base, uint64_t size) : base(base) {
    data.resize(size);
}

void Memory::load(std::uint64_t addr, const std::vector<std::uint8_t> &data) {
    auto offset = addr - base;
    std::copy(data.begin(), data.end(), this->data.begin() + offset);
}

void Memory::clear(std::uint64_t addr, std::uint64_t size) {
    auto offset = addr - base;
    std::fill(data.begin() + offset, data.begin() + offset + size, 0);
}

std::uint8_t Memory::get_8(std::uint64_t addr) {
    uint64_t offset = addr - this->base;
    return this->data[offset];
}

std::uint16_t Memory::get_16(std::uint64_t addr) {
    assert(!(addr % 2));
    uint64_t offset = addr - this->base;
    return reinterpret_cast<std::uint16_t*>(this->data.data())[offset / 2];
}

std::uint32_t Memory::get_32(std::uint64_t addr) {
    assert(!(addr % 4));
    uint64_t offset = addr - this->base;
    return reinterpret_cast<std::uint32_t*>(this->data.data())[offset / 4];
}

std::uint64_t Memory::get_64(std::uint64_t addr) {
    assert(!(addr % 8));
    uint64_t offset = addr - this->base;
    return reinterpret_cast<std::uint64_t*>(this->data.data())[offset / 8];
}

void Memory::set_8(std::uint64_t addr, std::uint8_t data) {
    uint64_t offset = addr - this->base;
    this->data[offset] = data;
}

void Memory::set_16(std::uint64_t addr, std::uint16_t data) {
    assert(!(addr % 2));
    uint64_t offset = addr - this->base;
    reinterpret_cast<uint16_t*>(this->data.data())[offset / 2] = data;
}

void Memory::set_32(std::uint64_t addr, std::uint32_t data) {
    assert(!(addr % 4));
    uint64_t offset = addr - this->base;
    reinterpret_cast<uint32_t*>(this->data.data())[offset / 4] = data;
}

void Memory::set_64(std::uint64_t addr, std::uint64_t data) {
    assert(!(addr % 8));
    uint64_t offset = addr - this->base;
    reinterpret_cast<uint64_t*>(this->data.data())[offset / 8] = data;
}