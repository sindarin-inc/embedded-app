#pragma once

#include <stdint.h>

typedef struct {
    uint8_t ssid[32];     /**< SSID of ESP32 soft-AP. If ssid_len field is 0, this must be a Null
                             terminated string. Otherwise, length is set according to ssid_len. */
    uint8_t password[64]; /**< Password of ESP32 soft-AP. */
    uint8_t ssid_len;     /**< Optional length of SSID field. */
    uint8_t channel;      /**< Channel of ESP32 soft-AP */
    // wifi_auth_mode_t
    //     authmode;        /**< Auth mode of ESP32 soft-AP. Do not support AUTH_WEP in soft-AP mode
    //     */
    uint8_t ssid_hidden;    /**< Broadcast SSID or not, default 0, broadcast the SSID */
    uint8_t max_connection; /**< Max number of stations allowed to connect in, default 4, max 10 */
    uint16_t beacon_interval; /**< Beacon interval which should be multiples of 100. Unit: TU(time
                                 unit, 1 TU = 1024 us). Range: 100 ~ 60000. Default value: 100 */
    // wifi_cipher_type_t
    //     pairwise_cipher; /**< pairwise cipher of SoftAP, group cipher will be derived using this.
    //                         cipher values are valid starting from WIFI_CIPHER_TYPE_TKIP, enum
    //                         values before that will be considered as invalid and default cipher
    //                         suites(TKIP+CCMP) will be used. Valid cipher suites in softAP mode
    //                         are WIFI_CIPHER_TYPE_TKIP, WIFI_CIPHER_TYPE_CCMP and
    //                         WIFI_CIPHER_TYPE_TKIP_CCMP. */
    bool ftm_responder; /**< Enable FTM Responder mode */
} wifi_ap_config_t;

/** @brief STA configuration settings for the ESP32 */
typedef struct {
    uint8_t ssid[32];     /**< SSID of target AP. */
    uint8_t password[64]; /**< Password of target AP. */
    // wifi_scan_method_t scan_method; /**< do all channel scan or fast scan */
    bool bssid_set;   /**< whether set MAC address of target AP or not. Generally,
                         station_config.bssid_set needs to be 0; and it needs to be 1 only when users
                         need to check the MAC address of the AP.*/
    uint8_t bssid[6]; /**< MAC address of target AP*/
    uint8_t
        channel; /**< channel of target AP. Set to 1~13 to scan starting from the specified channel
                    before connecting to AP. If the channel of AP is unknown, set it to 0.*/
    uint16_t listen_interval; /**< Listen interval for ESP32 station to receive beacon when
                                 WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3
                                 if set to 0. */
    // wifi_sort_method_t sort_method; /**< sort the connect AP in the list by rssi or security mode
    // */ wifi_scan_threshold_t threshold; /**< When sort_method is set, only APs which have an auth
    // mode
    //                                     that is more secure than the selected auth mode and a
    //                                     signal stronger than the minimum RSSI will be used. */
    // wifi_pmf_config_t pmf_cfg; /**< Configuration for Protected Management Frame. Will be
    // advertized
    //   in RSN Capabilities in RSN IE. */
    uint32_t rm_enabled : 1; /**< Whether Radio Measurements are enabled for the connection */
    uint32_t
        btm_enabled : 1; /**< Whether BSS Transition Management is enabled for the connection */
    uint32_t mbo_enabled : 1; /**< Whether MBO is enabled for the connection */
    uint32_t reserved : 29;   /**< Reserved for future feature set */
} wifi_sta_config_t;

typedef union {
    wifi_ap_config_t ap;   /**< configuration of AP */
    wifi_sta_config_t sta; /**< configuration of STA */
} wifi_config_t;
