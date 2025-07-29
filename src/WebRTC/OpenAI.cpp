#include "OpenAI.hpp"

#include <cJSON.h>
#include <cstring>
#include <esp_log.h>
#include <esp_peer_default.h>
#include <string>

#include "WebRTC/OpenAISignaling.hpp"
#include "WebRTC/ToolCalling.hpp"

constexpr const char *TAG = "OpenAI";

#define OPENAI_API_KEY                                                                             \
    "sk-proj-hl26OTzd32-M_qo2vc6KoCFYA8iJBXTN2npOark9yyjjumE7nJcnEKuLZwUFj-"                       \
    "xtULV6FUqCpUT3BlbkFJTj6SqpAhe1un-"                                                            \
    "Hm62wWGVqwQBIQZ1Jkh95gHKLe3vszxSKG8QZW6ISIIRUyIb70vkrydSYPHoA"

esp_webrtc_handle_t webrtc = NULL;

static ToolCalling *g_toolCalling = nullptr; // Global pointer used by C callbacks

static int webrtc_data_handler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size,
                               void *ctx) {
    std::string jsonStr(reinterpret_cast<char *>(data), size);

    ESP_LOGI(TAG, "Received json: %s", jsonStr.c_str());
    if (g_toolCalling) {
        g_toolCalling->handleReceivedJson(jsonStr.c_str());
    }

    cJSON *root = cJSON_Parse(jsonStr.c_str());
    if (!root) {
        return -1;
    }
    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        char *text = strstr(payload, "transcript\":");
        if (text) {
            text += strlen("transcript\":");
            char *start = strchr(text, '"');
            char *end = start ? strchr(start + 1, '"') : NULL;
            if (end) {
                start++;
                printf("Transcript: %.*s\n", (int)(end - start), start);
            }
        }
        free(payload);
    }
    cJSON_Delete(root);
    return 0;
}

static int send_response(char *text) {
    if (webrtc == NULL) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root) {
        cJSON_AddStringToObject(root, "type", "response.create");
        cJSON *response = cJSON_CreateObject();
        cJSON *modalities = cJSON_CreateArray();
        cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
        cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
        cJSON_AddItemToObject(response, "modalities", modalities);
        cJSON_AddStringToObject(response, "instructions", text);
        cJSON_AddItemToObject(root, "response", response);
    }
    // Print the initial JSON structure
    char *send_text = cJSON_Print(root);
    if (send_text) {
        printf("Begin to send json:%s\n", send_text);
        esp_webrtc_send_custom_data(webrtc, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL,
                                    (uint8_t *)send_text, strlen(send_text));
        // Clean up
        free(send_text);
    }
    cJSON_Delete(root); // Free the cJSON object
    return 0;
}

static int webrtc_event_handler(esp_webrtc_event_t *event, void *ctx) {
    printf("====================Event %d======================\n", event->type);
    if (event->type == ESP_WEBRTC_EVENT_DATA_CHANNEL_CONNECTED) {
        // As ESP32 act as SCTP server, it does not create data channel automatically
        // Here manually create one data channel
        esp_peer_data_channel_cfg_t cfg = {
            .label = "esp_channel",
        };
        esp_peer_handle_t peer_handle = NULL;
        esp_webrtc_get_peer_connection(webrtc, &peer_handle);
        esp_peer_create_data_channel(peer_handle, &cfg);
    }
    if (event->type == ESP_WEBRTC_EVENT_DATA_CHANNEL_OPENED) {
        // Send prompt here
        send_response(
            "You are helpful and have some tools installed. In the tools you have the ability to "
            "control a light bulb and change speaker volume. Say 'How can I help?");

        // TODO: Configure tool calling here
        cJSON *root = cJSON_CreateObject();
        if (g_toolCalling) {
            g_toolCalling->generateDescriptionJson(root);
        }
        char *json_string = cJSON_Print(root);
        if (json_string) {
            printf("Sending tool call description: %s\n", json_string);
            esp_webrtc_send_custom_data(webrtc, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL,
                                        (uint8_t *)json_string, strlen(json_string));
            free(json_string);
        }
        cJSON_Delete(root);
    }
    return 0;
}

int openai_send_text(const char *text) {
    if (webrtc == NULL) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root) {
        cJSON_AddStringToObject(root, "type", "conversation.item.create");
        cJSON_AddNullToObject(root, "previous_item_id");
        cJSON *item = cJSON_CreateObject();
        if (item) {
            cJSON_AddStringToObject(item, "type", "message");
            cJSON_AddStringToObject(item, "role", "user");
        }
        cJSON *contentArray = cJSON_CreateArray();
        cJSON *contentItem = cJSON_CreateObject();
        cJSON_AddStringToObject(contentItem, "type", "input_text");
        cJSON_AddStringToObject(contentItem, "text", text);
        cJSON_AddItemToArray(contentArray, contentItem);
        cJSON_AddItemToObject(item, "content", contentArray);
        // Add the item to the root object
        cJSON_AddItemToObject(root, "item", item);
    }
    // Print the initial JSON structure
    char *send_text = cJSON_Print(root);
    if (send_text) {
        printf("Begin to send json:%s\n", send_text);
        esp_webrtc_send_custom_data(webrtc, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL,
                                    (uint8_t *)send_text, strlen(send_text));
        // Clean up
        free(send_text);
    }
    cJSON_Delete(root); // Free the cJSON object
    return 0;
}

int openai_send_tool_output(const char *tool_call_id, const char *page_text) {
    if (webrtc == NULL) {
        ESP_LOGE(TAG, "WebRTC not started yet");
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    if (root) {
        // Top-level type indicating a response
        cJSON_AddStringToObject(root, "type", "response.create");

        // Build the response object
        cJSON *response = cJSON_CreateObject();

        cJSON *modalities = cJSON_CreateArray();
        cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
        cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
        cJSON_AddItemToObject(response, "modalities", modalities);

        // Build the inputs array which will contain the tool output message
        cJSON *inputs = cJSON_CreateArray();

        // Build the tool_output entry
        cJSON *tool_output = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_output, "type", "function_call_output");
        cJSON_AddStringToObject(tool_output, "call_id", tool_call_id);
        cJSON_AddStringToObject(tool_output, "output", page_text);

        // Add tool_output to inputs array
        cJSON_AddItemToArray(inputs, tool_output);

        // Attach inputs array to response object
        cJSON_AddItemToObject(response, "input", inputs);

        // Attach response to root
        cJSON_AddItemToObject(root, "response", response);
    }
    char *send_text = cJSON_Print(root);
    if (send_text) {
        printf("Sending tool output: %s\n", send_text);
        esp_webrtc_send_custom_data(webrtc, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL,
                                    (uint8_t *)send_text, strlen(send_text));
        free(send_text);
    }
    cJSON_Delete(root);
    return 0;
}

auto OpenAI::sendToolOutput(const char *tool_call_id, const char *page_text) -> int {
    return openai_send_tool_output(tool_call_id, page_text);
}

int stop_webrtc(void) {
    if (webrtc) {
        esp_webrtc_handle_t handle = webrtc;
        webrtc = NULL;
        esp_webrtc_close(handle);
    }
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
    // Set global pointer for callbacks
    g_toolCalling = &toolCalling_;
    // if (network_is_connected() == false) {
    //     ESP_LOGE(TAG, "Wifi not connected yet");
    //     return -1;
    // }
    if (webrtc) {
        esp_webrtc_close(webrtc);
        webrtc = NULL;
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
                .on_custom_data = webrtc_data_handler,
            },
    };
    int ret = esp_webrtc_open(&cfg, &webrtc);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to open webrtc");
        return ret;
    }
    // Set media provider
    esp_webrtc_media_provider_t media_provider = {};
    mediaSystem_.getWebRTCMediaProvider(&media_provider);
    esp_webrtc_set_media_provider(webrtc, &media_provider);

    // Set event handler
    esp_webrtc_set_event_handler(webrtc, webrtc_event_handler, NULL);

    // Start webrtc
    ret = esp_webrtc_start(webrtc);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to start webrtc");
    }
    return ret;
}

auto OpenAI::stop() -> int { return stop_webrtc(); }

auto OpenAI::sendText(const char *text) -> int { return openai_send_text(text); }

int openai_send_raw_json(const char *json) {
    if (webrtc == NULL) {
        return -1;
    }
    printf("Sending raw json: %s\n", json);
    esp_webrtc_send_custom_data(webrtc, ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL, (uint8_t *)json,
                                strlen(json));
    return 0;
}

auto OpenAI::sendRawJson(const char *json) -> int { return openai_send_raw_json(json); }
