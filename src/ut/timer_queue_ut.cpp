#include "timer_queue.h"

#include <gtest/gtest.h>

#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

using namespace NTPCC;

TEST(TBinnedTimerQueueTest, ShouldAddSingleItem) {
    TBinnedTimerQueue<int> queue(10, 100);
    queue.Add(std::chrono::milliseconds(10), 42);
    ASSERT_EQ(queue.Size(), 1UL);
    ASSERT_TRUE(queue.Validate());
}

TEST(TBinnedTimerQueueTest, ShouldPopItemInOrder) {
    TBinnedTimerQueue<int> queue(10, 100);
    queue.Add(std::chrono::milliseconds(1), 1);
    queue.Add(std::chrono::milliseconds(2), 2);
    queue.Add(std::chrono::milliseconds(3), 3);
    ASSERT_TRUE(queue.Validate());

    auto a = queue.PopFront().Value;
    ASSERT_TRUE(queue.Validate());
    auto b = queue.PopFront().Value;
    ASSERT_TRUE(queue.Validate());
    auto c = queue.PopFront().Value;

    ASSERT_EQ(a, 1);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(c, 3);
    ASSERT_EQ(queue.Size(), 0UL);
    ASSERT_TRUE(queue.Validate());
}

TEST(TBinnedTimerQueueTest, ShouldInsertOutOfOrderAndStillPopInOrder) {
    TBinnedTimerQueue<int> queue(10, 100);
    queue.Add(std::chrono::milliseconds(20), 2);
    ASSERT_TRUE(queue.Validate());
    queue.Add(std::chrono::milliseconds(10), 1);
    ASSERT_TRUE(queue.Validate());
    queue.Add(std::chrono::milliseconds(30), 3);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.PopFront().Value, 1);
    ASSERT_TRUE(queue.Validate());
    ASSERT_EQ(queue.PopFront().Value, 2);
    ASSERT_TRUE(queue.Validate());
    ASSERT_EQ(queue.PopFront().Value, 3);
    ASSERT_TRUE(queue.Validate());
}

TEST(TBinnedTimerQueueTest, ShouldAdvanceWhenBucketExhausted) {
    TBinnedTimerQueue<int> queue(10, 100);
    queue.Add(std::chrono::milliseconds(1), 11);
    ASSERT_TRUE(queue.Validate());

    queue.Add(std::chrono::milliseconds(200), 22);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.PopFront().Value, 11);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.PopFront().Value, 22);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.Size(), 0UL);
    ASSERT_TRUE(queue.Validate());
}

TEST(TBinnedTimerQueueTest, ShouldThrowOnEmptyPop) {
    TBinnedTimerQueue<int> queue(4, 100);
    ASSERT_TRUE(queue.Validate());

    ASSERT_THROW(queue.PopFront(), std::runtime_error);
}

TEST(TBinnedTimerQueueTest, ShouldRespectBucketSoftLimit) {
    TBinnedTimerQueue<int> queue(4, 1);
    queue.Add(std::chrono::milliseconds(100), 1);
    ASSERT_TRUE(queue.Validate());

    queue.Add(std::chrono::milliseconds(100), 2);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.Size(), 2UL);
    ASSERT_EQ(queue.PopFront().Value, 1);
    ASSERT_TRUE(queue.Validate());

    ASSERT_EQ(queue.PopFront().Value, 2);
    ASSERT_TRUE(queue.Validate());
}

TEST(TBinnedTimerQueueTest, ShouldPopInStrictOrderAfterShuffledInsertion) {
    TBinnedTimerQueue<int> queue(32, 29);

    std::vector<int> input;
    for (int i = 1; i <= 9999; ++i) {
        input.push_back(i);
    }

    std::mt19937 rng{12345};
    std::shuffle(input.begin(), input.end(), rng);

    for (int v : input) {
        queue.Add(std::chrono::milliseconds(v), v, std::chrono::steady_clock::time_point{});
        ASSERT_TRUE(queue.Validate());
    }

    for (int i = 1; i <= 999; ++i) {
        ASSERT_EQ(queue.PopFront().Value, i);
        ASSERT_TRUE(queue.Validate());
    }
}

TEST(TBinnedTimerQueueTest, ShouldDistributeTimersAcrossBuckets) {
    TBinnedTimerQueue<int> queue(8, 4);
    for (int i = 0; i < 8; ++i) {
        queue.Add(std::chrono::milliseconds(i * 100), i);
        ASSERT_TRUE(queue.Validate());
    }

    for (int i = 0; i < 8; ++i) {
        ASSERT_EQ(queue.PopFront().Value, i);
        ASSERT_TRUE(queue.Validate());
    }
}

TEST(TBinnedTimerQueueTest, ShouldHandleBucketBoundaries) {
    TBinnedTimerQueue<int> queue(4, 100);
    auto now = std::chrono::steady_clock::now();

    for (int i = 0; i < 100; ++i) {
        queue.Add(std::chrono::milliseconds(i), i, now);
    }

    queue.Add(std::chrono::milliseconds(100), 100, now);
    queue.Add(std::chrono::milliseconds(101), 101, now);

    for (int i = 0; i <= 101; ++i) {
        ASSERT_EQ(queue.PopFront().Value, i);
        ASSERT_TRUE(queue.Validate());
    }
}

TEST(TBinnedTimerQueueTest, ShouldHandleBucketOverflow) {
    TBinnedTimerQueue<int> queue(4, 2);

    for (int i = 0; i < 10; ++i) {
        queue.Add(std::chrono::milliseconds(i * 100), i);
        ASSERT_TRUE(queue.Validate());
    }

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(queue.PopFront().Value, i);
        ASSERT_TRUE(queue.Validate());
    }
}

TEST(TBinnedTimerQueueTest, ShouldHandleBucketRotation) {
    TBinnedTimerQueue<int> queue(3, 100);

    for (int i = 0; i < 300; ++i) {
        auto timePoint = std::chrono::steady_clock::time_point{} + std::chrono::milliseconds(i * 100);
        queue.Add(std::chrono::milliseconds(i * 100), i, timePoint);

        if (!queue.Validate()) {
            std::cerr << "Validation failed after adding item " << i << std::endl;
            ASSERT_TRUE(queue.Validate());
        }
    }

    for (int i = 0; i < 300; ++i) {
        auto item = queue.PopFront();
        ASSERT_EQ(item.Value, i);

        if (!queue.Validate()) {
            std::cerr << "Validation failed after popping item " << i << std::endl;
            ASSERT_TRUE(queue.Validate());
        }
    }
}

TEST(TBinnedTimerQueueTest, ShouldHandleBucketRotationWithFixedTimePoints) {
    TBinnedTimerQueue<int> queue(3, 100);
    auto baseTime = std::chrono::steady_clock::time_point{};

    for (int i = 0; i < 9; ++i) {
        auto timePoint = baseTime + std::chrono::milliseconds(i * 1000);
        queue.Add(std::chrono::milliseconds(i * 1000), i, timePoint);
        ASSERT_TRUE(queue.Validate());
    }

    for (int i = 0; i < 9; ++i) {
        auto item = queue.PopFront();
        ASSERT_EQ(item.Value, i);
        ASSERT_TRUE(queue.Validate());
    }
}
