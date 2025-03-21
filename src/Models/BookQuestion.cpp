#include "BookQuestion.hpp"

#include "Models/Json.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestionContext &src, JsonVariant dst) -> bool {
    dst["context_type"] = src.contextType;
    dst["name"] = src.name;
    dst["description"] = src.description;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestionModel &src, JsonVariant dst) -> bool {
    dst["id"] = src.id;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestion &src, JsonVariant dst) -> bool {
    dst["slug"] = src.slug;
    dst["question"] = src.question;
    dst["description"] = src.description;
    JsonArray contexts = dst.createNestedArray("allowed_contexts");
    for (const auto &context : src.allowedContexts) {
        contexts.add(context);
    }
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const BookQuestions &src, JsonVariant dst) -> bool {
    dst["version"] = src.version;
    JsonArray questions = dst.createNestedArray("questions");
    for (const auto &question : src.questions) {
        JsonObject q = questions.createNestedObject();
        convertToJson(question, q);
    }
    JsonArray contexts = dst.createNestedArray("contexts");
    for (const auto &context : src.contexts) {
        JsonObject c = contexts.createNestedObject();
        convertToJson(context, c);
    }
    // Add models array
    if (!src.models.empty()) {
        JsonArray models = dst.createNestedArray("models");
        for (const auto &model : src.models) {
            JsonObject m = models.createNestedObject();
            convertToJson(model, m);
        }
    }
    return true;
}

auto BookQuestions::loadFromJson(const std::string &jsonStr) -> BookQuestions {
    size_t contentLength = jsonStr.length();
    SpiRamJsonDocument doc(contentLength * 2);
    deserializeJson(doc, jsonStr);
    BookQuestions questions;
    questions.fromJson(doc.as<JsonObject>());
    return questions;
}
