#include <iostream>
// #include <string>
#include <filesystem>
#include <decode.h>
#include <elf_loader.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: zinc-sim <elf>" << std::endl;
        return 1;
    }

    std::filesystem::path elf_path(argv[1]);

    ElfLoader loader(elf_path);

    return 0;
}