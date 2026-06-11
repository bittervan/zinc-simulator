#include <cstdint>
#include <array>

constexpr std::size_t kSubPartionPerSM = 4;
constexpr std::size_t kProcessingElementPerSMSP = 32;

class ProcessingElement {

}

class SubPartition {
    std::array<
};

class StreamMultiprocessor {
    std::array<SubPartition, kSubPartionPerSM> sub_partitions_;
};

