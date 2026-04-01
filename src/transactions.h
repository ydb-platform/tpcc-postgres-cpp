#pragma once

#include "pg_session.h"
#include "task_queue.h"

#include <folly/futures/Future.h>

#include <atomic>
#include <chrono>
#include <memory>

namespace NTPCC {

//-----------------------------------------------------------------------------

extern std::atomic<size_t> TransactionsInflight;

struct TTransactionInflightGuard {
    TTransactionInflightGuard() {
        TransactionsInflight.fetch_add(1, std::memory_order_relaxed);
    }

    ~TTransactionInflightGuard() {
        TransactionsInflight.fetch_sub(1, std::memory_order_relaxed);
    }
};

//-----------------------------------------------------------------------------

struct TTransactionContext {
    size_t TerminalID;
    size_t WarehouseID;
    size_t WarehouseCount;
    ITaskQueue& TaskQueue;

    // Simulation mode parameters (0 = disabled)
    int SimulateTransactionMs = 0;
    int SimulateTransactionSelect1 = 0;
};

struct TUserAbortedException : public std::runtime_error {
    TUserAbortedException() : std::runtime_error("User aborted transaction (expected rollback)") {}
};

//-----------------------------------------------------------------------------

// Each transaction is a coroutine returning folly::SemiFuture<bool>.
// Returns true on success, false on retryable failure.
// Throws on fatal errors.
// Latency is measured by the coroutine and stored in the latency output param.

folly::SemiFuture<bool> GetNewOrderTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

folly::SemiFuture<bool> GetDeliveryTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

folly::SemiFuture<bool> GetOrderStatusTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

folly::SemiFuture<bool> GetPaymentTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

folly::SemiFuture<bool> GetStockLevelTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

folly::SemiFuture<bool> GetSimulationTask(
    TTransactionContext& context,
    std::chrono::microseconds& latency,
    PgSession& session);

} // namespace NTPCC
