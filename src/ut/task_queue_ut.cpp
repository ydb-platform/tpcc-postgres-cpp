#include "task_queue.h"
#include "folly_coro_traits.h"

#include <gtest/gtest.h>

#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include <thread>
#include <vector>

using namespace NTPCC;

folly::SemiFuture<folly::Unit> MakeTransactionTask(int& counter) {
    counter++;
    co_return;
}

folly::SemiFuture<folly::Unit> MakeTerminalTask(
    ITaskQueue& queue, int& transactionCounter, int& sleepCounter, size_t terminalId)
{
    co_await TTaskReady(queue, terminalId);

    co_await TSuspend(queue, terminalId, std::chrono::milliseconds(10));
    sleepCounter++;

    co_await MakeTransactionTask(transactionCounter);

    co_await TSuspend(queue, terminalId, std::chrono::milliseconds(20));
    sleepCounter++;

    co_await MakeTransactionTask(transactionCounter);

    co_await TSuspend(queue, terminalId, std::chrono::milliseconds(30));
    sleepCounter++;

    co_return;
}

TEST(TTaskQueueTest, ShouldExecuteTerminalTaskWithSleepsAndTransactions) {
    auto queue = CreateTaskQueue(1, 0, 10, 10);

    int transactionCounter = 0;
    int sleepCounter = 0;
    const size_t terminalId = 1;

    queue->Run();

    auto taskFuture = MakeTerminalTask(*queue, transactionCounter, sleepCounter, terminalId);
    std::move(taskFuture).get();

    ASSERT_EQ(transactionCounter, 2);
    ASSERT_EQ(sleepCounter, 3);

    queue->Join();
}

folly::SemiFuture<folly::Unit> MakeTerminalTaskWithMultipleTransactions(
    ITaskQueue& queue, int& transactionCounter, int& sleepCounter,
    size_t terminalId, int numTransactions)
{
    co_await TTaskReady(queue, terminalId);

    for (int i = 0; i < numTransactions; ++i) {
        co_await TSuspend(queue, terminalId, std::chrono::milliseconds(10));
        sleepCounter++;
        co_await MakeTransactionTask(transactionCounter);
    }

    co_return;
}

TEST(TTaskQueueTest, ShouldExecuteMultipleTransactionsWithSleeps) {
    auto queue = CreateTaskQueue(1, 0, 10, 10);

    int transactionCounter = 0;
    int sleepCounter = 0;
    const size_t terminalId = 1;
    const int numTransactions = 5;

    queue->Run();

    auto taskFuture = MakeTerminalTaskWithMultipleTransactions(
        *queue, transactionCounter, sleepCounter, terminalId, numTransactions);
    std::move(taskFuture).get();

    ASSERT_EQ(transactionCounter, numTransactions);
    ASSERT_EQ(sleepCounter, numTransactions);

    queue->Join();
}

folly::SemiFuture<folly::Unit> MakeFailingTransactionTask() {
    throw std::runtime_error("Transaction failed");
    co_return;
}

folly::SemiFuture<folly::Unit> MakeTerminalTaskWithFailingTransaction(
    ITaskQueue& queue, size_t terminalId)
{
    co_await TTaskReady(queue, terminalId);
    co_await TSuspend(queue, terminalId, std::chrono::milliseconds(10));
    co_await MakeFailingTransactionTask();
    co_return;
}

TEST(TTaskQueueTest, ShouldPropagateTransactionFailure) {
    auto queue = CreateTaskQueue(1, 0, 10, 10);

    const size_t terminalId = 1;

    queue->Run();

    auto taskFuture = MakeTerminalTaskWithFailingTransaction(*queue, terminalId);
    try {
        std::move(taskFuture).get();
        FAIL() << "Expected exception not thrown";
    } catch (const std::runtime_error& e) {
        ASSERT_NE(std::string(e.what()).find("Transaction failed"), std::string::npos)
            << "Exception message: " << e.what();
    }

    queue->Join();
}

TEST(TTaskQueueTest, ShouldHandleMultipleTerminals) {
    const int numTerminals = 147;

    auto queue = CreateTaskQueue(4, 0, numTerminals, numTerminals);

    std::vector<int> transactionCounters(numTerminals, 0);
    std::vector<int> sleepCounters(numTerminals, 0);
    std::vector<folly::SemiFuture<folly::Unit>> taskFutures;

    queue->Run();

    for (int i = 0; i < numTerminals; ++i) {
        taskFutures.push_back(MakeTerminalTaskWithMultipleTransactions(
            *queue, transactionCounters[i], sleepCounters[i], i, 2));
    }

    for (size_t i = 0; i < taskFutures.size(); ++i) {
        std::move(taskFutures[i]).get();
    }

    for (int i = 0; i < numTerminals; ++i) {
        ASSERT_EQ(transactionCounters[i], 2);
        ASSERT_EQ(sleepCounters[i], 2);
    }

    queue->Join();
}

TEST(TTaskQueueTest, ShouldHandleQueueLimits) {
    const size_t maxTerminals = 2;
    const size_t maxTransactions = 2;
    auto queue = CreateTaskQueue(1, 0, maxTerminals, maxTransactions);

    int transactionCounter = 0;
    int sleepCounter = 0;
    std::vector<folly::SemiFuture<folly::Unit>> taskFutures;

    for (size_t i = 0; i < maxTerminals + 1; ++i) {
        taskFutures.push_back(MakeTerminalTask(*queue, transactionCounter, sleepCounter, i));
    }

    queue->Run();

    size_t exceptionCount = 0;
    for (size_t i = 0; i < taskFutures.size(); ++i) {
        try {
            std::move(taskFutures[i]).get();
        } catch (...) {
            ++exceptionCount;
        }
    }

    ASSERT_EQ(exceptionCount, 1u);

    queue->Join();
}

folly::SemiFuture<folly::Unit> MakeTransactionTaskWithInflight(
    ITaskQueue& queue, int& counter, size_t terminalId)
{
    co_await TTaskHasInflight(queue, terminalId);
    counter++;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    queue.DecInflight();
    co_return;
}

folly::SemiFuture<folly::Unit> MakeTerminalTaskWithInflightControl(
    ITaskQueue& queue, int& transactionCounter, size_t terminalId, int numTransactions)
{
    co_await TTaskReady(queue, terminalId);

    for (int i = 0; i < numTransactions; ++i) {
        co_await TSuspend(queue, terminalId, std::chrono::milliseconds(5));
        co_await MakeTransactionTaskWithInflight(queue, transactionCounter, terminalId);
    }
    co_return;
}

TEST(TTaskQueueTest, ShouldSupportUnlimitedInflight) {
    const size_t maxRunningTerminals = 0;
    const int numTerminals = 5;
    const int transactionsPerTerminal = 2;
    auto queue = CreateTaskQueue(4, maxRunningTerminals, numTerminals, numTerminals);

    std::vector<int> transactionCounters(numTerminals, 0);
    std::vector<folly::SemiFuture<folly::Unit>> taskFutures;

    queue->Run();

    for (int i = 0; i < numTerminals; ++i) {
        taskFutures.push_back(MakeTerminalTaskWithInflightControl(
            *queue, transactionCounters[i], i, transactionsPerTerminal));
    }

    for (size_t i = 0; i < taskFutures.size(); ++i) {
        std::move(taskFutures[i]).get();
    }

    for (int i = 0; i < numTerminals; ++i) {
        ASSERT_EQ(transactionCounters[i], transactionsPerTerminal);
    }

    queue->Join();
}

TEST(TTaskQueueTest, ShouldLimitInflightTerminals) {
    const size_t maxRunningTerminals = 2;
    const int numTerminals = 5;
    const int transactionsPerTerminal = 3;
    auto queue = CreateTaskQueue(4, maxRunningTerminals, numTerminals, numTerminals);

    std::vector<int> transactionCounters(numTerminals, 0);
    std::vector<folly::SemiFuture<folly::Unit>> taskFutures;

    queue->Run();

    for (int i = 0; i < numTerminals; ++i) {
        taskFutures.push_back(MakeTerminalTaskWithInflightControl(
            *queue, transactionCounters[i], i, transactionsPerTerminal));
    }

    for (size_t i = 0; i < taskFutures.size(); ++i) {
        std::move(taskFutures[i]).get();
    }

    for (int i = 0; i < numTerminals; ++i) {
        ASSERT_EQ(transactionCounters[i], transactionsPerTerminal);
    }

    queue->Join();
}
