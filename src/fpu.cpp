#include <fpu.h>
#include <stdexcept>

namespace {

constexpr std::uint32_t FP32_SIGN_MASK = 1U << 31;
constexpr std::uint32_t FP32_MAGNITUDE_MASK = ~FP32_SIGN_MASK;
constexpr std::uint32_t FP32_INFINITY = 0x7f80'0000U;
constexpr std::uint32_t FP32_MAX_FINITE = 0x7f7f'ffffU;
constexpr std::uint32_t FP32_CANONICAL_NAN = 0x7fc0'0000U;

constexpr std::uint32_t FP_FLAG_NX = 1U << 0;
constexpr std::uint32_t FP_FLAG_UF = 1U << 1;
constexpr std::uint32_t FP_FLAG_OF = 1U << 2;
constexpr std::uint32_t FP_FLAG_DZ = 1U << 3;
constexpr std::uint32_t FP_FLAG_NV = 1U << 4;

constexpr std::uint64_t FP32_HIDDEN_BIT = 1ULL << 23;
constexpr std::uint64_t FP32_EXTENDED_HIDDEN_BIT = 1ULL << 26;

bool is_nan(FpClass fp_class) {
    return fp_class == FpClass::SignalingNaN ||
           fp_class == FpClass::QuietNaN;
}

bool is_signaling_nan(FpClass fp_class) {
    return fp_class == FpClass::SignalingNaN;
}

std::uint64_t shift_right_jam(std::uint64_t value, unsigned distance) {
    if (distance == 0) {
        return value;
    }

    if (distance < 64) {
        const std::uint64_t discarded_mask =
            (std::uint64_t{1} << distance) - 1;
        const bool discarded = (value & discarded_mask) != 0;
        return (value >> distance) | static_cast<std::uint64_t>(discarded);
    }

    return value != 0 ? 1 : 0;
}

bool should_round_up(
    RoundingMode rm,
    bool sign,
    std::uint64_t rounded_significand,
    std::uint32_t round_bits
) {
    switch (rm) {
        case RoundingMode::Rne:
            return round_bits > 0b100 ||
                   (round_bits == 0b100 && (rounded_significand & 1) != 0);
        case RoundingMode::Rtz:
            return false;
        case RoundingMode::Rdn:
            return sign && round_bits != 0;
        case RoundingMode::Rup:
            return !sign && round_bits != 0;
        case RoundingMode::Rmm:
            return round_bits >= 0b100;
        case RoundingMode::Dynamic:
            throw std::logic_error("dynamic rounding mode was not resolved");
    }

    throw std::logic_error("invalid floating-point rounding mode");
}

FpResult round_and_pack(
    bool sign,
    std::int32_t exponent,
    std::uint64_t significand,
    RoundingMode rm
) {
    const std::uint32_t sign_bits = sign ? FP32_SIGN_MASK : 0;

    if (significand == 0) {
        return FpResult{.value = sign_bits, .flags = 0};
    }

    while ((significand & (FP32_EXTENDED_HIDDEN_BIT << 1)) != 0) {
        significand = shift_right_jam(significand, 1);
        ++exponent;
    }

    while ((significand & FP32_EXTENDED_HIDDEN_BIT) == 0) {
        significand <<= 1;
        --exponent;
    }

    if (exponent < -126) {
        const auto distance = static_cast<unsigned>(-126 - exponent);
        significand = shift_right_jam(significand, distance);
        exponent = -126;
    }

    const std::uint32_t round_bits =
        static_cast<std::uint32_t>(significand & 0b111);
    const bool inexact = round_bits != 0;
    std::uint64_t rounded_significand = significand >> 3;

    if (should_round_up(rm, sign, rounded_significand, round_bits)) {
        ++rounded_significand;
    }

    if (rounded_significand >= (FP32_HIDDEN_BIT << 1)) {
        rounded_significand >>= 1;
        ++exponent;
    }

    if (exponent > 127) {
        const bool round_to_infinity =
            rm == RoundingMode::Rne ||
            rm == RoundingMode::Rmm ||
            (rm == RoundingMode::Rup && !sign) ||
            (rm == RoundingMode::Rdn && sign);

        return FpResult{
            .value = sign_bits |
                (round_to_infinity ? FP32_INFINITY : FP32_MAX_FINITE),
            .flags = FP_FLAG_OF | FP_FLAG_NX,
        };
    }

    const bool subnormal =
        exponent == -126 && rounded_significand < FP32_HIDDEN_BIT;

    std::uint32_t flags = inexact ? FP_FLAG_NX : 0;
    if (subnormal && inexact) {
        flags |= FP_FLAG_UF;
    }

    if (rounded_significand == 0) {
        return FpResult{.value = sign_bits, .flags = flags};
    }

    const std::uint32_t fraction =
        static_cast<std::uint32_t>(rounded_significand) & 0x7f'ffffU;
    const std::uint32_t raw_exponent = subnormal
        ? 0
        : static_cast<std::uint32_t>(exponent + 127);

    return FpResult{
        .value = sign_bits | (raw_exponent << 23) | fraction,
        .flags = flags,
    };
}

FpResult propagate_nan(const UnpackedFp32& lhs, const UnpackedFp32& rhs) {
    const bool invalid =
        is_signaling_nan(lhs.classification()) ||
        is_signaling_nan(rhs.classification());

    return FpResult{
        .value = FP32_CANONICAL_NAN,
        .flags = invalid ? FP_FLAG_NV : 0,
    };
}

FpResult add_or_sub(
    std::uint32_t lhs_raw,
    std::uint32_t rhs_raw,
    bool subtract,
    RoundingMode rm
) {
    const UnpackedFp32 lhs{lhs_raw};
    const UnpackedFp32 rhs{rhs_raw};
    const bool rhs_sign = rhs.sign_bit() ^ subtract;

    if (is_nan(lhs.classification()) || is_nan(rhs.classification())) {
        return propagate_nan(lhs, rhs);
    }

    if (lhs.classification() == FpClass::Infinity ||
        rhs.classification() == FpClass::Infinity) {
        if (lhs.classification() == FpClass::Infinity &&
            rhs.classification() == FpClass::Infinity &&
            lhs.sign_bit() != rhs_sign) {
            return FpResult{
                .value = FP32_CANONICAL_NAN,
                .flags = FP_FLAG_NV,
            };
        }

        const bool result_sign = lhs.classification() == FpClass::Infinity
            ? lhs.sign_bit()
            : rhs_sign;
        return FpResult{
            .value = (result_sign ? FP32_SIGN_MASK : 0) | FP32_INFINITY,
            .flags = 0,
        };
    }

    if (lhs.classification() == FpClass::Zero &&
        rhs.classification() == FpClass::Zero) {
        const bool result_sign = lhs.sign_bit() == rhs_sign
            ? lhs.sign_bit()
            : rm == RoundingMode::Rdn;
        return FpResult{
            .value = result_sign ? FP32_SIGN_MASK : 0,
            .flags = 0,
        };
    }

    if (lhs.classification() == FpClass::Zero) {
        return FpResult{
            .value = (rhs_raw & FP32_MAGNITUDE_MASK) |
                (rhs_sign ? FP32_SIGN_MASK : 0),
            .flags = 0,
        };
    }

    if (rhs.classification() == FpClass::Zero) {
        return FpResult{.value = lhs_raw, .flags = 0};
    }

    std::int32_t exponent = lhs.unbiased_exponent();
    std::uint64_t lhs_significand =
        static_cast<std::uint64_t>(lhs.normalized_significand()) << 3;
    std::uint64_t rhs_significand =
        static_cast<std::uint64_t>(rhs.normalized_significand()) << 3;

    if (lhs.unbiased_exponent() > rhs.unbiased_exponent()) {
        rhs_significand = shift_right_jam(
            rhs_significand,
            static_cast<unsigned>(lhs.unbiased_exponent() - rhs.unbiased_exponent())
        );
    } else if (rhs.unbiased_exponent() > lhs.unbiased_exponent()) {
        lhs_significand = shift_right_jam(
            lhs_significand,
            static_cast<unsigned>(rhs.unbiased_exponent() - lhs.unbiased_exponent())
        );
        exponent = rhs.unbiased_exponent();
    }

    if (lhs.sign_bit() == rhs_sign) {
        return round_and_pack(
            lhs.sign_bit(),
            exponent,
            lhs_significand + rhs_significand,
            rm
        );
    }

    if (lhs_significand == rhs_significand) {
        return FpResult{
            .value = rm == RoundingMode::Rdn ? FP32_SIGN_MASK : 0,
            .flags = 0,
        };
    }

    const bool lhs_has_larger_magnitude = lhs_significand > rhs_significand;
    const std::uint64_t result_significand = lhs_has_larger_magnitude
        ? lhs_significand - rhs_significand
        : rhs_significand - lhs_significand;
    const bool result_sign = lhs_has_larger_magnitude
        ? lhs.sign_bit()
        : rhs_sign;

    return round_and_pack(result_sign, exponent, result_significand, rm);
}

FpResult multiply(
    std::uint32_t lhs_raw,
    std::uint32_t rhs_raw,
    RoundingMode rm
) {
    const UnpackedFp32 lhs{lhs_raw};
    const UnpackedFp32 rhs{rhs_raw};
    const bool sign = lhs.sign_bit() ^ rhs.sign_bit();

    if (is_nan(lhs.classification()) || is_nan(rhs.classification())) {
        return propagate_nan(lhs, rhs);
    }

    const bool lhs_zero = lhs.classification() == FpClass::Zero;
    const bool rhs_zero = rhs.classification() == FpClass::Zero;
    const bool lhs_infinity = lhs.classification() == FpClass::Infinity;
    const bool rhs_infinity = rhs.classification() == FpClass::Infinity;

    if ((lhs_zero && rhs_infinity) || (lhs_infinity && rhs_zero)) {
        return FpResult{.value = FP32_CANONICAL_NAN, .flags = FP_FLAG_NV};
    }

    if (lhs_infinity || rhs_infinity) {
        return FpResult{
            .value = (sign ? FP32_SIGN_MASK : 0) | FP32_INFINITY,
            .flags = 0,
        };
    }

    if (lhs_zero || rhs_zero) {
        return FpResult{
            .value = sign ? FP32_SIGN_MASK : 0,
            .flags = 0,
        };
    }

    const std::uint64_t product =
        static_cast<std::uint64_t>(lhs.normalized_significand()) *
        rhs.normalized_significand();
    const bool product_at_least_two = (product & (1ULL << 47)) != 0;
    const unsigned shift = product_at_least_two ? 21 : 20;
    const std::int32_t exponent =
        lhs.unbiased_exponent() +
        rhs.unbiased_exponent() +
        static_cast<std::int32_t>(product_at_least_two);

    return round_and_pack(sign, exponent, shift_right_jam(product, shift), rm);
}

struct DivisionResult {
    std::uint64_t quotient;
    std::uint64_t remainder;
};

DivisionResult divide_unsigned(std::uint64_t dividend, std::uint64_t divisor) {
    DivisionResult result{};

    for (int bit = 63; bit >= 0; --bit) {
        result.remainder =
            (result.remainder << 1) | ((dividend >> bit) & 1);
        if (result.remainder >= divisor) {
            result.remainder -= divisor;
            result.quotient |= std::uint64_t{1} << bit;
        }
    }

    return result;
}

FpResult divide(
    std::uint32_t lhs_raw,
    std::uint32_t rhs_raw,
    RoundingMode rm
) {
    const UnpackedFp32 lhs{lhs_raw};
    const UnpackedFp32 rhs{rhs_raw};
    const bool sign = lhs.sign_bit() ^ rhs.sign_bit();
    const bool lhs_zero = lhs.classification() == FpClass::Zero;
    const bool rhs_zero = rhs.classification() == FpClass::Zero;
    const bool lhs_infinity = lhs.classification() == FpClass::Infinity;
    const bool rhs_infinity = rhs.classification() == FpClass::Infinity;

    if (is_nan(lhs.classification()) || is_nan(rhs.classification())) {
        return propagate_nan(lhs, rhs);
    }

    if ((lhs_zero && rhs_zero) || (lhs_infinity && rhs_infinity)) {
        return FpResult{.value = FP32_CANONICAL_NAN, .flags = FP_FLAG_NV};
    }

    if (rhs_zero) {
        return FpResult{
            .value = (sign ? FP32_SIGN_MASK : 0) | FP32_INFINITY,
            .flags = FP_FLAG_DZ,
        };
    }

    if (lhs_infinity) {
        return FpResult{
            .value = (sign ? FP32_SIGN_MASK : 0) | FP32_INFINITY,
            .flags = 0,
        };
    }

    if (lhs_zero || rhs_infinity) {
        return FpResult{.value = sign ? FP32_SIGN_MASK : 0, .flags = 0};
    }

    std::uint64_t numerator = lhs.normalized_significand();
    std::int32_t exponent =
        lhs.unbiased_exponent() - rhs.unbiased_exponent();
    if (numerator < rhs.normalized_significand()) {
        numerator <<= 1;
        --exponent;
    }

    const DivisionResult division = divide_unsigned(
        numerator << 26,
        rhs.normalized_significand()
    );
    std::uint64_t quotient = division.quotient;
    if (division.remainder != 0) {
        quotient |= 1;
    }

    return round_and_pack(sign, exponent, quotient, rm);
}

bool less_than(std::uint32_t lhs, std::uint32_t rhs) {
    const bool lhs_sign = (lhs & FP32_SIGN_MASK) != 0;
    const bool rhs_sign = (rhs & FP32_SIGN_MASK) != 0;
    if (lhs_sign != rhs_sign) {
        return lhs_sign;
    }

    const std::uint32_t lhs_magnitude = lhs & FP32_MAGNITUDE_MASK;
    const std::uint32_t rhs_magnitude = rhs & FP32_MAGNITUDE_MASK;
    return lhs_sign
        ? lhs_magnitude > rhs_magnitude
        : lhs_magnitude < rhs_magnitude;
}

FpResult min_or_max(std::uint32_t lhs_raw, std::uint32_t rhs_raw, bool maximum) {
    const UnpackedFp32 lhs{lhs_raw};
    const UnpackedFp32 rhs{rhs_raw};
    const bool lhs_nan = is_nan(lhs.classification());
    const bool rhs_nan = is_nan(rhs.classification());
    const std::uint32_t flags =
        is_signaling_nan(lhs.classification()) ||
        is_signaling_nan(rhs.classification())
            ? FP_FLAG_NV
            : 0;

    if (lhs_nan && rhs_nan) {
        return FpResult{.value = FP32_CANONICAL_NAN, .flags = flags};
    }
    if (lhs_nan) {
        return FpResult{.value = rhs_raw, .flags = flags};
    }
    if (rhs_nan) {
        return FpResult{.value = lhs_raw, .flags = flags};
    }

    const bool lhs_zero = lhs.classification() == FpClass::Zero;
    const bool rhs_zero = rhs.classification() == FpClass::Zero;
    if (lhs_zero && rhs_zero) {
        return FpResult{
            .value = maximum
                ? (lhs_raw & rhs_raw)
                : (lhs_raw | rhs_raw),
            .flags = flags,
        };
    }

    const bool lhs_less = less_than(lhs_raw, rhs_raw);
    const std::uint32_t value = maximum
        ? (lhs_less ? rhs_raw : lhs_raw)
        : (lhs_less ? lhs_raw : rhs_raw);
    return FpResult{.value = value, .flags = flags};
}

struct SquareRootResult {
    std::uint64_t root;
    std::uint64_t remainder;
};

SquareRootResult square_root_unsigned(std::uint64_t value) {
    std::uint64_t root = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return SquareRootResult{.root = root, .remainder = value};
}

FpResult square_root(std::uint32_t raw, RoundingMode rm) {
    const UnpackedFp32 operand{raw};

    if (is_nan(operand.classification())) {
        return FpResult{
            .value = FP32_CANONICAL_NAN,
            .flags = is_signaling_nan(operand.classification()) ? FP_FLAG_NV : 0,
        };
    }

    if (operand.classification() == FpClass::Zero) {
        return FpResult{.value = raw, .flags = 0};
    }

    if (operand.sign_bit()) {
        return FpResult{.value = FP32_CANONICAL_NAN, .flags = FP_FLAG_NV};
    }

    if (operand.classification() == FpClass::Infinity) {
        return FpResult{.value = FP32_INFINITY, .flags = 0};
    }

    std::uint64_t significand = operand.normalized_significand();
    std::int32_t exponent = operand.unbiased_exponent();
    if ((exponent & 1) != 0) {
        significand <<= 1;
        --exponent;
    }

    const SquareRootResult result = square_root_unsigned(significand << 29);
    std::uint64_t rounded_significand = result.root;
    if (result.remainder != 0) {
        rounded_significand |= 1;
    }

    return round_and_pack(false, exponent / 2, rounded_significand, rm);
}

} // namespace

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
    switch (op) {
        case OpFpType::Add:
            return add_or_sub(lhs, rhs, false, rm);
        case OpFpType::Sub:
            return add_or_sub(lhs, rhs, true, rm);
        case OpFpType::Mul:
            return multiply(lhs, rhs, rm);
        case OpFpType::Div:
            return divide(lhs, rhs, rm);
        case OpFpType::Min:
            return min_or_max(lhs, rhs, false);
        case OpFpType::Max:
            return min_or_max(lhs, rhs, true);
        case OpFpType::Sgnj:
            return FpResult{
                .value = (lhs & FP32_MAGNITUDE_MASK) |
                    (rhs & FP32_SIGN_MASK),
                .flags = 0,
            };
        case OpFpType::Sgnjn:
            return FpResult{
                .value = (lhs & FP32_MAGNITUDE_MASK) |
                    ((~rhs) & FP32_SIGN_MASK),
                .flags = 0,
            };
        case OpFpType::Sgnjx:
            return FpResult{
                .value = (lhs & FP32_MAGNITUDE_MASK) |
                    ((lhs ^ rhs) & FP32_SIGN_MASK),
                .flags = 0,
            };
        default:
            throw std::logic_error(
                "operation is not a binary FP32 operation"
            );
    }
}

FpResult Fpu::unary(OpFpType op, std::uint32_t operand, RoundingMode rm) {
    switch (op) {
        case OpFpType::Sqrt:
            return square_root(operand, rm);
        default:
            throw std::logic_error(
                "operation is not a unary FP32 operation"
            );
    }
}
