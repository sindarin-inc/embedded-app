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
    SpiramString href;
    size_t byte_offset{0};

    auto fromJson(const JsonObject &j) -> bool override {
        return true; // Not needed for request
    }

    auto toJson(JsonObject &j) const -> bool {
        j["question_slug"] = question_slug;
        j["context_type"] = context_type;
        j["href"] = href;
        j["byte_offset"] = byte_offset;
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

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("success")) {
            return false;
        }
        success = j["success"].as<bool>();

        if (success) {
            if (!j.containsKey("answer")) {
                return false;
            }
            answer = j["answer"].as<std::string>();
        } else if (j.containsKey("error")) {
            error = j["error"].as<std::string>();
        }
        return true;
    }
};
