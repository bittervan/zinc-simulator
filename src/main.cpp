#include "zinc/sim/binary_loader.hpp"
#include "zinc/sim/memory.hpp"
#include "zinc/sim/rv64_core.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t kDefaultLoadAddress = 0x80000000;
constexpr std::uint64_t kDefaultEntryPoint = 0x80000000;
constexpr std::uint64_t kDefaultMemorySize = 64 * 1024;
constexpr int kDefaultMaxSteps = 16;

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program << " [--max-steps N] [--trace-json] <binary>\n";
}

int parse_int(std::string_view text) {
    int value = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || ptr != end || value < 0) {
        throw std::runtime_error("invalid integer: " + std::string(text));
    }
    return value;
}

void print_hex64(std::uint64_t value) {
    std::cout << "0x" << std::hex << std::setw(16) << std::setfill('0') << value << std::dec
              << std::setfill(' ');
}

void print_hex32(std::uint32_t value) {
    std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec
              << std::setfill(' ');
}

void print_trace(const zinc::sim::CommitTrace& trace) {
    std::cout << "core   0: ";
    print_hex64(trace.pc);
    std::cout << " (";
    print_hex32(trace.instruction);
    std::cout << ") " << trace.disassembly << '\n';

    std::cout << "core   0: 3 ";
    print_hex64(trace.pc);
    std::cout << " (";
    print_hex32(trace.instruction);
    std::cout << ")";
    if (trace.write.has_value()) {
        std::cout << " x" << static_cast<int>(trace.write->reg) << ' ';
        print_hex64(trace.write->value);
    }
    std::cout << '\n';
}

std::string hex64_string(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

std::string hex32_string(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

std::string json_escape(std::string_view value) {
    std::string escaped;
    for (const char ch : value) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

void print_json_trace(std::span<const zinc::sim::CommitTrace> commits) {
    std::cout << "{\n";
    std::cout << "  \"source\": \"zinc\",\n";
    std::cout << "  \"isa\": \"RV64I\",\n";
    std::cout << "  \"load_address\": \"" << hex64_string(kDefaultLoadAddress) << "\",\n";
    std::cout << "  \"entry\": \"" << hex64_string(kDefaultEntryPoint) << "\",\n";
    std::cout << "  \"commits\": [\n";

    for (std::size_t i = 0; i < commits.size(); ++i) {
        const auto& commit = commits[i];
        std::cout << "    {\n";
        std::cout << "      \"priv\": 3,\n";
        std::cout << "      \"pc\": \"" << hex64_string(commit.pc) << "\",\n";
        std::cout << "      \"instruction\": \"" << hex32_string(commit.instruction) << "\",\n";
        std::cout << "      \"disassembly\": \"" << json_escape(commit.disassembly) << "\",\n";
        std::cout << "      \"reg_write\": ";
        if (commit.write.has_value()) {
            std::cout << "{\"reg\": " << static_cast<int>(commit.write->reg) << ", \"value\": \""
                      << hex64_string(commit.write->value) << "\"}";
        } else {
            std::cout << "null";
        }
        std::cout << ",\n";
        std::cout << "      \"memory_writes\": [";
        for (std::size_t write_index = 0; write_index < commit.memory_writes.size(); ++write_index) {
            const auto& write = commit.memory_writes[write_index];
            if (write_index != 0) {
                std::cout << ", ";
            }
            std::cout << "{\"address\": \"" << hex64_string(write.address) << "\", \"size\": "
                      << static_cast<int>(write.size) << ", \"value\": \"" << hex64_string(write.value)
                      << "\"}";
        }
        std::cout << "]\n";
        std::cout << "    }";
        if (i + 1 < commits.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }

    std::cout << "  ]\n";
    std::cout << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    int max_steps = kDefaultMaxSteps;
    bool trace_json = false;
    std::string_view binary_path;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--max-steps") {
            if (i + 1 >= argc) {
                print_usage(argc > 0 ? argv[0] : "zinc-simulator");
                return 2;
            }
            max_steps = parse_int(argv[++i]);
        } else if (arg == "--trace-json") {
            trace_json = true;
        } else if (binary_path.empty()) {
            binary_path = arg;
        } else {
            print_usage(argc > 0 ? argv[0] : "zinc-simulator");
            return 2;
        }
    }

    if (binary_path.empty()) {
        print_usage(argc > 0 ? argv[0] : "zinc-simulator");
        return 2;
    }

    try {
        const auto binary = zinc::sim::load_binary(binary_path);
        zinc::sim::Memory memory(kDefaultLoadAddress, kDefaultMemorySize);
        memory.load(kDefaultLoadAddress, std::span<const std::uint8_t>(binary.data(), binary.size()));

        zinc::sim::Rv64Core core(memory, kDefaultEntryPoint);
        std::vector<zinc::sim::CommitTrace> commits;
        commits.reserve(static_cast<std::size_t>(max_steps));
        for (int step = 0; step < max_steps; ++step) {
            commits.push_back(core.step());
        }

        if (trace_json) {
            print_json_trace(commits);
            return 0;
        }

        std::cout << "loaded " << binary.size() << " bytes"
                  << " at 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << kDefaultLoadAddress << std::dec << std::setfill(' ') << '\n';
        std::cout << "entry  0x" << std::hex << std::setw(8) << std::setfill('0')
                  << kDefaultEntryPoint << std::dec << std::setfill(' ') << '\n';

        for (const auto& commit : commits) {
            print_trace(commit);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
