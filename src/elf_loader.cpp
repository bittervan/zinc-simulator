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
    
    // Get the to_host position to end our test.
    for (const auto& section : reader.sections) {
        if (section->get_type() != ELFIO::SHT_SYMTAB && section->get_type() != ELFIO::SHT_DYNSYM) {
            continue;
        }

        ELFIO::const_symbol_section_accessor symbols(reader, section.get());

        ELFIO::Elf64_Addr value = 0;
        ELFIO::Elf_Xword size = 0;
        unsigned char bind = 0;
        unsigned char type = 0;
        ELFIO::Elf_Half section_index = 0;
        unsigned char other = 0;

        if (symbols.get_symbol("tohost", value, size, bind, type, section_index, other)) {
            image.tohost = static_cast<std::uint64_t>(value);
            break;
        }
    }

    if (!image.tohost) throw std::runtime_error("tohost symbol is not defined");

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