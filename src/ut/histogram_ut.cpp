#include "histogram.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace NTPCC;

TEST(THistogramTest, ShouldInitializeCorrectly) {
    THistogram hist(4, 16);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 0u);
}

TEST(THistogramTest, ShouldThrowOnInvalidParameters) {
    ASSERT_THROW(THistogram(0, 10), std::invalid_argument);
    ASSERT_THROW(THistogram(10, 0), std::invalid_argument);
    ASSERT_THROW(THistogram(20, 10), std::invalid_argument);
}

TEST(THistogramTest, ShouldRecordValuesInLinearBuckets) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(2);
    hist.RecordValue(3);

    ASSERT_EQ(hist.GetValueAtPercentile(25.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(75.0), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 4u);
}

TEST(THistogramTest, ShouldRecordValuesInExponentialBuckets) {
    THistogram hist(4, 16);

    hist.RecordValue(4);
    hist.RecordValue(7);
    hist.RecordValue(8);
    hist.RecordValue(15);

    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 8u);
    ASSERT_EQ(hist.GetValueAtPercentile(75.0), 16u);
}

TEST(THistogramTest, ShouldHandleValuesAboveMaxValue) {
    THistogram hist(4, 16);

    hist.RecordValue(20);
    hist.RecordValue(100);
    hist.RecordValue(1000);

    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 1000u);
}

TEST(THistogramTest, ShouldHandleZeroValues) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(0);
    hist.RecordValue(1);

    ASSERT_EQ(hist.GetValueAtPercentile(33.3), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(66.6), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 2u);
}

TEST(THistogramTest, ShouldAddHistograms) {
    THistogram hist1(4, 16);
    THistogram hist2(4, 16);

    hist1.RecordValue(0);
    hist1.RecordValue(1);
    hist2.RecordValue(2);
    hist2.RecordValue(3);

    hist1.Add(hist2);

    ASSERT_EQ(hist1.GetValueAtPercentile(25.0), 1u);
    ASSERT_EQ(hist1.GetValueAtPercentile(50.0), 2u);
    ASSERT_EQ(hist1.GetValueAtPercentile(75.0), 3u);
    ASSERT_EQ(hist1.GetValueAtPercentile(100.0), 4u);
}

TEST(THistogramTest, ShouldThrowOnAddingDifferentHistograms) {
    THistogram hist1(4, 16);
    THistogram hist2(5, 16);
    THistogram hist3(4, 20);

    ASSERT_THROW(hist1.Add(hist2), std::invalid_argument);
    ASSERT_THROW(hist1.Add(hist3), std::invalid_argument);
}

TEST(THistogramTest, ShouldResetHistogram) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(2);

    hist.Reset();

    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 0u);
}

TEST(THistogramTest, ShouldHandlePercentileEdgeCases) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(2);

    ASSERT_THROW(hist.GetValueAtPercentile(-1.0), std::invalid_argument);
    ASSERT_THROW(hist.GetValueAtPercentile(101.0), std::invalid_argument);
    ASSERT_EQ(hist.GetValueAtPercentile(0.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 3u);
}

TEST(THistogramTest, ShouldHandleEmptyHistogram) {
    THistogram hist(4, 16);

    ASSERT_EQ(hist.GetValueAtPercentile(0.0), 0u);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 0u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 0u);
}

TEST(THistogramTest, ShouldHandleSingleValueHistogram) {
    THistogram hist(4, 16);

    hist.RecordValue(0);

    ASSERT_EQ(hist.GetValueAtPercentile(0.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 1u);
}

TEST(THistogramTest, ShouldHandleLargeValues) {
    THistogram hist(4, 16);

    hist.RecordValue(std::numeric_limits<uint64_t>::max());
    hist.RecordValue(std::numeric_limits<uint64_t>::max() - 1);

    ASSERT_EQ(hist.GetValueAtPercentile(100.0), std::numeric_limits<uint64_t>::max());
}

TEST(THistogramTest, ShouldHandleAllBucketTypes) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(2);
    hist.RecordValue(3);
    hist.RecordValue(4);
    hist.RecordValue(7);
    hist.RecordValue(8);
    hist.RecordValue(15);
    hist.RecordValue(20);

    ASSERT_EQ(hist.GetValueAtPercentile(11.1), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(22.2), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(33.3), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(44.4), 4u);
    ASSERT_EQ(hist.GetValueAtPercentile(55.5), 8u);
    ASSERT_EQ(hist.GetValueAtPercentile(77.7), 16u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 20u);
}

TEST(THistogramTest, ShouldHandleLargeHistogram) {
    THistogram hist(128, 8192);

    for (uint64_t i = 0; i < 128; ++i) {
        hist.RecordValue(i);
    }
    hist.RecordValue(200);
    hist.RecordValue(400);
    hist.RecordValue(800);
    hist.RecordValue(1500);
    hist.RecordValue(3000);
    hist.RecordValue(6000);
    hist.RecordValue(10000);

    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 68u);
    ASSERT_EQ(hist.GetValueAtPercentile(95.0), 256u);
    ASSERT_EQ(hist.GetValueAtPercentile(99.0), 8192u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 10000u);
}

TEST(THistogramTest, ShouldHandleBucketBoundaries) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(2);
    hist.RecordValue(3);
    hist.RecordValue(4);
    hist.RecordValue(8);
    hist.RecordValue(16);

    ASSERT_EQ(hist.GetValueAtPercentile(14.3), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(28.6), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(42.9), 4u);
    ASSERT_EQ(hist.GetValueAtPercentile(57.1), 4u);
    ASSERT_EQ(hist.GetValueAtPercentile(71.4), 8u);
    ASSERT_EQ(hist.GetValueAtPercentile(85.7), 16u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 16u);
}

TEST(THistogramTest, ShouldHandleVerySmallPercentiles) {
    THistogram hist(4, 16);

    for (int i = 0; i < 1000; ++i) {
        hist.RecordValue(i % 4);
    }

    ASSERT_EQ(hist.GetValueAtPercentile(0.1), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(0.5), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(1.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(99.0), 4u);
    ASSERT_EQ(hist.GetValueAtPercentile(99.9), 4u);
}

TEST(THistogramTest, ShouldHandleExactPercentileBoundaries) {
    THistogram hist(4, 16);

    for (int i = 0; i < 25; ++i) {
        hist.RecordValue(0);
        hist.RecordValue(1);
        hist.RecordValue(2);
        hist.RecordValue(3);
    }

    ASSERT_EQ(hist.GetValueAtPercentile(25.0), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(75.0), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 4u);
}

TEST(THistogramTest, ShouldHandleMaxValueBoundary) {
    THistogram hist(4, 16);

    hist.RecordValue(15);
    hist.RecordValue(16);
    hist.RecordValue(17);

    ASSERT_EQ(hist.GetValueAtPercentile(33.3), 16u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 17u);
}

TEST(THistogramTest, ShouldHandleRepeatedValues) {
    THistogram hist(4, 16);

    for (int i = 0; i < 100; ++i) {
        hist.RecordValue(2);
    }

    ASSERT_EQ(hist.GetValueAtPercentile(1.0), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(50.0), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(99.0), 3u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), 3u);
}

TEST(THistogramTest, ShouldHandleDifferentHistogramSizes) {
    THistogram hist1(2, 8);
    THistogram hist2(16, 64);
    THistogram hist3(64, 1024);

    hist1.RecordValue(0);
    hist1.RecordValue(1);
    hist1.RecordValue(2);
    hist1.RecordValue(4);
    hist1.RecordValue(8);

    hist2.RecordValue(0);
    hist2.RecordValue(8);
    hist2.RecordValue(16);
    hist2.RecordValue(32);
    hist2.RecordValue(64);

    hist3.RecordValue(0);
    hist3.RecordValue(32);
    hist3.RecordValue(64);
    hist3.RecordValue(128);
    hist3.RecordValue(1024);

    ASSERT_EQ(hist1.GetValueAtPercentile(40.0), 2u);
    ASSERT_EQ(hist2.GetValueAtPercentile(40.0), 9u);
    ASSERT_EQ(hist3.GetValueAtPercentile(40.0), 33u);
}

TEST(THistogramTest, ShouldHandleEdgeCaseValues) {
    THistogram hist(4, 16);

    hist.RecordValue(0);
    hist.RecordValue(std::numeric_limits<uint64_t>::max());
    hist.RecordValue(std::numeric_limits<uint64_t>::max() - 1);

    ASSERT_EQ(hist.GetValueAtPercentile(33.3), 1u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), std::numeric_limits<uint64_t>::max());
}

TEST(THistogramTest, ShouldHandleStressTest) {
    THistogram hist(128, 8192);

    for (uint64_t i = 0; i < 10000; ++i) {
        uint64_t value = (i * 17 + 13) % 10000;
        hist.RecordValue(value);
    }

    uint64_t prev = 0;
    for (double p = 1.0; p <= 100.0; p += 1.0) {
        uint64_t current = hist.GetValueAtPercentile(p);
        ASSERT_GE(current, prev);
        prev = current;
    }
}

TEST(THistogramTest, ShouldHandleHistogramMerging) {
    THistogram hist1(128, 8192);
    THistogram hist2(128, 8192);
    THistogram hist3(128, 8192);

    for (uint64_t i = 0; i < 100; ++i) {
        hist1.RecordValue(i);
        hist2.RecordValue(i + 1000);
        hist3.RecordValue(i + 5000);
    }

    hist1.Add(hist2);
    hist1.Add(hist3);

    ASSERT_EQ(hist1.GetValueAtPercentile(33.3), 100u);
    ASSERT_EQ(hist1.GetValueAtPercentile(66.6), 2048u);
    ASSERT_EQ(hist1.GetValueAtPercentile(100.0), 8192u);
}

TEST(THistogramTest, ShouldHandleSpecific128_8192Case) {
    THistogram hist(128, 8192);

    hist.RecordValue(0);
    hist.RecordValue(1);
    hist.RecordValue(63);
    hist.RecordValue(127);
    hist.RecordValue(128);
    hist.RecordValue(255);
    hist.RecordValue(256);
    hist.RecordValue(511);
    hist.RecordValue(512);
    hist.RecordValue(1023);
    hist.RecordValue(1024);
    hist.RecordValue(2047);
    hist.RecordValue(2048);
    hist.RecordValue(4095);
    hist.RecordValue(4096);
    hist.RecordValue(8191);
    hist.RecordValue(8192);
    hist.RecordValue(10000);
    hist.RecordValue(std::numeric_limits<uint64_t>::max());

    ASSERT_EQ(hist.GetValueAtPercentile(5.3), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(10.5), 2u);
    ASSERT_EQ(hist.GetValueAtPercentile(26.3), 256u);
    ASSERT_EQ(hist.GetValueAtPercentile(47.4), 1024u);
    ASSERT_EQ(hist.GetValueAtPercentile(84.2), 8192u);
    ASSERT_EQ(hist.GetValueAtPercentile(100.0), std::numeric_limits<uint64_t>::max());

    THistogram hist2(128, 8192);
    hist2.RecordValue(8192);
    ASSERT_EQ(hist2.GetValueAtPercentile(100.0), 8192u);

    THistogram hist3(128, 8192);
    hist3.RecordValue(8191);
    ASSERT_EQ(hist3.GetValueAtPercentile(100.0), 8192u);
}
