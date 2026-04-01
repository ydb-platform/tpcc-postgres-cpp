#include "histogram.h"
#include <cmath>
#include <stdexcept>
#include <limits>

namespace NTPCC {

THistogram::THistogram(uint64_t hdrTill, uint64_t maxValue)
    : HdrTill_(hdrTill)
    , MaxValue_(maxValue)
    , TotalCount_(0)
    , MaxRecordedValue_(0)
{
    if (hdrTill == 0 || maxValue == 0 || hdrTill > maxValue) {
        throw std::invalid_argument("Invalid histogram parameters");
    }

    size_t totalBuckets = GetTotalBuckets();
    Buckets_.resize(totalBuckets, 0);
}

void THistogram::RecordValue(uint64_t value) {
    size_t bucketIndex = GetBucketIndex(value);
    if (bucketIndex >= Buckets_.size()) {
        throw std::runtime_error("THistogram internal error");
    }

    Buckets_[bucketIndex]++;
    TotalCount_++;
    MaxRecordedValue_ = std::max(MaxRecordedValue_, value);
}

void THistogram::Add(const THistogram& other) {
    if (HdrTill_ != other.HdrTill_ || MaxValue_ != other.MaxValue_) {
        throw std::invalid_argument("Cannot add histograms with different parameters");
    }

    for (size_t i = 0; i < Buckets_.size() && i < other.Buckets_.size(); ++i) {
        Buckets_[i] += other.Buckets_[i];
    }
    TotalCount_ += other.TotalCount_;
    MaxRecordedValue_ = std::max(MaxRecordedValue_, other.MaxRecordedValue_);
}

void THistogram::Sub(const THistogram& other) {
    if (HdrTill_ != other.HdrTill_ || MaxValue_ != other.MaxValue_) {
        throw std::invalid_argument("Cannot sub histograms with different parameters");
    }

    for (size_t i = 0; i < Buckets_.size() && i < other.Buckets_.size(); ++i) {
        Buckets_[i] -= other.Buckets_[i];
    }
    TotalCount_ -= other.TotalCount_;
}

uint64_t THistogram::GetValueAtPercentile(double percentile) const {
    if (percentile < 0.0 || percentile > 100.0) {
        throw std::invalid_argument("Percentile must be between 0 and 100");
    }

    if (TotalCount_ == 0) {
        return 0;
    }

    uint64_t targetCount = static_cast<uint64_t>(std::ceil(percentile * TotalCount_ / 100.0));
    uint64_t currentCount = 0;

    for (size_t i = 0; i < Buckets_.size(); ++i) {
        currentCount += Buckets_[i];
        if (currentCount >= targetCount) {
            return GetBucketUpperBound(i);
        }
    }

    return std::numeric_limits<uint64_t>::max();
}

void THistogram::Reset() {
    std::fill(Buckets_.begin(), Buckets_.end(), 0);
    TotalCount_ = 0;
    MaxRecordedValue_ = 0;
}

size_t THistogram::GetBucketIndex(uint64_t value) const {
    if (value < HdrTill_) {
        return static_cast<size_t>(value);
    } else {
        uint64_t bucketStart = HdrTill_;
        uint64_t bucketSize = HdrTill_;
        size_t bucketIndex = HdrTill_;

        while (bucketIndex < Buckets_.size() - 1) {
            uint64_t bucketEnd = bucketStart + bucketSize;
            if (value < bucketEnd) {
                return bucketIndex;
            }
            bucketStart = bucketEnd;
            bucketSize *= 2;
            bucketIndex++;
        }

        return Buckets_.size() - 1;
    }
}

uint64_t THistogram::GetBucketUpperBound(size_t bucketIndex) const {
    if (bucketIndex < HdrTill_) {
        return static_cast<uint64_t>(bucketIndex + 1);
    } else {
        if (bucketIndex == Buckets_.size() - 1) {
            return MaxRecordedValue_ > 0 ? MaxRecordedValue_ : std::numeric_limits<uint64_t>::max();
        }

        uint64_t bucketStart = HdrTill_;
        uint64_t bucketSize = HdrTill_;
        size_t currentBucket = HdrTill_;

        while (currentBucket < bucketIndex) {
            bucketStart += bucketSize;
            bucketSize *= 2;
            currentBucket++;
        }

        return bucketStart + bucketSize;
    }
}

size_t THistogram::GetTotalBuckets() const {
    return HdrTill_ + GetExponentialBucketCount() + 1;
}

size_t THistogram::GetExponentialBucketCount() const {
    size_t count = 0;
    for (uint64_t range = HdrTill_ * 2; range <= MaxValue_; range *= 2) {
        count++;
    }
    return count;
}

} // namespace NTPCC
