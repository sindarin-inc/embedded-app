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
        // TODO: This is incorrect, as it assumes the response looks like this:
        // {"bookmarks": [{"uuid": "123", "starting_position": 1, "ending_position": 2, "readable":
        // "123"}, ...]}
        //
        // But it should look like this:
        // [{"uuid": "123", "starting_position": 1, "ending_position": 2, "readable": "123"}, ...]
        // We just don't do anything with this yet. We'll fix it when we need it.
        for (const auto &readableBookmark : j["bookmarks"].as<JsonObject>()) {
            readableBookmarks.emplace_back();
            readableBookmarks.back().fromJson(readableBookmark.value().as<JsonObject>());
        }
        return true;
    }
};
