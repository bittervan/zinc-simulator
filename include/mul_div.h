#pragma once

#include <cstdint>

#include <decode.h>

class MulDivUnit {
public:
    static std::uint64_t execute(
        OpType op,
        std::uint64_t lhs,
        std::uint64_t rhs
    );

    static std::uint64_t execute_word(
        OpType op,
        std::uint64_t lhs,
        std::uint64_t rhs
    );
};
