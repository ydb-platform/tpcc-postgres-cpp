#pragma once

#include "log.h"

#ifndef TPCC_NO_SPDLOG
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#endif

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

namespace NTPCC {

#ifndef TPCC_NO_SPDLOG
using TLogProcessor = std::function<void(spdlog::level::level_enum, const std::string&)>;
#endif

// Captures recent log lines for TUI display
class TLogCapture {
public:
    explicit TLogCapture(size_t maxLines = 1000);

    void AddLine(const std::string& line);
    std::vector<std::string> GetLines() const;
    void Clear();

private:
    mutable std::mutex Mutex_;
    std::vector<std::string> Lines_;
    size_t MaxLines_;
    size_t WritePos_ = 0;
    bool Wrapped_ = false;
};

} // namespace NTPCC
