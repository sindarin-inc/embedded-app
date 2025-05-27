#pragma once

#include <ArduinoJson.h>
#include <string>

#include "Models/JsonSerializable.hpp"

struct KindlePageProgress {
    int percentage = 0;
    int currentPage = 0;
    int totalPages = 0;

    auto fromJson(const JsonObject &json) -> bool {
        if (json.containsKey("percentage")) {
            percentage = json["percentage"];
        }

        if (json.containsKey("current_page") && !json["current_page"].isNull()) {
            currentPage = json["current_page"];
        }

        if (json.containsKey("total_pages") && !json["total_pages"].isNull()) {
            totalPages = json["total_pages"];
        }

        return true;
    }
};

struct KindlePageResponse : public JsonSerializable {
    bool success = false;
    KindlePageProgress progress;
    SpiramString ocrText;
    SpiramString screenshotUrl;
    float timeTaken = 0.0f;
    bool lastReadDialog = false;
    SpiramString dialogText;
    bool alreadyOpen = false;

    auto fromJson(const JsonObject &json) -> bool override {
        if (json.containsKey("success")) {
            success = json["success"];
        }

        if (json.containsKey("progress") && json["progress"].is<JsonObject>()) {
            progress.fromJson(json["progress"]);
        }

        if (json.containsKey("ocr_text")) {
            ocrText = json["ocr_text"].as<std::string>();
        }

        if (json.containsKey("screenshot_url")) {
            screenshotUrl = json["screenshot_url"].as<std::string>();
        }

        if (json.containsKey("time_taken")) {
            timeTaken = json["time_taken"];
        }

        // Handle last read dialog fields
        if (json.containsKey("last_read_dialog")) {
            lastReadDialog = json["last_read_dialog"];
        }

        if (json.containsKey("dialog_text")) {
            dialogText = json["dialog_text"].as<std::string>();
        }

        if (json.containsKey("already_open")) {
            alreadyOpen = json["already_open"];
        }

        return true;
    }
};