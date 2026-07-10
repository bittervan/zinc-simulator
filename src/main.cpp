#define MEM_BASE 0x80000000
#define MEM_SIZE (128 * 1024 * 1024)

#include <iostream>
// #include <string>
#include <filesystem>
#include <decode.h>
#include <elf_loader.h>
#include <ram.h>
#include <cpu.h>
#include <execute.h>
#include <format>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: zinc-sim <elf>" << std::endl;
        return 1;
    }

    std::filesystem::path elf_path(argv[1]);
    ElfImage image = ElfLoader::load(elf_path);

    Memory mem(MEM_BASE, MEM_SIZE);

    for (const auto &seg : image.segments) {
        mem.load(seg.addr, seg.data);
        mem.clear(seg.addr + seg.data.size(), seg.size - seg.data.size());
    }

    Core core(image.entry);

    while (true) {
        std::uint32_t insn = mem.get_32(core.get_pc());

        DecodedInsn decoded = Decoder::decode(insn);

        if (std::holds_alternative<InvalidInsn>(decoded)) {
            throw std::runtime_error(
                std::format("Invalid instruction at {:016x}: {:08x}", core.get_pc(), insn)
            );
        }

        if (const SystemInsn *sys_insn = std::get_if<SystemInsn>(&decoded)) {
            if (sys_insn->type == SystemInsnType::Ecall) {
                break;
            }
        }

        Commit step_commit = Executor::step(core, mem, decoded);

        std::cout << step_commit.to_string() << std::endl;
    }

    return 0;
}