#include "log_backend.h"
#include "log.h"

#ifndef TPCC_NO_SPDLOG

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace NTPCC {

//-----------------------------------------------------------------------------

// Wraps the real stderr sink. When capture is active, formats log messages
// as plain text and routes them to TLogCapture instead of stderr.
class TRoutingSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit TRoutingSink(spdlog::sink_ptr realSink)
        : RealSink_(std::move(realSink))
    {}

    void SetCapture(TLogCapture* capture) {
        std::lock_guard<std::mutex> lock(mutex_);
        Capture_ = capture;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (Capture_) {
            spdlog::memory_buf_t formatted;
            formatter_->format(msg, formatted);
            Capture_->AddLine(std::string(formatted.data(), formatted.size()));
        } else {
            RealSink_->log(msg);
        }
    }

    void flush_() override {
        RealSink_->flush();
    }

private:
    spdlog::sink_ptr RealSink_;
    TLogCapture* Capture_ = nullptr;
};

//-----------------------------------------------------------------------------

static std::shared_ptr<spdlog::logger> GlobalLogger;
static std::shared_ptr<TRoutingSink> GlobalRoutingSink;

void InitLogging(spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(level);
    console_sink->set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%l%$: %v");

    GlobalRoutingSink = std::make_shared<TRoutingSink>(console_sink);
    GlobalRoutingSink->set_level(level);
    GlobalRoutingSink->set_pattern("%Y-%m-%dT%H:%M:%S.%e %l: %v");

    GlobalLogger = std::make_shared<spdlog::logger>("tpcc", GlobalRoutingSink);
    GlobalLogger->set_level(level);
    spdlog::set_default_logger(GlobalLogger);
}

std::shared_ptr<spdlog::logger>& GetLogger() {
    if (!GlobalLogger) {
        InitLogging();
    }
    return GlobalLogger;
}

void StartLogCapture(TLogCapture& capture) {
    if (GlobalRoutingSink) {
        GlobalRoutingSink->SetCapture(&capture);
    }
}

void StopLogCapture() {
    if (GlobalRoutingSink) {
        GlobalRoutingSink->SetCapture(nullptr);
    }
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
