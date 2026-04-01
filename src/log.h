#pragma once

#ifdef TPCC_NO_SPDLOG

#define LOG_T(...) (void)0
#define LOG_D(...) (void)0
#define LOG_I(...) (void)0
#define LOG_W(...) (void)0
#define LOG_E(...) (void)0

#else

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

namespace NTPCC {

// Initialize the global logger. Call once at startup.
void InitLogging(spdlog::level::level_enum level = spdlog::level::info);

// Get the shared logger instance
std::shared_ptr<spdlog::logger>& GetLogger();

} // namespace NTPCC

#define LOG_T(...) SPDLOG_LOGGER_TRACE(NTPCC::GetLogger(), __VA_ARGS__)
#define LOG_D(...) SPDLOG_LOGGER_DEBUG(NTPCC::GetLogger(), __VA_ARGS__)
#define LOG_I(...) SPDLOG_LOGGER_INFO(NTPCC::GetLogger(), __VA_ARGS__)
#define LOG_W(...) SPDLOG_LOGGER_WARN(NTPCC::GetLogger(), __VA_ARGS__)
#define LOG_E(...) SPDLOG_LOGGER_ERROR(NTPCC::GetLogger(), __VA_ARGS__)

#endif
