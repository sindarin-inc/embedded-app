#pragma once

#include <cJSON.h>
#include <memory>
#include <string>
#include <vector>

namespace OpenAIAPI {

// Base class for Server events
// https://platform.openai.com/docs/api-reference/realtime-server-events
class ServerEvent {
public:
    // Used when parsing server events.
    ServerEvent(cJSON *json) : json_(json) {
        type = cJSON_GetObjectItem(json_, "type")->valuestring;
        eventId = cJSON_GetObjectItem(json_, "event_id")->valuestring;
    }

    virtual ~ServerEvent() {
        if (json_) {
            cJSON_Delete(json_);
        }
    }

    const char *type;
    const char *eventId;

protected:
    // Since this is the root parsed object, and we're using it for string storage, keep the
    // reference around until we deallocate it.
    cJSON *json_;
};

class ResponseAudioTranscriptDeltaEvent : public ServerEvent {
public:
    ResponseAudioTranscriptDeltaEvent(cJSON *json) : ServerEvent(json) {
        responseId = cJSON_GetObjectItem(json_, "response_id")->valuestring;
        itemId = cJSON_GetObjectItem(json_, "item_id")->valuestring;
        outputIndex = cJSON_GetObjectItem(json_, "output_index")->valueint;
        contentIndex = cJSON_GetObjectItem(json_, "content_index")->valueint;
        delta = cJSON_GetObjectItem(json_, "delta")->valuestring;
    }

    const char *responseId;
    const char *itemId;
    int outputIndex;
    int contentIndex;
    const char *delta;
};

class ResponseFunctionCallArgumentsDoneEvent : public ServerEvent {
public:
    ResponseFunctionCallArgumentsDoneEvent(cJSON *json) : ServerEvent(json) {
        responseId = cJSON_GetObjectItem(json_, "response_id")->valuestring;
        itemId = cJSON_GetObjectItem(json_, "item_id")->valuestring;
        outputIndex = cJSON_GetObjectItem(json_, "output_index")->valueint;
        callId = cJSON_GetObjectItem(json_, "call_id")->valuestring;
        name = cJSON_GetObjectItem(json_, "name")->valuestring;
        arguments = cJSON_GetObjectItem(json_, "arguments")->valuestring;
    }

    const char *responseId;
    const char *itemId;
    int outputIndex;
    const char *callId;
    const char *name;
    const char *arguments;
};

struct ResponseCreateEvent {
    struct Response {
        const std::vector<char *> modalities;
        const char *instructions;

        cJSON *toJson() const {
            cJSON *json = cJSON_CreateObject();
            cJSON *modalitiesArray = cJSON_CreateArray();
            for (const auto &modality : modalities) {
                cJSON_AddItemToArray(modalitiesArray, cJSON_CreateString(modality));
            }
            cJSON_AddItemToObject(json, "modalities", modalitiesArray);
            cJSON_AddStringToObject(json, "instructions", instructions);
            return json;
        }
    };

    Response response;

    cJSON *toJson() const {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "type", "response.create");
        cJSON_AddItemToObject(json, "response", response.toJson());
        return json;
    }
};
struct Tool {
    struct ToolParameter {
        struct ToolParameterProperties {
            struct ToolParameterProperty {
                const char *name;
                const char *type;
                const char *description;

                cJSON *toJson() const {
                    cJSON *property = cJSON_CreateObject();
                    cJSON_AddStringToObject(property, "type", type);
                    cJSON_AddStringToObject(property, "description", description);
                    return property;
                }
            };

            std::vector<ToolParameterProperty> properties;

            cJSON *toJson() const {
                cJSON *json = cJSON_CreateObject();
                for (const auto &property : properties) {
                    cJSON_AddItemToObject(json, property.name, property.toJson());
                }
                return json;
            }
        };

        const char *type;
        const ToolParameterProperties properties;
        const std::vector<char *> required;

        cJSON *toJson() const {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "type", type);
            cJSON_AddItemToObject(json, "properties", properties.toJson());
            cJSON *requiredArray = cJSON_CreateArray();
            for (const auto &r : required) {
                cJSON_AddItemToArray(requiredArray, cJSON_CreateString(r));
            }
            cJSON_AddItemToObject(json, "required", requiredArray);
            return json;
        }
    };

    const char *name;
    const char *description;
    const std::vector<ToolParameter> parameters;

    cJSON *toJson() const {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "type", "function");
        cJSON_AddStringToObject(tool, "name", name);
        cJSON_AddStringToObject(tool, "description", description);
        // Use a single parameters object instead of an array (per OpenAI spec)
        cJSON *parametersObj =
            parameters.empty() ? cJSON_CreateObject() : parameters.front().toJson();
        cJSON_AddItemToObject(tool, "parameters", parametersObj);
        return tool;
    }
};

// https://platform.openai.com/docs/api-reference/realtime-client-events/session/update
struct SessionUpdateEvent {
    struct Session {
        std::vector<const char *> modalities;
        // If nullptr, this will be serialized as JSON null
        const char *inputAudioTranscription;
        std::vector<Tool> tools;
        const char *toolChoice;

        cJSON *toJson() const {
            cJSON *json = cJSON_CreateObject();
            // Modalities --------------------------------------------------
            cJSON *modalitiesArray = cJSON_CreateArray();
            for (const auto &m : modalities) {
                cJSON_AddItemToArray(modalitiesArray, cJSON_CreateString(m));
            }
            cJSON_AddItemToObject(json, "modalities", modalitiesArray);
            // input_audio_transcription -----------------------------------
            if (inputAudioTranscription == nullptr) {
                cJSON_AddNullToObject(json, "input_audio_transcription");
            } else {
                cJSON_AddStringToObject(json, "input_audio_transcription", inputAudioTranscription);
            }
            // Tools -------------------------------------------------------
            cJSON *toolsArray = cJSON_CreateArray();
            for (const auto &tool : tools) {
                cJSON_AddItemToArray(toolsArray, tool.toJson());
            }
            cJSON_AddItemToObject(json, "tools", toolsArray);
            // Tool choice -------------------------------------------------
            if (toolChoice) {
                cJSON_AddStringToObject(json, "tool_choice", toolChoice);
            }
            return json;
        }
    };

    Session session;

    cJSON *toJson() const {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "type", "session.update");
        cJSON_AddItemToObject(json, "session", session.toJson());
        return json;
    }
};

struct ConversationItemCreateEvent {
    struct ConversationItem {
        struct Content {
            const char *type;
            const char *text;

            cJSON *toJson() const {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", type);
                cJSON_AddStringToObject(json, "text", text);
                return json;
            }
        };

        const char *type;
        const char *role;
        const char *callId;
        const char *output;
        std::vector<Content> content;

        cJSON *toJson() const {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddStringToObject(json, "type", type);
            cJSON_AddStringToObject(json, "role", role);
            if (callId) {
                cJSON_AddStringToObject(json, "call_id", callId);
            }
            if (output) {
                cJSON_AddStringToObject(json, "output", output);
            }
            if (!content.empty()) {
                cJSON *contentArray = cJSON_CreateArray();
                for (const auto &c : content) {
                    cJSON_AddItemToArray(contentArray, c.toJson());
                }
                cJSON_AddItemToObject(json, "content", contentArray);
            }
            return json;
        }
    };

    const char *previousItemId = nullptr;
    ConversationItem item;

    cJSON *toJson() const {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "type", "conversation.item.create");
        if (previousItemId) {
            cJSON_AddStringToObject(json, "previous_item_id", previousItemId);
        }
        cJSON_AddItemToObject(json, "item", item.toJson());
        return json;
    }
};

// Request body used when creating a realtime session via `POST /v1/realtime/sessions`.
struct RealtimeSessionRequest {
    const char *model;
    const char *voice;
    const std::vector<char *> modalities;

    cJSON *toJson() const {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "model", model);
        cJSON_AddStringToObject(json, "voice", voice);
        cJSON *modalitiesArray = cJSON_CreateArray();
        for (const auto &modality : modalities) {
            cJSON_AddItemToArray(modalitiesArray, cJSON_CreateString(modality));
        }
        cJSON_AddItemToObject(json, "modalities", modalitiesArray);
        return json;
    }
};

} // namespace OpenAIAPI