#include "ReadablePlacemark.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadablePlacemark &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    if (src.place->isFor(DocType::TXT)) {
        dst["position"] = src.place->getOffset();
    } else if (src.place->isFor(DocType::EPUB)) {
        dst["chapter"] = std::dynamic_pointer_cast<EPubPlace>(src.place)->getHRef();
        dst["chapter_offset"] = src.place->getOffset();
    } else {
        return false;
    }
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadablePlacemark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    if (src.containsKey("chapter") && src.containsKey("chapter_offset")) {
        dst.place = Place::factory(src["chapter_offset"].as<std::uint32_t>(),
                                   src["chapter"].as<std::string>());
    } else if (src.containsKey("position")) {
        dst.place = Place::factory(src["position"].as<uint32_t>());
    } else {
        dst.place = std::make_shared<NonePlace>();
    }
}
