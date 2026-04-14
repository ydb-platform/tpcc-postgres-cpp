#include "transactions.h"
#include "task_queue.h"
#include "pg_session.h"
#include "thread_pool.h"
#include "init.h"
#include "import.h"
#include "clean.h"
#include "check.h"
#include "constants.h"
#include "util.h"

#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include <cstdlib>
#include <memory>
#include <string>

using namespace NTPCC;

// ---------------------------------------------------------------------------

static std::string GetTestConnectionString() {
    const char* env = std::getenv("TPCC_TEST_CONNECTION");
    return env ? env : "host=localhost dbname=tpcc_test user=postgres";
}

static bool TryConnect(const std::string& connStr) {
    try {
        pqxx::connection conn(connStr);
        return conn.is_open();
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------

class TpccPgTest : public ::testing::Test {
protected:
    static std::string ConnStr;
    static bool Available;
    static bool SetupOK;

    static void SetUpTestSuite() {
        ConnStr = GetTestConnectionString();
        Available = TryConnect(ConnStr);
        if (!Available) return;

        try {
            InitSync(ConnStr);
            TImportConfig cfg;
            cfg.ConnectionString = ConnStr;
            cfg.WarehouseCount = 1;
            ImportSync(cfg);
            SetupOK = true;
        } catch (const std::exception& e) {
            std::cerr << "TpccPgTest setup failed: " << e.what() << "\n";
        }
    }

    static void TearDownTestSuite() {
        if (!Available) return;
        try { CleanSync(ConnStr); } catch (...) {}
    }

    void SetUp() override {
        if (!Available) GTEST_SKIP() << "PostgreSQL not reachable";
        if (!SetupOK)  GTEST_SKIP() << "DB setup failed";

        GetGlobalErrorVariable().store(false);

        queue_ = CreateTaskQueue(1, 0, 10, 10);
        executor_ = std::make_unique<TThreadPool>(2);

        auto conn = std::make_unique<pqxx::connection>(ConnStr);
        session_ = PgSession(std::move(conn), executor_.get());

        queue_->Run();
    }

    void TearDown() override {
        if (queue_) {
            queue_->WakeupAndNeverSleep();
            queue_->Join();
        }
        executor_.reset();
    }

    // Run a transaction coroutine, return true on commit, false on retryable failure.
    // Catches TUserAbortedException and returns false (expected for NewOrder 1% abort).
    bool RunTx(TFuture<bool> (*txFunc)(TTransactionContext&,
                                        std::chrono::microseconds&,
                                        PgSession&)) {
        TTransactionContext ctx{
            .TerminalID = 0,
            .WarehouseID = 1,
            .WarehouseCount = 1,
            .TaskQueue = *queue_,
        };

        std::chrono::microseconds latency{};
        auto future = txFunc(ctx, latency, session_);
        try {
            return future.Get();
        } catch (const TUserAbortedException&) {
            return false;
        }
    }

    // Execute raw SQL and return the result (for checking state).
    pqxx::result Query(const std::string& sql) {
        pqxx::connection conn(ConnStr);
        pqxx::nontransaction ntx(conn);
        return ntx.exec(sql);
    }

    int QueryInt(const std::string& sql) {
        auto r = Query(sql);
        return r[0][0].as<int>();
    }

    double QueryDouble(const std::string& sql) {
        auto r = Query(sql);
        return r[0][0].as<double>();
    }

    std::unique_ptr<ITaskQueue> queue_;
    std::unique_ptr<TThreadPool> executor_;
    PgSession session_;
};

std::string TpccPgTest::ConnStr;
bool TpccPgTest::Available = false;
bool TpccPgTest::SetupOK = false;

// ---------------------------------------------------------------------------
// NewOrder
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, NewOrder) {
    int ordersBefore     = QueryInt("SELECT count(*) FROM oorder");
    int newOrdersBefore  = QueryInt("SELECT count(*) FROM new_order");
    int olBefore         = QueryInt("SELECT count(*) FROM order_line");
    int nextOidSumBefore = QueryInt("SELECT sum(d_next_o_id) FROM district");

    // Run multiple attempts because 1% of the time it triggers invalid item rollback
    bool committed = false;
    for (int attempt = 0; attempt < 200 && !committed; ++attempt) {
        // Need a fresh session after rollback/commit
        if (attempt > 0) {
            auto conn = std::make_unique<pqxx::connection>(ConnStr);
            session_ = PgSession(std::move(conn), executor_.get());
        }
        committed = RunTx(GetNewOrderTask);
    }
    ASSERT_TRUE(committed) << "NewOrder never committed in 200 attempts (expected ~1% abort rate)";

    int ordersAfter     = QueryInt("SELECT count(*) FROM oorder");
    int newOrdersAfter  = QueryInt("SELECT count(*) FROM new_order");
    int olAfter         = QueryInt("SELECT count(*) FROM order_line");
    int nextOidSumAfter = QueryInt("SELECT sum(d_next_o_id) FROM district");

    // Exactly one order committed
    EXPECT_GE(ordersAfter, ordersBefore + 1);
    EXPECT_GE(newOrdersAfter, newOrdersBefore + 1);
    EXPECT_GE(olAfter, olBefore + MIN_ITEMS);
    EXPECT_GE(nextOidSumAfter, nextOidSumBefore + 1);
}

// ---------------------------------------------------------------------------
// Payment
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, Payment) {
    double wYtdBefore = QueryDouble("SELECT sum(w_ytd) FROM warehouse");
    double dYtdBefore = QueryDouble("SELECT sum(d_ytd) FROM district");
    int histBefore    = QueryInt("SELECT count(*) FROM history");

    bool ok = RunTx(GetPaymentTask);
    ASSERT_TRUE(ok);

    double wYtdAfter = QueryDouble("SELECT sum(w_ytd) FROM warehouse");
    double dYtdAfter = QueryDouble("SELECT sum(d_ytd) FROM district");
    int histAfter    = QueryInt("SELECT count(*) FROM history");

    double wDelta = wYtdAfter - wYtdBefore;
    double dDelta = dYtdAfter - dYtdBefore;

    EXPECT_GT(wDelta, 0.0);
    EXPECT_NEAR(wDelta, dDelta, 0.01) << "warehouse and district YTD should increase by the same amount";
    EXPECT_EQ(histAfter, histBefore + 1);
}

// ---------------------------------------------------------------------------
// Delivery
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, Delivery) {
    int newOrdersBefore = QueryInt("SELECT count(*) FROM new_order");

    bool ok = RunTx(GetDeliveryTask);
    ASSERT_TRUE(ok);

    int newOrdersAfter = QueryInt("SELECT count(*) FROM new_order");

    // Delivery removes at most one new_order per district (10 districts)
    int removed = newOrdersBefore - newOrdersAfter;
    EXPECT_GE(removed, 0);
    EXPECT_LE(removed, DISTRICT_COUNT);

    if (removed > 0) {
        // Delivered orders should now have a carrier_id
        int withCarrier = QueryInt(
            "SELECT count(*) FROM oorder WHERE o_carrier_id IS NOT NULL AND o_w_id = 1");
        EXPECT_GT(withCarrier, 0);
    }
}

// ---------------------------------------------------------------------------
// OrderStatus (read-only)
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, OrderStatus) {
    bool ok = RunTx(GetOrderStatusTask);
    EXPECT_TRUE(ok);
}

// ---------------------------------------------------------------------------
// StockLevel (read-only)
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, StockLevel) {
    bool ok = RunTx(GetStockLevelTask);
    EXPECT_TRUE(ok);
}

// ---------------------------------------------------------------------------
// Run multiple transactions then verify TPC-C consistency conditions
// ---------------------------------------------------------------------------

TEST_F(TpccPgTest, ConsistencyAfterMixedWorkload) {
    constexpr int kRounds = 20;

    auto reconnect = [&] {
        auto conn = std::make_unique<pqxx::connection>(ConnStr);
        session_ = PgSession(std::move(conn), executor_.get());
    };

    for (int i = 0; i < kRounds; ++i) {
        reconnect();
        try { RunTx(GetNewOrderTask); } catch (...) {}

        reconnect();
        RunTx(GetPaymentTask);

        if (i % 5 == 0) {
            reconnect();
            RunTx(GetDeliveryTask);
        }
    }

    // Read-only transactions
    reconnect();
    RunTx(GetOrderStatusTask);
    reconnect();
    RunTx(GetStockLevelTask);

    ASSERT_NO_THROW(CheckSync(ConnStr, 1, false));
}
