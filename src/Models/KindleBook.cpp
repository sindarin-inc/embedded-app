#include "Models/KindleBook.hpp"

auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["author"] = src.author;
    // Don't store progress, percentage, or current page in JSON anymore
    // Only store total pages as it rarely changes
    if (src.progressTotalPages >= 1) {
        dst["progress_total_pages"] = src.progressTotalPages;
    }
    dst["is_approved_for_device"] = src.isApprovedForDevice;
    dst["last_modified"] = src.lastModified;
    return true;
}
