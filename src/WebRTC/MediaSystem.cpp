/* Media system

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "MediaSystem.hpp"

#include <av_render.h>
#include <av_render_default.h>
#include <codec_board.h>
#include <codec_init.h>
#include <esp_audio_dec_default.h>
#include <esp_audio_enc_default.h>
#include <esp_capture_defaults.h>
#include <esp_capture_path_simple.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <media_lib_adapter.h>
#include <media_lib_os.h>

constexpr const char *TAG = "MediaSystem";

static void thread_scheduler(const char *thread_name, media_lib_thread_cfg_t *thread_cfg) {
    if (strcmp(thread_name, "pc_task") == 0) {
        thread_cfg->stack_size = 25 * 1024;
        thread_cfg->priority = 18;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "start") == 0) {
        thread_cfg->stack_size = 6 * 1024;
    }
    if (strcmp(thread_name, "pc_send") == 0) {
        thread_cfg->stack_size = 4 * 1024;
        thread_cfg->priority = 15;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "Adec") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "venc") == 0) {
#if CONFIG_IDF_TARGET_ESP32S3
        thread_cfg->stack_size = 20 * 1024;
#endif
        thread_cfg->priority = 10;
    }
#ifdef WEBRTC_SUPPORT_OPUS
    if (strcmp(thread_name, "aenc") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
    }
    if (strcmp(thread_name, "SrcRead") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 16;
        thread_cfg->core_id = 0;
    }
    if (strcmp(thread_name, "buffer_in") == 0) {
        thread_cfg->stack_size = 6 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 0;
    }
#endif
}

int build_media_os() {
    auto err = media_lib_add_default_adapter();
    media_lib_thread_set_schedule_cb(thread_scheduler);
    return err;
}

#define RET_ON_NULL(ptr, v)                                                                        \
    do {                                                                                           \
        if (ptr == NULL) {                                                                         \
            ESP_LOGE(TAG, "Memory allocate fail on %d", __LINE__);                                 \
            return v;                                                                              \
        }                                                                                          \
    } while (0)

int build_player_system(player_system_t *player_sys) {
    i2s_render_cfg_t i2s_cfg = {
        .play_handle = get_playback_handle(),
    };
    player_sys->audio_render = av_render_alloc_i2s_render(&i2s_cfg);
    if (player_sys->audio_render == NULL) {
        ESP_LOGE(TAG, "Fail to create audio render");
        return -1;
    }
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, 85);
    av_render_cfg_t render_cfg = {
        .audio_render = player_sys->audio_render,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    player_sys->player = av_render_open(&render_cfg);
    if (player_sys->player == NULL) {
        ESP_LOGE(TAG, "Fail to create player");
        return -1;
    }
    // When support AEC, reference data is from speaker right channel for ES8311 so must output 2
    // channel
    av_render_audio_frame_info_t aud_info = {
        .channel = 2,
        .bits_per_sample = 16,
        .sample_rate = 16000,
    };
    av_render_set_fixed_frame_info(player_sys->player, &aud_info);
    return 0;
}

int build_capture_system(capture_system_t *capture_sys) {
    capture_sys->aud_enc = esp_capture_new_audio_encoder();
    RET_ON_NULL(capture_sys->aud_enc, -1);
    // For S3 when use ES7210 it use TDM mode second channel is reference data
    esp_capture_audio_aec_src_cfg_t codec_cfg = {
        .record_handle = get_record_handle(),
#if CONFIG_IDF_TARGET_ESP32S3
        .channel = 4,
        .channel_mask = 1 | 2,
#endif
    };
    // capture_sys.aud_src = esp_capture_new_audio_codec_src(&codec_cfg);
    capture_sys->aud_src = esp_capture_new_audio_aec_src(&codec_cfg);
    RET_ON_NULL(capture_sys->aud_src, -1);
    esp_capture_simple_path_cfg_t simple_cfg = {
        .aenc = capture_sys->aud_enc,
    };
    capture_sys->path_if = esp_capture_build_simple_path(&simple_cfg);
    RET_ON_NULL(capture_sys->path_if, -1);
    // Create capture system
    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capture_sys->aud_src,
        .capture_path = capture_sys->path_if,
    };
    ESP_LOGI(TAG, "capture_sys.capture_path: %p", cfg.capture_path);
    int err = esp_capture_open(&cfg, &capture_sys->capture_handle);
    if (err != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open capture: %d", err);
        return -1;
    }
    return 0;
}

int MediaSystem::build(void) {
    build_media_os();

    // TODO: I wonder how to configure this directly without the board config file
    set_codec_board_type("ESP32_P4_DEV_V14");
    // Notes when use playback and record at same time, must set reuse_dev = false
    codec_init_cfg_t cfg = {
#if CONFIG_IDF_TARGET_ESP32S3
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
#endif
        .reuse_dev = false};
    init_codec(&cfg);

    // Register default audio encoder
    esp_audio_enc_register_default();
    // Register default audio decoder
    esp_audio_dec_register_default();
    // Build capture system
    build_capture_system(&capture_);
    // Build player system
    build_player_system(&player_);

    // TODO: Better error handling
    return 0;
}

int MediaSystem::getWebRTCMediaProvider(esp_webrtc_media_provider_t *provider) {
    provider->capture = capture_.capture_handle;
    provider->player = player_.player;
    return 0;
}
