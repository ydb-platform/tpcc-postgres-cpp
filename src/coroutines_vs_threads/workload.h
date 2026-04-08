#pragma once

#include <cstdint>

namespace NCoroVsThreads {

constexpr uint64_t DATA_SIZE = 1024;

struct TOpResult {
    uint64_t Acc = 0;
    uint64_t MemoryScannedBytes = 0;
};

void PreparePattern(uint8_t* pattern, uint64_t size);
void PrepareData(uint64_t* data, uint64_t size);

// Returns an accumulator value; must not be optimized away.
TOpResult PredictionFriendlyOp(const uint8_t* pattern, const uint64_t* data, uint64_t size);

} // namespace NCoroVsThreads
