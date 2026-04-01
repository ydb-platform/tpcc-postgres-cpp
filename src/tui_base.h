#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <thread>

namespace NTPCC {

// Base class for FTXUI-based TUI screens providing common lifecycle:
// - owns ScreenInteractive in fullscreen mode
// - starts a UI thread that runs Screen.Loop(BuildComponent())
// - requests global stop after loop exits
// - exits screen and joins thread in destructor
class TuiBase {
public:
    virtual ~TuiBase();

protected:
    TuiBase();

    void StartLoop();

    virtual ftxui::Component BuildComponent() = 0;

protected:
    ftxui::ScreenInteractive Screen;
    std::thread TuiThread;
};

} // namespace NTPCC
