#include "Author.hpp"

bool convertToJson(const Author &src, JsonVariant dst) {
    dst["id"] = src.id;
    dst["name"] = src.name;
    dst["description"] = src.description;
    return true;
}

void convertFromJson(JsonVariantConst src, Author &dst) {
    dst.id = src["id"].as<int>();
    dst.name = src["name"].as<std::string>();
    dst.description = src["description"].as<std::string>();
}
