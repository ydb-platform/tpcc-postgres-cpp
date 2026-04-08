#include "workload.h"

namespace NCoroVsThreads {

void PreparePattern(uint8_t* pattern, uint64_t size) {
    for (uint64_t i = 0; i < size; ++i) {
        // Period-6 pattern: 1,1,1,0,1,0,1,1,1,0,1,0,...
        uint64_t phase = i % 6;
        pattern[i] = (phase != 3 && phase != 5) ? 1 : 0;
    }
}

void PrepareData(uint64_t* data, uint64_t size) {
    for (uint64_t i = 0; i < size; ++i) {
        data[i] = i * 0x9E3779B97F4A7C15ULL + 1;
    }
}

TOpResult PredictionFriendlyOp(const uint8_t* pattern, const uint64_t* data, uint64_t size) {
    uint64_t acc = 0;
    for (uint64_t i = 0; i < size; ++i) {
        if (pattern[i])
            acc += data[i];
        else
            acc ^= data[i];
    }
    return {
        .Acc = acc,
        .MemoryScannedBytes = size * (sizeof(uint8_t) + sizeof(uint64_t)),
    };
}

} // namespace NCoroVsThreads
