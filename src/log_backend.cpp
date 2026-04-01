#include "log_backend.h"
#include "log.h"

#ifndef TPCC_NO_SPDLOG

#include <spdlog/sinks/stdout_color_sinks.h>

namespace NTPCC {

//-----------------------------------------------------------------------------

static std::shared_ptr<spdlog::logger> GlobalLogger;

void InitLogging(spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(level);
    console_sink->set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%l%$: %v");

    GlobalLogger = std::make_shared<spdlog::logger>("tpcc", console_sink);
    GlobalLogger->set_level(level);
    spdlog::set_default_logger(GlobalLogger);
}

std::shared_ptr<spdlog::logger>& GetLogger() {
    if (!GlobalLogger) {
        InitLogging();
    }
    return GlobalLogger;
}

} // namespace NTPCC
#endif // TPCC_NO_SPDLOG

namespace NTPCC {

//-----------------------------------------------------------------------------

TLogCapture::TLogCapture(size_t maxLines)
    : MaxLines_(maxLines)
{
    Lines_.resize(maxLines);
}

void TLogCapture::AddLine(const std::string& line) {
    std::lock_guard lock(Mutex_);
    Lines_[WritePos_] = line;
    WritePos_ = (WritePos_ + 1) % MaxLines_;
    if (WritePos_ == 0) {
        Wrapped_ = true;
    }
}

std::vector<std::string> TLogCapture::GetLines() const {
    std::lock_guard lock(Mutex_);
    std::vector<std::string> result;
    if (Wrapped_) {
        for (size_t i = WritePos_; i < MaxLines_; ++i) {
            if (!Lines_[i].empty()) result.push_back(Lines_[i]);
        }
        for (size_t i = 0; i < WritePos_; ++i) {
            if (!Lines_[i].empty()) result.push_back(Lines_[i]);
        }
    } else {
        for (size_t i = 0; i < WritePos_; ++i) {
            if (!Lines_[i].empty()) result.push_back(Lines_[i]);
        }
    }
    return result;
}

void TLogCapture::Clear() {
    std::lock_guard lock(Mutex_);
    for (auto& line : Lines_) {
        line.clear();
    }
    WritePos_ = 0;
    Wrapped_ = false;
}

} // namespace NTPCC
