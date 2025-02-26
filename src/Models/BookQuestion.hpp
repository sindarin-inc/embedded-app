#pragma once

#include <string>
#include <vector>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class BookQuestionContext : public JsonSerializable {
public:
    BookQuestionContext() = default;
    ~BookQuestionContext() override = default;

    SpiramString contextType;
    SpiramString name;
    SpiramString description;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("context_type") || !j.containsKey("name") ||
            !j.containsKey("description")) {
            return false;
        }
        contextType = j["context_type"].as<std::string>();
        name = j["name"].as<std::string>();
        description = j["description"].as<std::string>();
        return true;
    }
};

class BookQuestion : public JsonSerializable {
public:
    BookQuestion() = default;
    ~BookQuestion() override = default;

    SpiramString slug;
    SpiramString question;
    SpiramString description;
    std::vector<SpiramString> allowedContexts;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("slug") || !j.containsKey("question") || !j.containsKey("description") ||
            !j.containsKey("allowed_contexts")) {
            return false;
        }
        slug = j["slug"].as<std::string>();
        question = j["question"].as<std::string>();
        description = j["description"].as<std::string>();

        JsonArray contexts = j["allowed_contexts"].as<JsonArray>();
        allowedContexts.clear();
        for (const auto &context : contexts) {
            allowedContexts.emplace_back(context.as<std::string>());
        }
        return true;
    }
};

class BookQuestions : public JsonSerializable {
public:
    BookQuestions() = default;
    ~BookQuestions() override = default;

    std::vector<BookQuestion> questions;
    std::vector<BookQuestionContext> contexts;
    int version{1};

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("questions") || !j.containsKey("contexts")) {
            return false;
        }

        version = j["version"] | 1;

        JsonArray questionsArray = j["questions"].as<JsonArray>();
        questions.clear();
        for (JsonObject questionObj : questionsArray) {
            BookQuestion question;
            if (question.fromJson(questionObj)) {
                questions.push_back(question);
            }
        }

        JsonArray contextsArray = j["contexts"].as<JsonArray>();
        contexts.clear();
        for (JsonObject contextObj : contextsArray) {
            BookQuestionContext context;
            if (context.fromJson(contextObj)) {
                contexts.push_back(context);
            }
        }

        return true;
    }

    static auto loadFromJson(const std::string &jsonStr) -> BookQuestions;
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestionContext &src, JsonVariant dst) -> bool;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestion &src, JsonVariant dst) -> bool;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestions &src, JsonVariant dst) -> bool;
