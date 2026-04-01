#pragma once

#include "histogram.h"

#include <folly/futures/Future.h>
#include <folly/Try.h>
#include <folly/Unit.h>
#include <folly/executors/InlineExecutor.h>
#include <folly/MicroSpinLock.h>

#include <exception>
#include <coroutine>
#include <memory>
#include <utility>

namespace NTPCC {

//-----------------------------------------------------------------------------

using Clock = std::chrono::steady_clock;

//-----------------------------------------------------------------------------

class ITaskQueue {
public:
    struct TThreadStats {
        constexpr static size_t BUCKET_COUNT = 4096;
        constexpr static size_t MAX_HIST_VALUE = 32768;

        TThreadStats() = default;

        TThreadStats(const TThreadStats& other) = delete;
        TThreadStats(TThreadStats&& other) = delete;
        TThreadStats& operator=(const TThreadStats& other) = delete;
        TThreadStats& operator=(TThreadStats&& other) = delete;

        void Collect(TThreadStats& dst) {
            dst.InternalTasksSleeping.fetch_add(
                InternalTasksSleeping.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.InternalTasksWaitingInflight.fetch_add(
                InternalTasksWaitingInflight.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.InternalTasksReady.fetch_add(
                InternalTasksReady.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.ExternalTasksReady.fetch_add(
                ExternalTasksReady.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.InternalTasksResumed.fetch_add(
                InternalTasksResumed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.ExternalTasksResumed.fetch_add(
                ExternalTasksResumed.load(std::memory_order_relaxed), std::memory_order_relaxed);

            dst.ExecutingTime.fetch_add(ExecutingTime.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dst.TotalTime.fetch_add(TotalTime.load(std::memory_order_relaxed), std::memory_order_relaxed);

            std::lock_guard guard(HistLock);
            dst.InternalInflightWaitTimeMs.Add(InternalInflightWaitTimeMs);
            dst.InternalQueueTimeMs.Add(InternalQueueTimeMs);
            dst.ExternalQueueTimeMs.Add(ExternalQueueTimeMs);
        }

        std::atomic<uint64_t> InternalTasksSleeping{0};
        std::atomic<uint64_t> InternalTasksWaitingInflight{0};

        std::atomic<uint64_t> InternalTasksReady{0};
        std::atomic<uint64_t> ExternalTasksReady{0};

        std::atomic<uint64_t> InternalTasksResumed{0};
        std::atomic<uint64_t> ExternalTasksResumed{0};

        std::atomic<double> ExecutingTime{0};
        std::atomic<double> TotalTime{0};

        mutable folly::MicroSpinLock HistLock{0};
        THistogram InternalInflightWaitTimeMs{BUCKET_COUNT, MAX_HIST_VALUE};
        THistogram InternalQueueTimeMs{BUCKET_COUNT, MAX_HIST_VALUE};
        THistogram ExternalQueueTimeMs{BUCKET_COUNT, MAX_HIST_VALUE};
    };

    ITaskQueue() = default;
    virtual ~ITaskQueue() = default;

    ITaskQueue(const ITaskQueue&) = delete;
    ITaskQueue& operator=(const ITaskQueue&) = delete;
    ITaskQueue(ITaskQueue&&) = delete;
    ITaskQueue& operator=(ITaskQueue&&) = delete;

    virtual void Run() = 0;
    virtual void Join() = 0;
    virtual void WakeupAndNeverSleep() = 0;

    virtual void TaskReady(std::coroutine_handle<> handle, size_t threadHint) = 0;
    virtual void AsyncSleep(std::coroutine_handle<> handle, size_t threadHint, std::chrono::milliseconds delay) = 0;

    virtual bool IncInflight(std::coroutine_handle<> handle, size_t threadHint) = 0;
    virtual void DecInflight() = 0;

    virtual void TaskReadyThreadSafe(std::coroutine_handle<> handle, size_t threadHint) = 0;

    virtual bool CheckCurrentThread() const = 0;

    virtual void CollectStats(size_t threadIndex, TThreadStats& dst) = 0;

    virtual size_t GetRunningCount() const = 0;
};

std::unique_ptr<ITaskQueue> CreateTaskQueue(
    size_t threadCount,
    size_t maxRunningInternal,
    size_t maxReadyInternal,
    size_t maxReadyExternal);

//-----------------------------------------------------------------------------

struct TSuspend {
    TSuspend(ITaskQueue& taskQueue, size_t threadHint, std::chrono::milliseconds delay)
        : TaskQueue(taskQueue)
        , ThreadHint(threadHint)
        , Delay(delay)
    {
    }

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        TaskQueue.AsyncSleep(handle, ThreadHint, Delay);
    }

    void await_resume() {}

    ITaskQueue& TaskQueue;
    size_t ThreadHint;
    std::chrono::milliseconds Delay;
};

//-----------------------------------------------------------------------------

// Bridge between folly::SemiFuture and the coroutine task queue.
// When the future completes, the coroutine is resumed on the task queue thread.
template <typename T>
struct TSuspendWithFuture {
    TSuspendWithFuture(folly::SemiFuture<T>&& future, ITaskQueue& taskQueue, size_t threadHint)
        : Future(std::move(future))
        , TaskQueue(taskQueue)
        , ThreadHint(threadHint)
    {}

    TSuspendWithFuture() = delete;
    TSuspendWithFuture(const TSuspendWithFuture&) = delete;
    TSuspendWithFuture& operator=(const TSuspendWithFuture&) = delete;

    bool await_ready() { return Future.isReady(); }

    void await_suspend(std::coroutine_handle<> handle) {
        std::move(Future)
            .via(&folly::InlineExecutor::instance())
            .thenTry([this, handle](folly::Try<T>&& t) {
                Result = std::move(t);
                TaskQueue.TaskReadyThreadSafe(handle, ThreadHint);
            });
    }

    T await_resume() { return std::move(Result).value(); }

    folly::SemiFuture<T> Future;
    folly::Try<T> Result;
    ITaskQueue& TaskQueue;
    size_t ThreadHint;
};

template <>
struct TSuspendWithFuture<folly::Unit> {
    TSuspendWithFuture(folly::SemiFuture<folly::Unit>&& future, ITaskQueue& taskQueue, size_t threadHint)
        : Future(std::move(future))
        , TaskQueue(taskQueue)
        , ThreadHint(threadHint)
    {}

    TSuspendWithFuture() = delete;
    TSuspendWithFuture(const TSuspendWithFuture&) = delete;
    TSuspendWithFuture& operator=(const TSuspendWithFuture&) = delete;

    bool await_ready() { return Future.isReady(); }

    void await_suspend(std::coroutine_handle<> handle) {
        std::move(Future)
            .via(&folly::InlineExecutor::instance())
            .thenTry([this, handle](folly::Try<folly::Unit>&& t) {
                Result = std::move(t);
                TaskQueue.TaskReadyThreadSafe(handle, ThreadHint);
            });
    }

    void await_resume() { Result.throwUnlessValue(); }

    folly::SemiFuture<folly::Unit> Future;
    folly::Try<folly::Unit> Result;
    ITaskQueue& TaskQueue;
    size_t ThreadHint;
};

//-----------------------------------------------------------------------------

struct TTaskReady {
    TTaskReady(ITaskQueue& taskQueue, size_t threadHint)
        : TaskQueue(taskQueue)
        , ThreadHint(threadHint)
    {}

    bool await_ready() { return false; }

    bool await_suspend(std::coroutine_handle<> h) {
        if (TaskQueue.CheckCurrentThread()) {
            return false;
        }
        TaskQueue.TaskReadyThreadSafe(h, ThreadHint);
        return true;
    }

    void await_resume() {}

    ITaskQueue& TaskQueue;
    size_t ThreadHint;
};

struct TTaskHasInflight {
    TTaskHasInflight(ITaskQueue& taskQueue, size_t threadHint)
        : TaskQueue(taskQueue)
        , ThreadHint(threadHint)
    {}

    bool await_ready() { return false; }

    bool await_suspend(std::coroutine_handle<> h) {
        return TaskQueue.IncInflight(h, ThreadHint);
    }

    void await_resume() {}

    ITaskQueue& TaskQueue;
    size_t ThreadHint;
};

} // namespace NTPCC
