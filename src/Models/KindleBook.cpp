#include "Models/KindleBook.hpp"

auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["author"] = src.author;
    dst["progress"] = src.progress;
    dst["progress_percent"] = src.progressPercent;
    if (src.progressCurrentPage >= 1) {
        dst["progress_current_page"] = src.progressCurrentPage;
    }
    if (src.progressTotalPages >= 1) {
        dst["progress_total_pages"] = src.progressTotalPages;
    }
    dst["is_approved_for_device"] = src.isApprovedForDevice;
    dst["last_modified"] = src.lastModified;
    return true;
}
