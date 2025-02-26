#include "Models/CheckSync.hpp"

auto ReadableModelTypeToString(ReadablesModelType type) -> std::string {
    switch (type) {
    case ReadablesModelType::Article:
        return "article";
    case ReadablesModelType::Book:
        return "book";
    default:
        return "unknown";
    }
}

auto ConvertToJson(const CheckSyncRequest &src, JsonVariant dst) -> bool {
    if (src.modelType == ReadablesModelType::Book) {
        dst["books_since"] = src.since;
    } else if (src.modelType == ReadablesModelType::Article) {
        dst["articles_since"] = src.since;
    }

    auto bookmarksJson = dst.createNestedArray("bookmarks");
    for (const auto &bookmark : src.bookmarks) {
        bookmarksJson.add(bookmark);
    }
    auto placemarksJson = dst.createNestedArray("placemarks");
    for (const auto &placemark : src.placemarks) {
        placemarksJson.add(placemark);
    }
    auto archivesJson = dst.createNestedArray("archives");
    for (const auto &archive : src.archives) {
        archivesJson.add(archive);
    }
    auto storedUuidsJson = dst.createNestedArray("stored_uuids");
    for (const auto &uuid : src.storedUuids) {
        storedUuidsJson.add(uuid);
    }

    dst["sync_reason"] = static_cast<int>(src.syncReason);
    dst["battery"] = src.battery;
    dst["pt_battery"] = src.ptBattery;
    dst["pt_build"] = src.ptBuild;
    dst["build"] = src.build;
    dst["git_hash"] = src.gitHash;
    dst["sn"] = src.sn;
    dst["skip_toc"] = src.skipToc;
    dst["skip_content"] = src.skipContent;
    dst["skip_summary"] = src.skipSummary;
    dst["book_questions_version"] = src.bookQuestionsVersion;

    return true;
}
