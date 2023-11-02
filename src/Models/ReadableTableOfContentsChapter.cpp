#include "ReadableTableOfContentsChapter.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableTableOfContentsChapter &src, JsonVariant dst) -> bool {
    dst["title"] = src.title;
    dst["href"] = src.href;
    dst["in_subsection_href"] = src.inSubsectionHref;
    dst["txt_offset"] = src.txtOffset;
    dst["end_txt_offset"] = src.endTxtOffset;
    if (src.partialChapterSummary != nullptr) {
        dst["partial_chapter_summary"] = *src.partialChapterSummary;
    }
    auto dictionary = dst.createNestedArray("dictionary");
    for (const auto &definition : src.dictionary) {
        dictionary.add(definition);
    }

    return true;
}
