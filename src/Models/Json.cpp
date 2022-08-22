#include "Models/Json.hpp"

std::string DumpJson(const JsonVariant &v) {
    std::string json;
    serializeJson(v, json);
    return json;
}
