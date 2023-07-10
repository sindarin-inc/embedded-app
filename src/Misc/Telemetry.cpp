#include "Telemetry.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

std::vector<std::string> TelemetryFields = {"Voltage", "ReadablePlace", "SyncStatus"};
