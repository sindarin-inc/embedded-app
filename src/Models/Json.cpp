#include "Models/Json.hpp"

auto DumpJson(const JsonVariant &v) -> std::string {
    std::string json;
    serializeJson(v, json);
    return json;
}
