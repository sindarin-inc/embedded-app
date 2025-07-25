#pragma once

#include <esp_capture.h>
#include <esp_capture_audio_enc.h>
#include <esp_webrtc.h>

#define WEBRTC_SUPPORT_OPUS 1

typedef struct {
    audio_render_handle_t audio_render;
    av_render_handle_t player;
} player_system_t;

typedef struct {
    esp_capture_path_handle_t capture_handle;
    esp_capture_aenc_if_t *aud_enc;
    esp_capture_audio_src_if_t *aud_src;
    esp_capture_path_if_t *path_if;
} capture_system_t;

//! Responsible for setting up the media pipelines for WebRTC
class MediaSystem {
public:
    MediaSystem() = default;
    ~MediaSystem() = default;

    int build();
    int getWebRTCMediaProvider(esp_webrtc_media_provider_t *provider);

private:
    capture_system_t capture_;
    player_system_t player_;
};
