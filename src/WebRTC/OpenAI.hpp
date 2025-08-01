#pragma once

#include <esp_webrtc.h>
#include <functional>

#include "WebRTC/MediaSystem.hpp"
#include "WebRTC/OpenAIAPI.hpp"

class OpenAI {
public:
    OpenAI() = default;
    ~OpenAI() = default;

    auto init() -> int;
    auto start(const std::string &prompt) -> int;
    auto stop() -> int;

    using ToolCallback =
        std::function<void(OpenAIAPI::Tool &tool, const char *callId, const char *output)>;
    struct ToolRecord {
        OpenAIAPI::Tool tool;
        ToolCallback callback;
    };
    void registerTool(OpenAIAPI::Tool tool, ToolCallback callback);

    auto sendText(const char *text) -> int;
    auto sendToolOutput(const char *tool_call_id, const char *page_text) -> int;
    auto sendCustomData(const char *json) -> int;

private:
    std::string prompt_;
    esp_webrtc_handle_t webrtc_ = nullptr;
    MediaSystem mediaSystem_;
    std::unordered_map<std::string, ToolRecord> toolRecords_;

    static auto webRTCDataHandler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size,
                                  void *ctx) -> int;
    static auto webRTCEventHandler(esp_webrtc_event_t *event, void *ctx) -> int;

    auto getTools() -> std::vector<OpenAIAPI::Tool>;
};
