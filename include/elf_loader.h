#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

class ElfLoader {

public:
    ElfLoader(const std::filesystem::path &elf_path);

private:
    std::vector<uint8_t> file_bytes;

};