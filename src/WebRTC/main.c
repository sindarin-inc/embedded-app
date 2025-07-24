/* OpenAI realtime communication test

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sys/param.h>

// #include "argtable3/argtable3.h"
#include "codec_board.h"
#include "codec_init.h"
#include "common.h"
#include "esp_capture_defaults.h"
#include "esp_codec_dev.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_webrtc.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "media_sys.h"
#include "sdkconfig.h"
#include "settings.h"

#define RUN_ASYNC(name, body)                                                                      \
    void run_async##name(void *arg) {                                                              \
        body;                                                                                      \
        media_lib_thread_destroy(NULL);                                                            \
    }                                                                                              \
    media_lib_thread_create_from_scheduler(NULL, #name, run_async##name, NULL);

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

// static int network_event_handler(bool connected) {
//     // Run async so that not block wifi event callback
//     if (connected) {
//         RUN_ASYNC(start, { start_webrtc(); });
//     } else {
//         RUN_ASYNC(stop, { stop_webrtc(); });
//     }
//     return 0;
// }

void begin_webrtc(void) {
    media_lib_add_default_adapter();
    media_lib_thread_set_schedule_cb(thread_scheduler);

    set_codec_board_type(TEST_BOARD_NAME);
    // Notes when use playback and record at same time, must set reuse_dev = false
    codec_init_cfg_t cfg = {
#if CONFIG_IDF_TARGET_ESP32S3
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
#endif
        .reuse_dev = false};
    init_codec(&cfg);

    // TODO: Pull into this initialization
    media_sys_buildup();

    start_webrtc();

    while (1) {
        media_lib_thread_sleep(2000);
        // Query WebRTC status
        if (webrtc) {
            esp_webrtc_query(webrtc);
        }
    }
}
