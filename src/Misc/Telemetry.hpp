#pragma once

#include <unordered_map>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/TelemetryVariant.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

// Currently 1MB
#define MAX_TELEMETRY_LOG_SIZE 1000000

enum TelemetryField {
    METRIC_EVENT,
    METRIC_BATTERY_VOLTAGE,
    METRIC_READABLE_PLACE,
    METRIC_READABLE_OPEN,
    METRIC_SET_PLACEMARK,
    METRIC_SYNC_STATUS,
    METRIC_BOOT,
    METRIC_CLOCK_SET,
    METRIC_SHUTDOWN,
    METRIC_WIFI,
    METRIC_LIGHTING,
    METRIC_REPORT,
    METRIC_STYLE,
};
auto TelemetryFieldToString(TelemetryField field) -> const char *;

class Telemetry {
public:
    Telemetry(FileSystem &fs, PreferencesStore &prefs) : fileSystem_(fs), prefs_(prefs) {}
    Telemetry(const Telemetry &) = delete;

    void logMetricData(TelemetryField field,
                       std::unordered_map<std::string, TelemetryVariant> data = {}) {
        logMetricImpl(field, "", data);
    }

    template <typename T>
    void logMetric(TelemetryField field, T value,
                   std::unordered_map<std::string, TelemetryVariant> data = {}) {
        logMetricImpl(field, value, data);
    }

    auto variantToJson(const TelemetryVariant &v) -> std::string;
    auto mapToJson(const std::unordered_map<std::string, TelemetryVariant> &data) -> std::string;

    static auto metricLogPath() -> std::string { return "/metrics/telemetry.jsonl"; }

    auto uploadAndResetTelemetry(FileSystem &fileSystem, Telemetry &telemetry, bool useBle = false)
        -> int;

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    unsigned long lastLoggingTime_ = 0;

private:
    static auto dispatchQueue() -> DispatchQueue * { return DispatchGetCore1BackgroundQueue(); }

    template <typename T>
    void logMetricImpl(TelemetryField field, T value,
                       std::unordered_map<std::string, TelemetryVariant> &data) {
        static_assert(
            std::disjunction_v<std::is_same<T, float>, std::is_same<T, uint32_t>,
                               std::is_same<T, uint16_t>, std::is_same<T, int>,
                               std::is_same<T, char *>, std::is_same<T, const char *>,
                               std::is_same<T, std::string>, std::is_same<T, SpiramString>>,
            "Unsupported type for logMetricImpl");

        // create log directory in filesystem if it doesn't exist
        if (!fileSystem_.createDir("/metrics")) {
            log_e("could not create /metrics directory");
            return;
        }

        auto timestamp = SolClock::getInstance().getEpochTime();

        std::string dataJsonFormat = mapToJson(data);
        std::string valueFormat = "";
        if constexpr (std::is_same<T, float>::value) {
            valueFormat = StringFormat(R"(,"v":%.2f)", value);
        } else if constexpr (std::is_same<T, uint32_t>::value || std::is_same<T, uint16_t>::value) {
            valueFormat = StringFormat(R"(,"v":%u)", value);
        } else if constexpr (std::is_same<T, int>::value) {
            valueFormat = StringFormat(R"(,"v":%d)", value);
        } else if constexpr (std::is_same<T, char *>::value ||
                             std::is_same<T, const char *>::value) {
            if (value && strcmp(value, "") != 0) { // Check if the C-style string is not empty
                valueFormat = StringFormat(R"(,"v":"%s")", value);
            }
        } else if constexpr (std::is_same<T, std::string>::value ||
                             std::is_same<T, SpiramString>::value) {
            if (!value.empty()) { // Check if the std::string is not empty
                valueFormat = StringFormat(R"(,"v":"%s")", value.c_str());
            }
        }

        std::string format = R"({"ts":%lld,"k":"%s"%s%s})";

        std::string logString;
        logString = StringFormat(format.c_str(), timestamp, TelemetryFieldToString(field),
                                 valueFormat.c_str(), dataJsonFormat.c_str());

        log_i("Logging metric: %s %s", TelemetryFieldToString(field), logString.c_str());

        Telemetry::dispatchQueue()->dispatch([this, logString, field]() {
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
};
