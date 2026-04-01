#include "spsc_circular_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>

using namespace NTPCC;

TEST(TSpscCircularQueueSingleThread, ShouldReportEmptyInitially) {
    TSpscCircularQueue<int> queue;
    queue.Resize(3);
    ASSERT_TRUE(queue.Empty());
    ASSERT_FALSE(queue.IsFull());
    ASSERT_EQ(queue.Size(), 0u);
}

TEST(TSpscCircularQueueSingleThread, ShouldPushAndPopSingleItem) {
    TSpscCircularQueue<int> queue;
    queue.Resize(1);

    ASSERT_TRUE(queue.TryPush(42));
    ASSERT_FALSE(queue.Empty());
    ASSERT_TRUE(queue.IsFull());

    int value = 0;
    ASSERT_TRUE(queue.TryPop(value));
    ASSERT_EQ(value, 42);
    ASSERT_TRUE(queue.Empty());
}

TEST(TSpscCircularQueueSingleThread, ShouldRejectPushWhenFull) {
    TSpscCircularQueue<int> queue;
    queue.Resize(2);
    ASSERT_TRUE(queue.TryPush(1));
    ASSERT_TRUE(queue.TryPush(2));
    ASSERT_FALSE(queue.TryPush(3));

    ASSERT_TRUE(queue.IsFull());
    ASSERT_EQ(queue.Size(), 2u);
}

TEST(TSpscCircularQueueSingleThread, ShouldRejectPopWhenEmpty) {
    TSpscCircularQueue<int> queue;
    queue.Resize(2);
    int value = 0;
    ASSERT_FALSE(queue.TryPop(value));
}

TEST(TSpscCircularQueueSingleThread, ShouldPreserveFIFOOrder) {
    TSpscCircularQueue<int> queue;
    queue.Resize(3);

    ASSERT_TRUE(queue.TryPush(10));
    ASSERT_TRUE(queue.TryPush(20));
    ASSERT_TRUE(queue.TryPush(30));

    int val;
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 10);

    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 20);

    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 30);
}

TEST(TSpscCircularQueueSingleThread, ShouldWrapAroundProperly) {
    TSpscCircularQueue<int> queue;
    queue.Resize(4);

    ASSERT_TRUE(queue.TryPush(1));
    ASSERT_TRUE(queue.TryPush(2));
    int val;
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_TRUE(queue.TryPop(val));

    ASSERT_TRUE(queue.TryPush(3));
    ASSERT_TRUE(queue.TryPush(4));
    ASSERT_TRUE(queue.TryPush(5));
    ASSERT_TRUE(queue.TryPush(6));

    ASSERT_FALSE(queue.TryPush(7));

    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 3);
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 4);
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 5);
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, 6);
}

TEST(TSpscCircularQueueSingleThread, ShouldResizeThenOperate) {
    TSpscCircularQueue<std::string> queue;
    queue.Resize(2);
    ASSERT_TRUE(queue.TryPush("a"));
    ASSERT_TRUE(queue.TryPush("b"));
    ASSERT_FALSE(queue.TryPush("c"));

    std::string val;
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, "a");
    ASSERT_TRUE(queue.TryPop(val));
    ASSERT_EQ(val, "b");
    ASSERT_TRUE(queue.Empty());
}

TEST(TSpscCircularQueueSingleThread, ShouldRoundCapacityUpToPowerOfTwo) {
    TSpscCircularQueue<int> queue;
    queue.Resize(3);

    ASSERT_TRUE(queue.TryPush(1));
    ASSERT_TRUE(queue.TryPush(2));
    ASSERT_TRUE(queue.TryPush(3));
    ASSERT_TRUE(queue.TryPush(4));
    ASSERT_FALSE(queue.TryPush(5));
}

TEST(TSpscCircularQueueTwoThreads, ShouldTransferAllItemsInOrder) {
    constexpr size_t capacity = 1024;
    constexpr size_t itemCount = 200000;

    TSpscCircularQueue<size_t> queue;
    queue.Resize(capacity);

    std::atomic<bool> orderBroken = false;
    std::atomic<size_t> firstExpected = 0;
    std::atomic<size_t> firstActual = 0;
    std::atomic<size_t> consumed = 0;
    std::atomic<unsigned long long> consumedSum = 0;

    std::thread producer([&] {
        for (size_t i = 0; i < itemCount; ++i) {
            while (!queue.TryPush(size_t{i})) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        size_t expected = 0;
        while (expected < itemCount) {
            size_t value = 0;
            if (!queue.TryPop(value)) {
                std::this_thread::yield();
                continue;
            }

            if (value != expected && !orderBroken.exchange(true)) {
                firstExpected.store(expected);
                firstActual.store(value);
            }

            ++expected;
            consumed.fetch_add(1);
            consumedSum.fetch_add(static_cast<unsigned long long>(value));
        }
    });

    producer.join();
    consumer.join();

    ASSERT_FALSE(orderBroken.load())
        << "FIFO order broken: expected "
        << firstExpected.load()
        << ", got "
        << firstActual.load();
    ASSERT_EQ(consumed.load(), itemCount);

    const auto expectedSum = static_cast<unsigned long long>(itemCount - 1) * itemCount / 2;
    ASSERT_EQ(consumedSum.load(), expectedSum);
    ASSERT_TRUE(queue.Empty());
    ASSERT_EQ(queue.Size(), 0u);
}

TEST(TSpscCircularQueueTwoThreads, ShouldWorkWithCapacityOneUnderContention) {
    constexpr size_t capacity = 1;
    constexpr size_t itemCount = 50000;

    TSpscCircularQueue<size_t> queue;
    queue.Resize(capacity);

    std::atomic<bool> orderBroken = false;
    std::atomic<size_t> firstExpected = 0;
    std::atomic<size_t> firstActual = 0;
    std::atomic<size_t> consumed = 0;

    std::thread producer([&] {
        for (size_t i = 0; i < itemCount; ++i) {
            while (!queue.TryPush(size_t{i})) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        size_t expected = 0;
        while (expected < itemCount) {
            size_t value = 0;
            if (!queue.TryPop(value)) {
                std::this_thread::yield();
                continue;
            }

            if (value != expected && !orderBroken.exchange(true)) {
                firstExpected.store(expected);
                firstActual.store(value);
            }

            ++expected;
            consumed.fetch_add(1);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_FALSE(orderBroken.load())
        << "FIFO order broken with capacity=1: expected "
        << firstExpected.load()
        << ", got "
        << firstActual.load();
    ASSERT_EQ(consumed.load(), itemCount);
    ASSERT_TRUE(queue.Empty());
    ASSERT_EQ(queue.Size(), 0u);
}
