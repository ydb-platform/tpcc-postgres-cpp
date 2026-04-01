#pragma once

#include "import.h"

namespace NTPCC {

struct TImportStatusData {
    size_t CurrentDataSizeLoaded = 0;
    double PercentLoaded = 0.0;
    double InstantSpeedMiBs = 0.0;
    double AvgSpeedMiBs = 0.0;
    int ElapsedMinutes = 0;
    int ElapsedSeconds = 0;
    int EstimatedTimeLeftMinutes = 0;
    int EstimatedTimeLeftSeconds = 0;
};

struct TImportDisplayData {
    TImportDisplayData(const TImportState& importState)
        : ImportState(importState)
    {
    }

    TImportDisplayData(const TImportDisplayData&) = default;
    TImportDisplayData& operator=(const TImportDisplayData&) = default;
    TImportDisplayData(TImportDisplayData&&) = default;
    TImportDisplayData& operator=(TImportDisplayData&&) = default;

    TImportState ImportState;
    TImportStatusData StatusData;
};

} // namespace NTPCC
