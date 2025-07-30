#include "OpenAI.hpp"

#include <cJSON.h>
#include <cstring>
#include <esp_log.h>
#include <esp_peer_default.h>
#include <string>

#include "WebRTC/OpenAIAPI.hpp"
#include "WebRTC/OpenAISignaling.hpp"
#include "WebRTC/ToolCalling.hpp"

constexpr const char *TAG = "OpenAI";

#define OPENAI_API_KEY                                                                             \
    "sk-proj-hl26OTzd32-M_qo2vc6KoCFYA8iJBXTN2npOark9yyjjumE7nJcnEKuLZwUFj-"                       \
    "xtULV6FUqCpUT3BlbkFJTj6SqpAhe1un-"                                                            \
    "Hm62wWGVqwQBIQZ1Jkh95gHKLe3vszxSKG8QZW6ISIIRUyIb70vkrydSYPHoA"

int OpenAI::webRTCDataHandler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size,
                              void *ctx) {
    OpenAI *openai = static_cast<OpenAI *>(ctx);

    std::string jsonStr(reinterpret_cast<char *>(data), size);

    ESP_LOGI(TAG, "<-- %s", jsonStr.c_str());
    openai->toolCalling_.handleReceivedJson(jsonStr.c_str());
    return 0;
}

int OpenAI::webRTCEventHandler(esp_webrtc_event_t *event, void *ctx) {
    OpenAI *openai = static_cast<OpenAI *>(ctx);
    printf("====================Event %d======================\n", event->type);
    if (event->type == ESP_WEBRTC_EVENT_DATA_CHANNEL_CONNECTED) {
        // As ESP32 act as SCTP server, it does not create data channel automatically
        // Here manually create one data channel
        esp_peer_data_channel_cfg_t cfg = {
            .label = "esp_channel",
        };
        esp_peer_handle_t peer_handle = NULL;
        esp_webrtc_get_peer_connection(openai->webrtc_, &peer_handle);
        esp_peer_create_data_channel(peer_handle, &cfg);
    }
    if (event->type == ESP_WEBRTC_EVENT_DATA_CHANNEL_OPENED) {
        // Send initial prompt
        auto event = OpenAIAPI::ResponseCreateEvent{
            .response =
                {
                    .modalities = {"text", "audio"},
                    .instructions =
                        "You are helpful and have some tools installed. In the tools you have the "
                        "ability to control a light bulb and change speaker volume. Say 'How can I "
                        "help?'",
                },
        };
        cJSON *root = event.toJson();
        char *send_text = cJSON_PrintUnformatted(root);
        if (send_text) {
            openai->sendCustomData(send_text);
            free(send_text);
        }
        cJSON_Delete(root);

        // TODO: Configure tool calling here
        root = cJSON_CreateObject();
        openai->toolCalling_.generateDescriptionJson(root);
        char *json_string = cJSON_PrintUnformatted(root);
        if (json_string) {
            openai->sendCustomData(json_string);
            free(json_string);
        }
        cJSON_Delete(root);
    }
    return 0;
}

auto OpenAI::sendToolOutput(const char *tool_call_id, const char *page_text) -> int {
    if (webrtc_ == nullptr) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }

    OpenAIAPI::ConversationItemCreateEvent event = {
        .item =
            {
                .type = "function_call_output",
                .callId = tool_call_id,
                .output = page_text,
            },
    };
    cJSON *root = event.toJson();
    char *send_text = cJSON_Print(root);
    if (send_text) {
        sendCustomData(send_text);
        free(send_text);
    }
    cJSON_Delete(root);
    return 0;
}

auto OpenAI::init() -> int {
    // Build media pipelines
    mediaSystem_.build();

    // // start_webrtc();

    // TODO: Is this necessary?
    // Apparently not! Just for debugging
    // while (1) {
    //     // TODO: Is any of this needed or does media lib create its own thread?
    //     media_lib_thread_sleep(2000);
    //     // Query WebRTC status
    //     // TODO: is this just for logging? Can we skip it?
    //     if (webrtc) {
    //         esp_webrtc_query(webrtc);
    //     }
    // }
    return 0;
}

auto OpenAI::start() -> int {
    if (webrtc_ != nullptr) {
        esp_webrtc_close(webrtc_);
        webrtc_ = nullptr;
    }
    esp_peer_default_cfg_t peer_cfg = {
        .agent_recv_timeout = 500,
    };
    openai_signaling_cfg_t openai_cfg = {
        .token = OPENAI_API_KEY,
    };
    esp_webrtc_cfg_t cfg = {
        .signaling_impl = esp_signaling_get_openai_signaling(),
        .signaling_cfg =
            {
                .extra_cfg = &openai_cfg,
                .extra_size = sizeof(openai_cfg),
            },
        .peer_impl = esp_peer_get_default_impl(),
        .peer_cfg =
            {
                .audio_info =
                    {
#ifdef WEBRTC_SUPPORT_OPUS
                        .codec = ESP_PEER_AUDIO_CODEC_OPUS,
                        .sample_rate = 16000,
                        .channel = 1,
#else
                        .codec = ESP_PEER_AUDIO_CODEC_G711A,
                        .sample_rate = 16000,
#endif
                    },
                .audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
                .enable_data_channel = true,
                .manual_ch_create = true, // Disable esp_peer create data channel automatically
                .extra_cfg = &peer_cfg,
                .extra_size = sizeof(peer_cfg),
                .ctx = this,
                .on_custom_data = webRTCDataHandler,
            },
    };
    int ret = esp_webrtc_open(&cfg, &webrtc_);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to open webrtc");
        return ret;
    }
    // Set media provider
    esp_webrtc_media_provider_t media_provider = {};
    mediaSystem_.getWebRTCMediaProvider(&media_provider);
    esp_webrtc_set_media_provider(webrtc_, &media_provider);

    // Set event handler
    esp_webrtc_set_event_handler(webrtc_, webRTCEventHandler, this);

    // Start webrtc
    ret = esp_webrtc_start(webrtc_);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to start webrtc");
    }
    return ret;
}

auto OpenAI::stop() -> int {
    if (webrtc_ != nullptr) {
        esp_webrtc_handle_t handle = webrtc_;
        webrtc_ = nullptr;
        esp_webrtc_close(handle);
    }
    return 0;
}

auto OpenAI::sendText(const char *text) -> int {
    if (webrtc_ == nullptr) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }

    OpenAIAPI::ConversationItemCreateEvent event = {
        .item =
            {
                .type = "message",
                .role = "user",
                .content = {{.type = "input_text", .text = text}},
            },
    };
    int ret = 0;
    cJSON *root = event.toJson();
    char *send_text = cJSON_Print(root);
    if (send_text) {
        ret = sendCustomData(send_text);
        free(send_text);
    } else {
        ret = -1;
    }
    cJSON_Delete(root);
    return ret;
}

// -------------------------------------------------------------------------
//  Private helpers
// -------------------------------------------------------------------------

auto OpenAI::sendCustomData(const char *json) -> int {
    if (webrtc_ == nullptr) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }
    if (json == nullptr) {
        ESP_LOGE(TAG, "Attempted to send null JSON");
        return -1;
    }

    ESP_LOGI(TAG, "--> \n%s", json);
    esp_webrtc_send_custom_data(webrtc_, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL, (uint8_t *)json,
                                strlen(json));
    return 0;
}
