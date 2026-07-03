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
    ElfImage image = ElfLoader::load(elf_path);

    for (auto seg : image.segments) {
        std::cout << std::hex << seg.addr << std::endl;
    }

    std::cout << "entry: " << image.entry << std::endl;

    return 0;
}