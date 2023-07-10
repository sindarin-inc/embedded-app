#include "Telemetry.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

const char *TelemetryFieldToString(TelemetryField field) {
    switch (field) {
    case METRIC_VOLTAGE:
        return "Voltage";
    case METRIC_READABLE_PLACE:
        return "ReadablePlace";
    case METRIC_SYNC_STATUS:
        return "SyncStatus";
    default:
        return "Unknown";
    }
}
