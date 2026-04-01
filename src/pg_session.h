#pragma once

#include "query_result.h"

#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/Unit.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include <pqxx/pqxx>

#include <memory>
#include <string>
#include <string_view>

namespace NTPCC {

class PgSession {
public:
    PgSession() = default;
    PgSession(std::unique_ptr<pqxx::connection> conn, folly::Executor* executor);

    PgSession(PgSession&& other) noexcept;
    PgSession& operator=(PgSession&& other) noexcept;

    PgSession(const PgSession&) = delete;
    PgSession& operator=(const PgSession&) = delete;

    ~PgSession();

    // Executes a parameterized query. Lazily begins a transaction on first call.
    folly::SemiFuture<QueryResult> ExecuteQuery(
        std::string_view sql, const pqxx::params& params = {});

    // Executes a query that doesn't return rows (INSERT/UPDATE/DELETE).
    folly::SemiFuture<uint64_t> ExecuteModify(
        std::string_view sql, const pqxx::params& params = {});

    folly::SemiFuture<folly::Unit> Commit();
    folly::SemiFuture<folly::Unit> Rollback();

    // For non-transactional operations (DDL, etc.)
    folly::SemiFuture<QueryResult> ExecuteNonTx(std::string_view sql);

    // For COPY-based bulk loading
    folly::SemiFuture<folly::Unit> ExecuteCopy(
        const std::string& tableName,
        const std::vector<std::string>& columns,
        std::function<void(pqxx::stream_to&)> writer);

    bool HasConnection() const { return conn_ != nullptr; }
    pqxx::connection& GetRawConnection() { return *conn_; }

    // Returns the underlying connection back (for pool return)
    std::unique_ptr<pqxx::connection> ReleaseConnection();

private:
    std::unique_ptr<pqxx::connection> conn_;
    std::unique_ptr<pqxx::work> txn_;
    folly::Executor* executor_ = nullptr;
};

} // namespace NTPCC
