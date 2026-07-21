#pragma once

#include <cstdint>
#include <decode.h>

struct FpResult {
    std::uint32_t value;
    std::uint32_t flags;
};

enum class FpClass {
    Zero,
    Subnormal,
    Normal,
    Infinity,
    SignalingNaN,
    QuietNaN,
};

class UnpackedFp32 {
    bool sign;
    std::int32_t exponent;
    std::uint32_t significand;
    FpClass fp_class;

public:
    explicit UnpackedFp32(std::uint32_t raw);

    [[nodiscard]] std::uint32_t to_bits() const;
    [[nodiscard]] bool sign_bit() const { return sign; }
    [[nodiscard]] std::int32_t unbiased_exponent() const { return exponent; }
    [[nodiscard]] std::uint32_t normalized_significand() const { return significand; }
    [[nodiscard]] FpClass classification() const { return fp_class; }
};

class Fpu {
public:

    static FpResult binary(OpFpType op, std::uint32_t lhs, std::uint32_t rhs, RoundingMode rm);

    static FpResult unary(OpFpType op, std::uint32_t operand, RoundingMode rm);

    // static FpResult fma(
    //     OpFma op,
    //     std::uint32_t lhs,
    //     std::uint32_t rhs,
    //     std::uint32_t addend,
    //     RoundingMode rm
    // );
};
