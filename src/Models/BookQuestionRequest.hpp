#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class BookQuestionRequest : public JsonSerializable {
public:
    BookQuestionRequest() = default;
    ~BookQuestionRequest() override = default;

    SpiramString question_slug;
    SpiramString context_type;
    SpiramString book_uuid;
    SpiramString chapter;
    size_t chapter_offset{0};

    auto fromJson(const JsonObject &j) -> bool override {
        return true; // Not needed for request
    }

    auto toJson(JsonObject &j) const -> bool {
        j["question_slug"] = question_slug;
        j["context_type"] = context_type;
        j["book_uuid"] = book_uuid;
        j["chapter"] = chapter;
        j["chapter_offset"] = chapter_offset;
        return true;
    }
};

class BookQuestionResponse : public JsonSerializable {
public:
    BookQuestionResponse() = default;
    ~BookQuestionResponse() override = default;

    SpiramString answer;
    bool success{false};
    SpiramString error;
    int tokens_used{0};
    float cost{0.0f};

    auto fromJson(const JsonObject &j) -> bool override {
        // If we have an answer field, consider it a success
        if (j.containsKey("answer")) {
            success = true;
            answer = j["answer"].as<std::string>();
            tokens_used = j["tokens_used"] | 0;
            cost = j["cost"] | 0.0f;
            return true;
        }

        // If we have an error field, it's a failure
        if (j.containsKey("error")) {
            success = false;
            error = j["error"].as<std::string>();
            return true;
        }

        // If we have neither answer nor error, it's invalid
        return false;
    }
};
