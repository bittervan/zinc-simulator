#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

class ElfSegment {
public:
    std::uint64_t addr;
    std::uint64_t size;
    std::vector<std::uint8_t> data;
};

class ElfImage {
public:
    std::uint64_t entry;
    std::vector<ElfSegment> segments;
};

class ElfLoader {
public:
    static ElfImage load(const std::filesystem::path &elf_path);
};