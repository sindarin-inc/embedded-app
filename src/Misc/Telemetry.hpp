#pragma once

#include <string>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

// Currently 1MB
#define MAX_TELEMETRY_LOG_SIZE 1000000

enum TelemetryField {
    METRIC_VOLTAGE,
    METRIC_READABLE_PLACE,
    METRIC_SYNC_STATUS,
};
const char *TelemetryFieldToString(TelemetryField field);

class Telemetry {
public:
    Telemetry(FileSystem &fs, PreferencesStore &prefs) : fileSystem_(fs), prefs_(prefs) {}
    Telemetry(const Telemetry &) = delete;
    template <typename T>
    void logMetric(TelemetryField field, T value, std::string readableUuid = "") {
        // create log directory in filesystem if it doesn't exist
        if (!fileSystem_.createDir("/logs")) {
            log_e("could not create /logs directory");
            return;
        }

        auto runTimeMs = GetRunTimeMs();
        auto bootCount = prefs_.bootCount.get().value_or(0);

        std::string uuidFormat =
            readableUuid.empty()
                ? ""
                : ",\"u\":\"%s\""; // if readableUuid is not empty, add it to the format
        std::string format;

        if (std::is_same<T, float>::value) {
            format = "{\"bc\":%d,\"rt\":%d" + uuidFormat + ",\"v\":%.2f},";
        } else if (std::is_same<T, uint32_t>::value || std::is_same<T, int>::value) {
            format = "{\"bc\":%d,\"rt\":%d" + uuidFormat + ",\"v\":%u},";
        } else {
            format = "{\"bc\":%d,\"rt\":%d" + uuidFormat + ",\"v\":\"%s\"},";
        }

        std::string logString;
        if (!readableUuid.empty()) {
            logString =
                StringFormat(format.c_str(), bootCount, runTimeMs, readableUuid.c_str(), value);
        } else {
            logString = StringFormat(format.c_str(), bootCount, runTimeMs, value);
        }

        log_i("Logging metric: %s %s", TelemetryFieldToString(field), logString.c_str());

        DispatchGetCore1BackgroundQueue()->dispatch([this, logString, field]() {
            std::string path = Telemetry::metricLogPath(field);
            SFile file = fileSystem_.open(path.c_str(), FILE_APPEND);
            if (file.size() < MAX_TELEMETRY_LOG_SIZE) {
                file.write((const uint8_t *)logString.c_str(), logString.length());
                file.write('\n');
                file.flush();
                file.close();
            } else {
                log_e("%s", StringFormat("Telemetry log file for %s is too large. Ignoring.",
                                         TelemetryFieldToString(field))
                                .c_str());
                file.close();
            }
        });
    }

    static auto metricLogPath(TelemetryField field) -> std::string {
        return StringFormat("/logs/%s.jsonl", TelemetryFieldToString(field));
    }

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    unsigned long lastLoggingTime_ = 0;
};
