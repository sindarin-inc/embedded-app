#include "ReadableTableOfContentsChapter.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableTableOfContentsChapter &src, JsonVariant dst) -> bool {
    dst["title"] = src.title;
    dst["href"] = src.href;
    dst["in_subsection_href"] = src.inSubsectionHref;
    dst["order"] = src.order;
    dst["txt_offset"] = src.txtOffset;

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadableTableOfContentsChapter &dst) {
    dst.title = src["title"].as<std::string>();
    dst.href = src["href"].as<std::string>();
    dst.inSubsectionHref = src["in_subsection_href"].as<std::string>();
    dst.order = src["order"].as<uint8_t>();
    dst.txtOffset = src["txt_offset"].as<uint32_t>();
}
