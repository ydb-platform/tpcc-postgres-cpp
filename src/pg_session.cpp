#include "pg_session.h"
#include "log.h"

#include <folly/ExceptionWrapper.h>
#include <folly/executors/InlineExecutor.h>

namespace NTPCC {

PgSession::PgSession(std::unique_ptr<pqxx::connection> conn, folly::Executor* executor)
    : conn_(std::move(conn))
    , executor_(executor)
{}

PgSession::PgSession(PgSession&& other) noexcept
    : conn_(std::move(other.conn_))
    , txn_(std::move(other.txn_))
    , executor_(other.executor_)
{
    other.executor_ = nullptr;
}

PgSession& PgSession::operator=(PgSession&& other) noexcept {
    if (this != &other) {
        conn_ = std::move(other.conn_);
        txn_ = std::move(other.txn_);
        executor_ = other.executor_;
        other.executor_ = nullptr;
    }
    return *this;
}

PgSession::~PgSession() {
    if (txn_) {
        try {
            txn_->abort();
        } catch (...) {
        }
        txn_.reset();
    }
}

folly::SemiFuture<QueryResult> PgSession::ExecuteQuery(
    std::string_view sql, const pqxx::params& params)
{
    auto [promise, future] = folly::makePromiseContract<QueryResult>();
    std::string sqlCopy(sql);

    folly::via(executor_, [this, sqlCopy = std::move(sqlCopy), params,
                           p = std::move(promise)]() mutable {
        try {
            if (!txn_) {
                txn_ = std::make_unique<pqxx::work>(*conn_);
            }
            auto result = txn_->exec(sqlCopy, params);
            p.setValue(QueryResult(std::move(result)));
        } catch (...) {
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

folly::SemiFuture<uint64_t> PgSession::ExecuteModify(
    std::string_view sql, const pqxx::params& params)
{
    auto [promise, future] = folly::makePromiseContract<uint64_t>();
    std::string sqlCopy(sql);

    folly::via(executor_, [this, sqlCopy = std::move(sqlCopy), params,
                           p = std::move(promise)]() mutable {
        try {
            if (!txn_) {
                txn_ = std::make_unique<pqxx::work>(*conn_);
            }
            auto result = txn_->exec(sqlCopy, params);
            p.setValue(result.affected_rows());
        } catch (...) {
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

folly::SemiFuture<folly::Unit> PgSession::Commit() {
    auto [promise, future] = folly::makePromiseContract<folly::Unit>();

    folly::via(executor_, [this, p = std::move(promise)]() mutable {
        try {
            if (txn_) {
                txn_->commit();
                txn_.reset();
            }
            p.setValue(folly::unit);
        } catch (...) {
            txn_.reset();
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

folly::SemiFuture<folly::Unit> PgSession::Rollback() {
    auto [promise, future] = folly::makePromiseContract<folly::Unit>();

    folly::via(executor_, [this, p = std::move(promise)]() mutable {
        try {
            if (txn_) {
                txn_->abort();
                txn_.reset();
            }
            p.setValue(folly::unit);
        } catch (...) {
            txn_.reset();
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

folly::SemiFuture<QueryResult> PgSession::ExecuteNonTx(std::string_view sql) {
    auto [promise, future] = folly::makePromiseContract<QueryResult>();
    std::string sqlCopy(sql);

    folly::via(executor_, [this, sqlCopy = std::move(sqlCopy),
                           p = std::move(promise)]() mutable {
        try {
            pqxx::nontransaction ntx(*conn_);
            auto result = ntx.exec(sqlCopy);
            p.setValue(QueryResult(std::move(result)));
        } catch (...) {
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

folly::SemiFuture<folly::Unit> PgSession::ExecuteCopy(
    const std::string& tableName,
    const std::vector<std::string>& columns,
    std::function<void(pqxx::stream_to&)> writer)
{
    auto [promise, future] = folly::makePromiseContract<folly::Unit>();

    folly::via(executor_, [this, tableName, columns, writer = std::move(writer),
                           p = std::move(promise)]() mutable {
        try {
            if (!txn_) {
                txn_ = std::make_unique<pqxx::work>(*conn_);
            }
            auto stream = pqxx::stream_to::raw_table(
                *txn_, conn_->quote_table(tableName), conn_->quote_columns(columns));
            writer(stream);
            stream.complete();
            p.setValue(folly::unit);
        } catch (...) {
            p.setException(folly::exception_wrapper(std::current_exception()));
        }
    });

    return std::move(future);
}

std::unique_ptr<pqxx::connection> PgSession::ReleaseConnection() {
    if (txn_) {
        try { txn_->abort(); } catch (...) {}
        txn_.reset();
    }
    return std::move(conn_);
}

} // namespace NTPCC
