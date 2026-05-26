#include "zinc/sim/binary_loader.hpp"

#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

constexpr std::uint32_t kDefaultLoadAddress = 0x00000000;
constexpr std::uint32_t kDefaultEntryPoint = 0x00000000;

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program << " <binary>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argc > 0 ? argv[0] : "zinc-simulator");
        return 2;
    }

    try {
        const auto binary = zinc::sim::load_binary(argv[1]);

        std::cout << "loaded " << binary.size() << " bytes"
                  << " at 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << kDefaultLoadAddress << '\n';
        std::cout << "entry  0x" << std::hex << std::setw(8) << std::setfill('0')
                  << kDefaultEntryPoint << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
