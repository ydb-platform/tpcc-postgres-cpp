#include "clean.h"

#include "constants.h"
#include "log.h"

#include <pqxx/pqxx>

#include <fmt/format.h>

namespace NTPCC {

namespace {

void DropTable(pqxx::nontransaction& txn, const char* tableName) {
    std::string sql = fmt::format("DROP TABLE IF EXISTS {} CASCADE", tableName);
    LOG_T("Dropping table {}", tableName);
    try {
        txn.exec(sql);
        LOG_I("Table {} dropped successfully", tableName);
    } catch (const std::exception& ex) {
        LOG_W("Failed to drop table {}: {}", tableName, ex.what());
    }
}

} // anonymous

void CleanSync(const std::string& connectionString) {
    pqxx::connection conn(connectionString);
    pqxx::nontransaction txn(conn);

    LOG_I("Starting to drop TPC-C tables");

    DropTable(txn, TABLE_ORDER_LINE);
    DropTable(txn, TABLE_NEW_ORDER);
    DropTable(txn, TABLE_OORDER);
    DropTable(txn, TABLE_HISTORY);
    DropTable(txn, TABLE_CUSTOMER);
    DropTable(txn, TABLE_DISTRICT);
    DropTable(txn, TABLE_STOCK);
    DropTable(txn, TABLE_ITEM);
    DropTable(txn, TABLE_WAREHOUSE);

    LOG_I("All TPC-C tables dropped successfully");
}

} // namespace NTPCC
