#include "Telemetry.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

auto TelemetryFieldToString(TelemetryField field) -> const char * {
    switch (field) {
    case METRIC_VOLTAGE_LOG:
        return "ba";
    case METRIC_READABLE_PLACE:
        return "rp";
    case METRIC_EVENT:
        return "ev";
    case METRIC_COUNTER:
        return "co";
    default:
        return "uk";
    }
}

class Telemetry {
    auto Telemetry::variantToJson(const std::variant<int, uint32_t, float, std::string> &v)
        -> std::string {
        if (std::holds_alternative<int>(v)) {
            return std::to_string(std::get<int>(v));
        } else if (std::holds_alternative<uint32_t>(v)) {
            return std::to_string(std::get<uint32_t>(v));
        } else if (std::holds_alternative<float>(v)) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << std::get<float>(v);
            return ss.str();
        } else {
            return "\"" + std::get<std::string>(v) + "\"";
        }
    }

    auto Telemetry::mapToJson(
        const std::map<std::string, std::variant<int, uint32_t, float, std::string>> &data)
        -> std::string {
        std::string json;
        for (const auto &kv : data) {
            json += StringFormat(",\"%s\":%s", kv.first.c_str(), variantToJson(kv.second).c_str());
        }
        return json;
    }
}
