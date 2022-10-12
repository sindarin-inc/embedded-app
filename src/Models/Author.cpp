#include "Author.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Author &src, JsonVariant dst) -> bool {
    dst["id"] = src.id;
    dst["name"] = src.name;
    dst["description"] = src.description;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, Author &dst) {
    dst.id = src["id"].as<std::string>();
    dst.name = src["name"].as<std::string>();
    dst.description = src["description"].as<std::string>();
}
