#pragma once

#include <ArduinoJson.h>
#include <string>

#include "Models/JsonSerializable.hpp"

// Error type constants from server responses
static constexpr const char *KINDLE_ERROR_SESSION_KEY_MISMATCH = "session_key_mismatch";

struct KindlePageProgress {
    int currentPage = 0;
    int totalPages = 0;
    bool hasCurrentPage = false;
    bool hasTotalPages = false;

    auto fromJson(const JsonObject &json) -> bool {
        if (json.containsKey("current_page") && !json["current_page"].isNull()) {
            currentPage = json["current_page"];
            hasCurrentPage = true;
        }

        if (json.containsKey("total_pages") && !json["total_pages"].isNull()) {
            totalPages = json["total_pages"];
            hasTotalPages = true;
        }

        return true;
    }
};

struct KindlePageResponse : public JsonSerializable {
    bool success = false;
    bool authenticated = true;
    SpiramString message;
    KindlePageProgress progress;
    SpiramString ocrText;
    SpiramString screenshotUrl;
    float timeTaken = 0.0f;
    bool lastReadDialog = false;
    SpiramString dialogText;
    bool alreadyOpen = false;
    SpiramString bookSessionKey;
    SpiramString text;
    int navigateTo = -1;
    int previewTo = -1;
    SpiramString error; // Error type from server (e.g., KINDLE_ERROR_SESSION_KEY_MISMATCH)
    SpiramString currentSessionKey; // New session key from server on mismatch
    int currentPosition = 0;        // Current position from server on mismatch

    auto fromJson(const JsonObject &json) -> bool override {
        if (json.containsKey("success")) {
            success = json["success"];
        }

        if (json.containsKey("authenticated")) {
            authenticated = json["authenticated"];
        }

        if (json.containsKey("message")) {
            message = json["message"].as<std::string>();
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

        if (json.containsKey("book_session_key")) {
            bookSessionKey = json["book_session_key"].as<std::string>();
        }

        if (json.containsKey("text")) {
            text = json["text"].as<std::string>();
        }

        if (json.containsKey("navigate_to")) {
            navigateTo = json["navigate_to"];
        }

        if (json.containsKey("preview_to")) {
            previewTo = json["preview_to"];
        }

        if (json.containsKey("error")) {
            error = json["error"].as<std::string>();
        }

        if (json.containsKey("current_session_key")) {
            currentSessionKey = json["current_session_key"].as<std::string>();
        }

        if (json.containsKey("current_position")) {
            currentPosition = json["current_position"];
        }

        return true;
    }
};
