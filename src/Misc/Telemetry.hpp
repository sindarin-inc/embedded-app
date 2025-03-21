#pragma once

#include <unordered_map>

#include "Misc/CancelFlag.hpp"
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
    METRIC_FIRMWARE_UPDATE,
    METRIC_BLE,
    METRIC_DIALOG,
};
auto TelemetryFieldToString(TelemetryField field) -> const char *;

class Telemetry {
public:
    Telemetry(FileSystem &fs, PreferencesStore &prefs) : fileSystem_(fs), prefs_(prefs) {}
    Telemetry(const Telemetry &) = delete;

    void logMetricData(TelemetryField field,
                       std::unordered_map<std::string, TelemetryVariant> data = {}) {
        logMetric(field, "", std::move(data));
    }

    template <typename T>
    void logMetric(TelemetryField field, T value,
                   std::unordered_map<std::string, TelemetryVariant> data = {}) {
        static_assert(
            std::disjunction_v<std::is_same<T, float>, std::is_same<T, uint32_t>,
                               std::is_same<T, uint16_t>, std::is_same<T, int>,
                               std::is_same<T, char *>, std::is_same<T, const char *>,
                               std::is_same<T, std::string>, std::is_same<T, SpiramString>>,
            "Unsupported type for logMetric");

        // It's difficult to capture all types T in the lambda so we convert to a string first
        std::string valueFormat;
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
        Telemetry::dispatchQueue()->dispatch(
            [this, field, valueFormat = std::move(valueFormat), data = std::move(data)]() {
            logMetricImpl(field, valueFormat, data);
        });
    }

    auto variantToJson(const TelemetryVariant &v) -> std::string;
    auto mapToJson(const std::unordered_map<std::string, TelemetryVariant> &data) -> std::string;

    static auto metricLogPath() -> std::string { return "/metrics/telemetry.jsonl"; }

    auto uploadAndResetTelemetry(FileSystem &fileSystem, bool useBle = false,
                                 const std::shared_ptr<CancelFlag> &cancel = nullptr) -> int;

protected:
    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    unsigned long lastLoggingTime_ = 0;

private:
    static auto dispatchQueue() -> DispatchQueue * { return DispatchGetCore1BackgroundQueue(); }

    void logMetricImpl(TelemetryField field, const std::string &valueFormat,
                       const std::unordered_map<std::string, TelemetryVariant> &data);
};
