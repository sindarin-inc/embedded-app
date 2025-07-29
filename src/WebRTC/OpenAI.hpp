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
    auto sendRawJson(const char *json) -> int;

    // Expose ToolCalling so callers can register their own tools
    ToolCalling &toolCalling() { return toolCalling_; }

private:
    MediaSystem mediaSystem_;
    esp_webrtc_handle_t webrtc_ = NULL;
    ToolCalling toolCalling_;
};
