#pragma once

#include <string>

namespace NTPCC {

void CheckSync(const std::string& connectionString, int warehouseCount, bool afterImport = false);

} // namespace NTPCC
