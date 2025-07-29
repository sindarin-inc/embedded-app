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
    auto sendToolOutput(const char *tool_call_id, const char *page_text, const char *original_json)
        -> int;
    auto sendRawJson(const char *json) -> int;

    // Expose ToolCalling so callers can register their own tools
    ToolCalling &toolCalling() { return toolCalling_; }

private:
    MediaSystem mediaSystem_;
    esp_webrtc_handle_t webrtc_ = NULL;
    ToolCalling toolCalling_;

    // Internal helper that logs and sends JSON over the data channel.
    auto sendCustomData(const char *json) -> int;
};
