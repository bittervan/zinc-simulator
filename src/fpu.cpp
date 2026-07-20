#include <fpu.h>
#include <stdexcept>

UnpackedFp32::UnpackedFp32(std::uint32_t raw)
    : sign(((raw >> 31) & 1U) != 0),
    exponent(0),
    significand(0),
    fp_class(FpClass::Zero) {

    const std::uint32_t raw_exponent =
        (raw >> 23) & 0xffU;

    const std::uint32_t fraction =
        raw & 0x7f'ffffU;

    // Infinity / NaN
    if (raw_exponent == 0xffU) {
        if (fraction == 0) {
            fp_class = FpClass::Infinity;
            return;
        }

        significand = fraction;

        const bool quiet =
            (fraction & (1U << 22)) != 0;

        fp_class = quiet
            ? FpClass::QuietNaN
            : FpClass::SignalingNaN;

        return;
    }

    // Zero / subnormal
    if (raw_exponent == 0) {
        exponent = -126;

        if (fraction == 0) {
            fp_class = FpClass::Zero;
            return;
        }

        significand = fraction;

        // Normalize the subnormal significand.
        while ((significand & (1U << 23)) == 0) {
            significand <<= 1;
            --exponent;
        }

        fp_class = FpClass::Subnormal;
        return;
    }

    // Normal
    exponent =
        static_cast<std::int32_t>(raw_exponent) - 127;

    significand =
        (1U << 23) | fraction;

    fp_class = FpClass::Normal;
}

std::uint32_t UnpackedFp32::to_bits() const {
    constexpr std::uint32_t SIGN_MASK = 1U << 31;
    constexpr std::uint32_t EXPONENT_MASK = 0xffU << 23;
    constexpr std::uint32_t FRACTION_MASK = 0x7f'ffffU;
    constexpr std::uint32_t QUIET_NAN_MASK = 1U << 22;

    const std::uint32_t sign_bits =
        sign ? SIGN_MASK : 0;

    switch (fp_class) {
        case FpClass::Zero:
            return sign_bits;

        case FpClass::Infinity:
            return sign_bits | EXPONENT_MASK;

        case FpClass::QuietNaN: {
            const std::uint32_t fraction =
                (significand & FRACTION_MASK) |
                QUIET_NAN_MASK;

            return sign_bits |
                    EXPONENT_MASK |
                    fraction;
        }

        case FpClass::SignalingNaN: {
            std::uint32_t fraction =
                (significand & FRACTION_MASK) &
                ~QUIET_NAN_MASK;

            // fraction=0 would encode Infinity.
            if (fraction == 0) {
                fraction = 1;
            }

            return sign_bits |
                    EXPONENT_MASK |
                    fraction;
        }

        case FpClass::Normal: {
            const std::uint32_t raw_exponent =
                static_cast<std::uint32_t>(
                    exponent + 127
                );

            const std::uint32_t fraction =
                significand & FRACTION_MASK;

            return sign_bits |
                    (raw_exponent << 23) |
                    fraction;
        }

        case FpClass::Subnormal: {
            // unpack 时从 -126 开始左移并递减 exponent。
            const unsigned shift =
                static_cast<unsigned>(
                    -126 - exponent
                );

            const std::uint32_t fraction =
                significand >> shift;

            return sign_bits |
                    (fraction & FRACTION_MASK);
        }
    }

    throw std::logic_error(
        "invalid FP32 classification"
    );
}


FpResult Fpu::binary(OpFpType op, std::uint32_t lhs, std::uint32_t rhs, RoundingMode rm) {

}

FpResult Fpu::unary(OpFpType op, std::uint32_t operand, RoundingMode rm) {

}