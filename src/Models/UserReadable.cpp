#include "UserReadable.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;

    if (src.txtPlace->isFor(DocType::TXT)) {
        dst["placemark"] = src.txtPlace->getOffset();
    }

    if (src.ePubPlace->isFor(DocType::EPUB)) {
        dst["chapter_offset"] = src.txtPlace->getOffset();
        dst["chapter"] = std::dynamic_pointer_cast<EPubPlace>(src.ePubPlace)->getHRef();
    }

    dst["is_archived"] = src.isArchived;
    dst["readable"] = *src.readable;

    return true;
}
