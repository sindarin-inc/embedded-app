#include "Telemetry.hpp"

#include <iomanip>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/LocalStore.hpp"
#include "sindarin-debug.h"

auto TelemetryFieldToString(TelemetryField field) -> const char * {
    switch (field) {
    case METRIC_BATTERY_VOLTAGE:
        return "ba";
    case METRIC_READABLE_PLACE:
        return "rp";
    case METRIC_READABLE_OPEN:
        return "ro";
    case METRIC_SET_PLACEMARK:
        return "sp";
    case METRIC_EVENT:
        return "ev";
    case METRIC_SYNC_STATUS:
        return "sy";
    case METRIC_BOOT:
        return "bo";
    case METRIC_CLOCK_SET:
        return "cl";
    case METRIC_SHUTDOWN:
        return "sd";
    case METRIC_WIFI:
        return "wf";
    case METRIC_LIGHTING:
        return "li";
    case METRIC_REPORT:
        return "rt";
    case METRIC_STYLE:
        return "st";
    }
    // Explicitly don't have a `default` case so the compiler can warn us when we have unhandled
    // enum values
    return "??";
}

auto Telemetry::variantToJson(const TelemetryVariant &v) -> std::string {
    if (std::holds_alternative<int>(v)) {
        return std::to_string(std::get<int>(v));
    } else if (std::holds_alternative<uint32_t>(v)) {
        return std::to_string(std::get<uint32_t>(v));
    } else if (std::holds_alternative<uint16_t>(v)) {
        return std::to_string(std::get<uint16_t>(v));
    } else if (std::holds_alternative<float>(v)) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << std::get<float>(v);
        return ss.str();
    } else if (std::holds_alternative<SpiramString>(v)) {
        return "\"" + std::string(std::get<SpiramString>(v)) + "\"";
    } else if (std::holds_alternative<const char *>(v)) {
        return "\"" + std::string(std::get<const char *>(v)) + "\"";
    } else {
        return "\"" + std::get<std::string>(v) + "\"";
    }
}

auto Telemetry::mapToJson(const std::map<std::string, TelemetryVariant> &data) -> std::string {
    std::string json;
    for (const auto &kv : data) {
        json += StringFormat(",\"%s\":%s", kv.first.c_str(), variantToJson(kv.second).c_str());
    }
    return json;
}

auto Telemetry::uploadAndResetTelemetry(FileSystem &fileSystem, Telemetry &telemetry) -> int {
    auto localPath = telemetry.metricLogPath();
    if (!fileSystem.exists(localPath)) {
        log_w("Log file %s does not exist", localPath.c_str());
        return -1;
    }

    auto file = fileSystem.open(localPath, FILE_READ);
    if (!file) {
        log_w("Failed to open log file %s", localPath.c_str());
        return -1;
    }

    int httpCode = SindarinAPI::getInstance()->uploadTelemetryFile(file, localPath, fileSystem);
    file.close();

    log_d("Telemetry log upload http code: %d", httpCode);

    // On successful telemetry log upload, delete the logs
    if (httpCode == 201) {
        log_d("Deleting %s", localPath.c_str());
        Telemetry::dispatchQueue()->dispatch(
            [&fileSystem, localPath]() { fileSystem.remove(localPath.c_str()); });
    } else {
        log_e("Failed to upload telemetry log: %s http code: %d", localPath.c_str(), httpCode);
    }

    return httpCode;
}
