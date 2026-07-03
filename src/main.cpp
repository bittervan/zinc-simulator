#define MEM_BASE 0x80000000
#define MEM_SIZE (128 * 1024 * 1024)

#include <iostream>
// #include <string>
#include <filesystem>
#include <decode.h>
#include <elf_loader.h>
#include <ram.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: zinc-sim <elf>" << std::endl;
        return 1;
    }

    std::filesystem::path elf_path(argv[1]);
    ElfImage image = ElfLoader::load(elf_path);

    std::cout << "entry: " << image.entry << std::endl;

    Memory mem(MEM_BASE, MEM_SIZE);

    for (const auto &seg : image.segments) {
        mem.load(seg.addr, seg.data);
        mem.clear(seg.addr, seg.size - seg.data.size());
    }

    return 0;
}