#include <elf_loader.h>
#include <elfio/elfio.hpp>
#include <memory>
#include <stdexcept>


ElfImage ElfLoader::load(const std::filesystem::path &elf_path) {
    ELFIO::elfio reader;
    if (!reader.load(elf_path.string())) {
        throw std::runtime_error("Failed to load ELF");
    }

    ElfImage image;
    image.entry = reader.get_entry();

    for (const std::unique_ptr<ELFIO::segment> &segment: reader.segments) {
        if (segment->get_type() != ELFIO::PT_LOAD) {
            continue;
        }

        const auto filesz = segment->get_file_size();
        const uint8_t *data = reinterpret_cast<const uint8_t*>(segment->get_data());

        image.segments.emplace_back(
            ElfSegment {
                .addr = segment->get_virtual_address(),
                .size = segment->get_memory_size(),
                .data = std::vector<std::uint8_t>(data, data + filesz),
            }
        );
    }

    return image;
}