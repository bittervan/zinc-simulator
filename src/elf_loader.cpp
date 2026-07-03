#include <elf_loader.h>
#include <fstream>
#include <ios>
#include <iostream>

ElfLoader::ElfLoader(const std::filesystem::path &elf_path) {
    std::ifstream elf_file(elf_path, std::ios::binary | std::ios::ate);
    const std::streamsize size = elf_file.tellg();
    file_bytes.resize(size);
    elf_file.seekg(0, std::ios::beg);
    elf_file.read(reinterpret_cast<char*>(file_bytes.data()), size);
}