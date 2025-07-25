#pragma once

#include <esp_peer_signaling.h>

/**
 * @brief  OpenAI signaling configuration
 *
 * @note   Details see:
 * https://platform.openai.com/docs/api-reference/realtime-sessions/create#realtime-sessions-create-voice
 */
typedef struct {
    char *token; /*!< OpenAI token */
    char *voice; /*!< Voice to select */
} openai_signaling_cfg_t;

/**
 * @brief  Get OpenAI signaling implementation
 *
 * @return
 *      - NULL    Not enough memory
 *      - Others  OpenAI signaling implementation
 */
const esp_peer_signaling_impl_t *esp_signaling_get_openai_signaling(void);
