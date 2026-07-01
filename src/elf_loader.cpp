#include <elf_loader.h>
#include <fstream>
#include <iostream>

ElfLoader::ElfLoader(const std::filesystem::path &elf_path) {
    std::ifstream elf_file(elf_path, std::ios::binary | std::ios::ate);
    std::cout << elf_file.tellg() << std::endl;
}