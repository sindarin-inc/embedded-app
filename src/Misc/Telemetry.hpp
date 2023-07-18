#pragma once

#include <map>
#include <string>
#include <variant>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

// Currently 1MB
#define MAX_TELEMETRY_LOG_SIZE 1000000

enum TelemetryField {
    METRIC_EVENT,
    METRIC_COUNTER,
    METRIC_VOLTAGE_LOG,
    METRIC_READABLE_PLACE,
};
auto TelemetryFieldToString(TelemetryField field) -> const char *;

class Telemetry {
public:
    Telemetry(FileSystem &fs, PreferencesStore &prefs) : fileSystem_(fs), prefs_(prefs) {}
    Telemetry(const Telemetry &) = delete;

    template <typename T>
    void logMetric(TelemetryField field, const std::string &key, std::optional<T> value,
                   std::map<std::string, std::variant<int, uint32_t, float, std::string>> &data) {
        // create log directory in filesystem if it doesn't exist
        if (!fileSystem_.createDir("/metrics")) {
            log_e("could not create /metrics directory");
            return;
        }

        auto runTimeMs = GetRunTimeMs();
        auto bootCount = prefs_.bootCount.get().value_or(0);
        auto timestamp = SolClock::getInstance().getEpochTime();

        std::string dataJsonFormat = mapToJson(data);

        std::string valueFormat = "";
        if (value.has_value()) {
            if constexpr (std::is_same<T, float>::value) {
                valueFormat = StringFormat(",\"%s\":%.2f", key.c_str(), *value);
            } else if constexpr (std::is_same<T, uint32_t>::value || std::is_same<T, int>::value) {
                valueFormat = StringFormat(",\"%s\":%u", key.c_str(), *value);
            } else {
                valueFormat = StringFormat(R"(,"%s":"%s")", key.c_str(), (*value).c_str());
            }
        }

        std::string format = R"({"ts":%zu,"t":"%s","k":"%s")" + dataJsonFormat + valueFormat + "},";

        std::string logString;
        logString =
            StringFormat(format.c_str(), timestamp, TelemetryFieldToString(field), key.c_str());

        log_i("Logging metric: %s %s", TelemetryFieldToString(field), logString.c_str());

        DispatchGetCore1BackgroundQueue()->dispatch([this, logString, field]() {
            std::string path = Telemetry::metricLogPath();
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

    auto variantToJson(const std::variant<int, uint32_t, float, std::string> &v) -> std::string;
    auto
    mapToJson(const std::map<std::string, std::variant<int, uint32_t, float, std::string>> &data)
        -> std::string;

    static auto metricLogPath() -> std::string { return "/metrics/telemetry.jsonl"; }

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    unsigned long lastLoggingTime_ = 0;
};
