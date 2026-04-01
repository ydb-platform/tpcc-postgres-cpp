#include "import_tui.h"

#include "log.h"
#include "logs_scroller.h"
#include "util.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

#include <iomanip>
#include <sstream>

using namespace ftxui;

namespace NTPCC {

TImportTui::TImportTui(TLogCapture& logCapture, size_t warehouseCount, size_t loadThreads, const TImportDisplayData& data)
    : LogCapture(logCapture)
    , WarehouseCount(warehouseCount)
    , LoadThreads(loadThreads)
    , DataToDisplay(data)
{
    StartLoop();
}

void TImportTui::Update(const TImportDisplayData& data) {
    DataToDisplay = data;
    Screen.PostEvent(Event::Custom);
}

Element TImportTui::BuildUpperPart() {
    std::stringstream headerSs;
    headerSs << "TPC-C Import: " << WarehouseCount << " warehouses, "
             << LoadThreads << " threads   Estimated size: "
             << GetFormattedSize(DataToDisplay.ImportState.ApproximateDataSize);

    std::stringstream progressSs;
    progressSs << std::fixed << std::setprecision(1) << DataToDisplay.StatusData.PercentLoaded << "% ("
               << GetFormattedSize(DataToDisplay.StatusData.CurrentDataSizeLoaded) << ")";

    std::stringstream speedSs;
    speedSs << std::fixed << std::setprecision(1)
            << "Speed: " << DataToDisplay.StatusData.InstantSpeedMiBs << " MiB/s   "
            << "Avg: " << DataToDisplay.StatusData.AvgSpeedMiBs << " MiB/s   "
            << "Elapsed: " << DataToDisplay.StatusData.ElapsedMinutes << ":"
            << std::setfill('0') << std::setw(2) << DataToDisplay.StatusData.ElapsedSeconds << "   "
            << "ETA: ";

    if (DataToDisplay.StatusData.EstimatedTimeLeftMinutes != 0 || DataToDisplay.StatusData.EstimatedTimeLeftSeconds != 0) {
        speedSs << std::fixed << std::setprecision(1)
            << DataToDisplay.StatusData.EstimatedTimeLeftMinutes << ":"
            << std::setfill('0') << std::setw(2) << DataToDisplay.StatusData.EstimatedTimeLeftSeconds;
    } else {
        speedSs << "n/a";
    }

    float progressRatio = static_cast<float>(DataToDisplay.StatusData.PercentLoaded / 100.0);

    auto importDetails = vbox({
        text(headerSs.str()),
        hbox({
            text("Progress: "),
            gauge(progressRatio) | flex,
            text("  " + progressSs.str())
        }),
        text(speedSs.str())
    });

    auto topRow = window(text("TPC-C data upload"), hbox({
        importDetails
    }));

    return topRow;
}

Component TImportTui::BuildComponent() {
    try {
        return Container::Vertical({
            Renderer([=]{ return BuildUpperPart(); }),
            LogsScroller(LogCapture),
        });
    } catch (const std::exception& ex) {
        LOG_E("Exception in TUI: {}", ex.what());
        RequestStopWithError();
        return Renderer([] { return filler(); });
    }
}

} // namespace NTPCC
