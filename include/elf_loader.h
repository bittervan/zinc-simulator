#pragma once
#include <string>
#include <filesystem>

class ElfLoader {

public:
    ElfLoader(const std::filesystem::path &elf_path);

};