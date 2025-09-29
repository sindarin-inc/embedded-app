#pragma once

#include <vector>

#include "Json.hpp"
#include "KindlePageResponse.hpp"

struct KindleChapterInfo {
    SpiramString title;
    SpiramString uuid;
    int page = 0;
};

struct KindleChaptersResponse : public JsonSerializable {
    bool success = false;
    SpiramString error;
    KindlePageProgress progress;
    std::vector<KindleChapterInfo> chapters;

    auto fromJson(const JsonObject &json) -> bool override {
        if (json.containsKey("success")) {
            success = json["success"];
        }

        if (json.containsKey("error")) {
            error = json["error"].as<std::string>();
        }

        if (json.containsKey("progress") && json["progress"].is<JsonObject>()) {
            progress.fromJson(json["progress"]);
        }

        if (json.containsKey("chapters") && json["chapters"].is<JsonArray>()) {
            chapters.clear();
            JsonArray chaptersArray = json["chapters"];
            for (JsonVariant value : chaptersArray) {
                KindleChapterInfo chapter;
                if (value.containsKey("title")) {
                    chapter.title = value["title"].as<std::string>();
                }
                if (value.containsKey("uuid")) {
                    chapter.uuid = value["uuid"].as<std::string>();
                }
                if (value.containsKey("page")) {
                    chapter.page = value["page"].as<int>();
                }
                if (!chapter.title.empty()) {
                    chapters.push_back(chapter);
                }
            }
        }

        return true;
    }
};
