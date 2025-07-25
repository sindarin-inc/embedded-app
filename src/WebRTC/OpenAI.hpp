#include <esp_webrtc.h>

#include "WebRTC/MediaSystem.hpp"

class OpenAI {
public:
    OpenAI() = default;
    ~OpenAI() = default;

    auto init() -> int;
    auto start() -> int;
    auto stop() -> int;

    auto sendText(const char *text) -> int;

private:
    MediaSystem mediaSystem_;
    esp_webrtc_handle_t webrtc_ = NULL;
};
