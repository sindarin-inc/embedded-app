#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class SyncBookmarksResponse : JsonSerializable {
public:
    SyncBookmarksResponse() = default;
    ~SyncBookmarksResponse() override = default;

    std::vector<ReadableBookmark, SpiramAllocator<ReadableBookmark>> readableBookmarks;

    auto fromJson(const JsonObject &j) -> bool override {
        readableBookmarks.clear();
        for (const auto &readableBookmark : j["bookmarks"].as<JsonArray>()) {
            readableBookmarks.emplace_back();
            readableBookmarks.back().fromJson(readableBookmark);
        }
        return true;
    }
};
