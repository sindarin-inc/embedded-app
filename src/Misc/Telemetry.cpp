#include "Telemetry.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

// Currently 1MB
#define MAX_TELEMETRY_LOG_SIZE 1000000

void Telemetry::logMetric(const std::string &name, float value) {
    // create log directory in filesystem if it doesn't exist
    if (!fileSystem_.createDir("/logs")) {
        log_e("could not create /logs directory");
        return;
    }

    auto runTimeMs = GetRunTimeMs();
    std::string logString =
        StringFormat("{\"rt\":%d,\"k\":%s,\"v\":%.2f}", runTimeMs, name.c_str(), value);

    log_d("Logging metric: %s", logString.c_str());

    std::string path = Telemetry::metricLogPath(name);
    SFile file = fileSystem_.open(path.c_str(), FILE_APPEND);
    if (file.size() < MAX_TELEMETRY_LOG_SIZE) {
        file.write((const uint8_t *)logString.c_str(), logString.length());
        file.write('\n');
        file.flush();
        file.close();
    } else {
        log_e(StringFormat("Telemetry log file for %s is too large. Ignoring.", name.c_str())
                  .c_str());
        file.close();
    }
}
