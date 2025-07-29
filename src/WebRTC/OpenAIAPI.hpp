/// OpenAIAPI.hpp
/// Abstraction layer for building OpenAI Realtime / Function-calling JSON objects.
/// Each helper returns a freshly allocated cJSON* tree which MUST be deleted
/// by the caller using cJSON_Delete().

#pragma once

#include <cJSON.h>
#include <string>
#include <vector>

namespace OpenAIAPI {

// Build a `response.create` payload containing assistant instructions. The
// default modalities are {"text", "audio"}.
cJSON *buildResponseCreate(const std::string &instructions,
                           const std::vector<std::string> &modalities = {"text", "audio"});

// Build a `conversation.item.create` payload from plain user text. If
// `previousItemId` is empty, the field will be set to null.
cJSON *buildConversationItemCreate(const std::string &text, const std::string &previousItemId = "");

// Build a `response.create` payload that contains both the original tool call
// JSON (as the first element of the input array) and a single
// `function_call_output` entry. `originalToolJson` **must** contain a valid
// JSON string representing the original function call request.
cJSON *buildFunctionCallOutput(const std::string &originalToolJson, const std::string &callId,
                               const std::string &output,
                               const std::vector<std::string> &modalities = {"text", "audio"});

// Build the request body used when creating a realtime session via
// `POST /v1/realtime/sessions`.
cJSON *buildRealtimeSessionRequest(const std::string &model, const std::string &voice,
                                   const std::vector<std::string> &modalities = {"text", "audio"});

} // namespace OpenAIAPI