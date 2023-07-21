#include "ReadableTableOfContentsChapter.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableTableOfContentsChapter &src, JsonVariant dst) -> bool {
    dst["title"] = src.title;
    dst["href"] = src.href;
    dst["in_subsection_href"] = src.inSubsectionHref;
    dst["txt_offset"] = src.txtOffset;
    dst["summary"] = src.summary;

    return true;
}
