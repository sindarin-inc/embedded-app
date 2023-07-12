#include "Telemetry.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

auto TelemetryFieldToString(TelemetryField field) -> const char * {
    switch (field) {
    case METRIC_VOLTAGE_LOG:
        return "VoltageLog";
    case METRIC_READABLE_PLACE:
        return "ReadablePlace";
    case METRIC_SYNC_STATUS:
        return "SyncStatus";
    default:
        return "Unknown";
    }
}
