#include <mul_div.h>

#include <bit>
#include <limits>
#include <stdexcept>

namespace {

std::uint64_t multiply_high_unsigned(
    std::uint64_t lhs,
    std::uint64_t rhs
) {
    const std::uint64_t lhs_low = static_cast<std::uint32_t>(lhs);
    const std::uint64_t lhs_high = lhs >> 32;
    const std::uint64_t rhs_low = static_cast<std::uint32_t>(rhs);
    const std::uint64_t rhs_high = rhs >> 32;

    const std::uint64_t low_low = lhs_low * rhs_low;
    const std::uint64_t low_high = lhs_low * rhs_high;
    const std::uint64_t high_low = lhs_high * rhs_low;
    const std::uint64_t high_high = lhs_high * rhs_high;
    const std::uint64_t carry =
        (low_low >> 32) +
        static_cast<std::uint32_t>(low_high) +
        static_cast<std::uint32_t>(high_low);

    return high_high +
        (low_high >> 32) +
        (high_low >> 32) +
        (carry >> 32);
}

std::uint64_t sign_extend_word(std::uint32_t value) {
    return (value & (1U << 31)) != 0
        ? 0xffff'ffff'0000'0000ULL | value
        : value;
}

} // namespace

std::uint64_t MulDivUnit::execute(
    OpType op,
    std::uint64_t lhs,
    std::uint64_t rhs
) {
    switch (op) {
        case OpType::Mul:
            return lhs * rhs;
        case OpType::Mulh: {
            std::uint64_t high = multiply_high_unsigned(lhs, rhs);
            if ((lhs & (1ULL << 63)) != 0) {
                high -= rhs;
            }
            if ((rhs & (1ULL << 63)) != 0) {
                high -= lhs;
            }
            return high;
        }
        case OpType::Mulhsu: {
            std::uint64_t high = multiply_high_unsigned(lhs, rhs);
            if ((lhs & (1ULL << 63)) != 0) {
                high -= rhs;
            }
            return high;
        }
        case OpType::Mulhu:
            return multiply_high_unsigned(lhs, rhs);
        case OpType::Div: {
            const std::int64_t signed_lhs = std::bit_cast<std::int64_t>(lhs);
            const std::int64_t signed_rhs = std::bit_cast<std::int64_t>(rhs);
            if (signed_rhs == 0) {
                return std::numeric_limits<std::uint64_t>::max();
            }
            if (
                signed_lhs == std::numeric_limits<std::int64_t>::min() &&
                signed_rhs == -1
            ) {
                return lhs;
            }
            return std::bit_cast<std::uint64_t>(signed_lhs / signed_rhs);
        }
        case OpType::Divu:
            return rhs == 0
                ? std::numeric_limits<std::uint64_t>::max()
                : lhs / rhs;
        case OpType::Rem: {
            const std::int64_t signed_lhs = std::bit_cast<std::int64_t>(lhs);
            const std::int64_t signed_rhs = std::bit_cast<std::int64_t>(rhs);
            if (signed_rhs == 0) {
                return lhs;
            }
            if (
                signed_lhs == std::numeric_limits<std::int64_t>::min() &&
                signed_rhs == -1
            ) {
                return 0;
            }
            return std::bit_cast<std::uint64_t>(signed_lhs % signed_rhs);
        }
        case OpType::Remu:
            return rhs == 0 ? lhs : lhs % rhs;
        default:
            throw std::logic_error("operation is not an RV64M instruction");
    }
}

std::uint64_t MulDivUnit::execute_word(
    OpType op,
    std::uint64_t lhs,
    std::uint64_t rhs
) {
    const std::uint32_t lhs_word = static_cast<std::uint32_t>(lhs);
    const std::uint32_t rhs_word = static_cast<std::uint32_t>(rhs);

    switch (op) {
        case OpType::Mul:
            return sign_extend_word(lhs_word * rhs_word);
        case OpType::Div: {
            const std::int32_t signed_lhs = std::bit_cast<std::int32_t>(lhs_word);
            const std::int32_t signed_rhs = std::bit_cast<std::int32_t>(rhs_word);
            if (signed_rhs == 0) {
                return std::numeric_limits<std::uint64_t>::max();
            }
            if (
                signed_lhs == std::numeric_limits<std::int32_t>::min() &&
                signed_rhs == -1
            ) {
                return sign_extend_word(lhs_word);
            }
            return sign_extend_word(
                std::bit_cast<std::uint32_t>(signed_lhs / signed_rhs)
            );
        }
        case OpType::Divu:
            return sign_extend_word(
                rhs_word == 0
                    ? std::numeric_limits<std::uint32_t>::max()
                    : lhs_word / rhs_word
            );
        case OpType::Rem: {
            const std::int32_t signed_lhs = std::bit_cast<std::int32_t>(lhs_word);
            const std::int32_t signed_rhs = std::bit_cast<std::int32_t>(rhs_word);
            if (signed_rhs == 0) {
                return sign_extend_word(lhs_word);
            }
            if (
                signed_lhs == std::numeric_limits<std::int32_t>::min() &&
                signed_rhs == -1
            ) {
                return 0;
            }
            return sign_extend_word(
                std::bit_cast<std::uint32_t>(signed_lhs % signed_rhs)
            );
        }
        case OpType::Remu:
            return sign_extend_word(
                rhs_word == 0 ? lhs_word : lhs_word % rhs_word
            );
        default:
            throw std::logic_error("operation is not an RV64M word instruction");
    }
}
