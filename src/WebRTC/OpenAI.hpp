#pragma once

#include <esp_webrtc.h>

#include "WebRTC/MediaSystem.hpp"
#include "WebRTC/ToolCalling.hpp"

class OpenAI {
public:
    OpenAI() = default;
    ~OpenAI() = default;

    auto init() -> int;
    auto start() -> int;
    auto stop() -> int;

    auto sendText(const char *text) -> int;
    auto sendToolOutput(const char *tool_call_id, const char *page_text) -> int;
    auto sendCustomData(const char *json) -> int;

    // Expose ToolCalling so callers can register their own tools
    ToolCalling &toolCalling() { return toolCalling_; }

private:
    esp_webrtc_handle_t webrtc_ = nullptr;
    MediaSystem mediaSystem_;
    ToolCalling toolCalling_;

    static auto webRTCDataHandler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size,
                                  void *ctx) -> int;
    static auto webRTCEventHandler(esp_webrtc_event_t *event, void *ctx) -> int;
};
