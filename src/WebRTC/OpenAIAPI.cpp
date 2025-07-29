#include "WebRTC/OpenAIAPI.hpp"

#include <cJSON.h>
#include <cstring>

namespace OpenAIAPI {

static cJSON *createModalitiesArray(const std::vector<std::string> &modalities) {
    cJSON *arr = cJSON_CreateArray();
    for (const auto &m : modalities) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(m.c_str()));
    }
    return arr;
}

cJSON *buildResponseCreate(const std::string &instructions,
                           const std::vector<std::string> &modalities) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return nullptr;
    }
    cJSON_AddStringToObject(root, "type", "response.create");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "response", response);

    cJSON_AddItemToObject(response, "modalities", createModalitiesArray(modalities));
    cJSON_AddStringToObject(response, "instructions", instructions.c_str());

    return root;
}

cJSON *buildConversationItemCreate(const std::string &text, const std::string &previousItemId) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return nullptr;
    }

    cJSON_AddStringToObject(root, "type", "conversation.item.create");

    if (previousItemId.empty()) {
        cJSON_AddNullToObject(root, "previous_item_id");
    } else {
        cJSON_AddStringToObject(root, "previous_item_id", previousItemId.c_str());
    }

    // Build the nested item
    cJSON *item = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "item", item);

    cJSON_AddStringToObject(item, "type", "message");
    cJSON_AddStringToObject(item, "role", "user");

    cJSON *contentArr = cJSON_CreateArray();
    cJSON_AddItemToObject(item, "content", contentArr);

    cJSON *contentItem = cJSON_CreateObject();
    cJSON_AddStringToObject(contentItem, "type", "input_text");
    cJSON_AddStringToObject(contentItem, "text", text.c_str());

    cJSON_AddItemToArray(contentArr, contentItem);

    return root;
}

// {
//     "type": "conversation.item.create",
//     "item": {
//       "type": "function_call_output",
//       "call_id": "call_sHlR7iaFwQ2YQOqm",
//       "output": "{\"horoscope\": \"You will soon meet a new friend.\"}"
//     }
//   }

cJSON *buildFunctionCallOutput(const std::string &callId, const std::string &output,
                               const std::vector<std::string> &modalities) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return nullptr;
    }
    cJSON_AddStringToObject(root, "type", "conversation.item.create");

    cJSON *item = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "item", item);

    cJSON_AddStringToObject(item, "type", "function_call_output");
    cJSON_AddStringToObject(item, "call_id", callId.c_str());
    cJSON_AddStringToObject(item, "output", output.c_str());

    return root;
}

cJSON *buildRealtimeSessionRequest(const std::string &model, const std::string &voice,
                                   const std::vector<std::string> &modalities) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return nullptr;
    }

    cJSON_AddStringToObject(root, "model", model.c_str());
    cJSON_AddItemToObject(root, "modalities", createModalitiesArray(modalities));
    cJSON_AddStringToObject(root, "voice", voice.c_str());

    return root;
}

} // namespace OpenAIAPI