#include "tui_base.h"

#include "util.h"

#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>
#include <typeinfo>

namespace NTPCC {

namespace {

std::mutex ScreenPtrMutex;
ftxui::ScreenInteractive* ScreenPtr = nullptr;

std::terminate_handler PrevTerminateHandler = nullptr;

void CustomTerminateHandler() noexcept {
    static std::atomic_flag onceFlag;
    if (onceFlag.test_and_set()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::abort();
    }

    {
        std::lock_guard guard(ScreenPtrMutex);
        if (ScreenPtr) {
            ScreenPtr->Exit();

            for (size_t i = 0; i < 5; ++i) {
                if (GetGlobalInterruptSource().stop_requested()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    std::exception_ptr currentException = std::current_exception();
    int depth = std::uncaught_exceptions();

    std::cerr << "terminate called; uncaught_exceptions=" << depth << std::endl;

    if (currentException) {
        try {
            std::rethrow_exception(currentException);
        } catch (const std::exception& ex) {
            std::cerr << "Active exception: " << typeid(ex).name()
                      << " what(): " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Active exception (non-std type)" << std::endl;
        }
    } else {
        std::cerr << "No active exception (std::terminate called explicitly?)" << std::endl;
    }

    if (PrevTerminateHandler) {
        PrevTerminateHandler();
    } else {
        std::abort();
    }
}

} // anonymous

TuiBase::TuiBase()
    : Screen(ftxui::ScreenInteractive::Fullscreen())
{
    {
        std::lock_guard guard(ScreenPtrMutex);
        ScreenPtr = &Screen;
    }

    if (PrevTerminateHandler) {
        throw std::runtime_error("Terminate handler is already set");
    }

    PrevTerminateHandler = std::set_terminate(CustomTerminateHandler);
}

TuiBase::~TuiBase() {
    {
        std::lock_guard guard(ScreenPtrMutex);
        ScreenPtr = nullptr;
    }

    Screen.Exit();
    if (TuiThread.joinable()) {
        TuiThread.join();
    }

    std::set_terminate(PrevTerminateHandler);
}

void TuiBase::StartLoop() {
    TuiThread = std::thread([&] {
        Screen.Loop(BuildComponent());
        GetGlobalInterruptSource().request_stop();
    });
}

} // namespace NTPCC
