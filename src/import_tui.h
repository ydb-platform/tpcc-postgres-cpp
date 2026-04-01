#pragma once

#include "import_display_data.h"
#include "log_backend.h"
#include "tui_base.h"

#include <ftxui/dom/elements.hpp>

namespace NTPCC {

class TImportTui : public TuiBase {
public:
    TImportTui(TLogCapture& logCapture, size_t warehouseCount, size_t loadThreads, const TImportDisplayData& data);

    void Update(const TImportDisplayData& data);

private:
    ftxui::Element BuildUpperPart();
    ftxui::Component BuildComponent() override;

private:
    TLogCapture& LogCapture;
    size_t WarehouseCount;
    size_t LoadThreads;
    TImportDisplayData DataToDisplay;
};

} // namespace NTPCC
