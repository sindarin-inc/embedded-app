#include "Models/KindleBook.hpp"

auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["author"] = src.author;
    dst["progress"] = src.progress;
    dst["size"] = src.size;
    return true;
}