//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.cpp

    \brief      WiFi ESP32 Wireless driver RTOS Task and Events

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESPTask.hpp" // File Header

#include "WiFiESP.hpp" // ESP Class
#include "config.h"
#include "sdkconfig.h"
#include "sindarin-debug.h"
// Std Libs
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
// Base Wifi
#include "IPAddress.h"
#include "IPv6Address.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
// ESP Libs
#include "esp32-hal.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wpa2.h"
#include "nvs_flash.h"
// RTOS Lib
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

// Define the Wifi Events
ESP_EVENT_DEFINE_BASE(WIFIESP_TASK_EVENTS);

//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
// Task Handler
static TaskHandle_t _wifiesp_event_task_handle = nullptr;
// Event Queue
static xQueueHandle _wifiesp_event_queue;
// Event Group
static EventGroupHandle_t _wifiesp_event_group = nullptr;

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetAuthModeStr

    \brief  Get the string for the authentication mode
    \param  authmode - mode
    \return const char * - name
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static auto getAuthModeStr(int authmode) -> const char * {
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        return ("OPEN");
        break;
    case WIFI_AUTH_WEP:
        return ("WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        return ("PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        return ("WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        return ("WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return ("WPA2_ENTERPRISE");
        break;
    default:
        break;
    }
    return ("UNKNOWN");
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_PostEvent

    \brief  Post event to wifi task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskPostEvent(wifiesp_event_t *data) -> esp_err_t {
    if (data == nullptr) {
        return ESP_FAIL;
    }
    auto *event = (wifiesp_event_t *)malloc(sizeof(wifiesp_event_t));
    if (event == nullptr) {
        log_e("WiFi ESP Event Malloc Failed!");
        return ESP_FAIL;
    }
    memcpy(event, data, sizeof(wifiesp_event_t));
    if (xQueueSend(_wifiesp_event_queue, &event, portMAX_DELAY) != pdPASS) {
        log_e("WiFi ESP Event Send Failed!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_EventTask

    \brief  The wifi event task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static void wiFiEspTaskEventTask(void *arg) {
    wifiesp_event_t *data = nullptr;
    for (;;) {
        if (xQueueReceive(_wifiesp_event_queue, &data, portMAX_DELAY) == pdTRUE) {
            CWiFiESP::eventCallback(data);
            free(data);
            data = nullptr;
        }
    }
    vTaskDelete(nullptr);
    _wifiesp_event_task_handle = nullptr;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_EventCB

    \brief  Event Callback
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static void wiFiEspTaskEventCb(void *arg, esp_event_base_t eventBase, int32_t eventId,
                               void *eventData) {
    wifiesp_event_t wifiespEvent;
    wifiespEvent.eventId = WIFIESP_EVENT_MAX;

    /*
     * STA
     * */
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        log_v("STA Started");
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_START;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_STOP) {
        log_v("STA Stopped");
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_STOP;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
        auto *event = (wifi_event_sta_authmode_change_t *)eventData;
        log_v("STA Auth Mode Changed: From: %s, To: %s", getAuthModeStr(event->old_mode),
              getAuthModeStr(event->new_mode));
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_AUTHMODE_CHANGE;
        memcpy(&wifiespEvent.eventInfo.wifiStaAuthmodeChange, eventData,
               sizeof(wifi_event_sta_authmode_change_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_CONNECTED) {
        auto *event = (wifi_event_sta_connected_t *)eventData;
        log_v("STA Connected: SSID: %s, BSSID: " MACSTR ", Channel: %u, Auth: %s", event->ssid,
              MAC2STR(event->bssid), event->channel, getAuthModeStr(event->authmode));
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_CONNECTED;
        memcpy(&wifiespEvent.eventInfo.wifiStaConnected, eventData,
               sizeof(wifi_event_sta_connected_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        auto *event = (wifi_event_sta_disconnected_t *)eventData;
        log_v("STA Disconnected: SSID: %s, BSSID: " MACSTR ", Reason: %u", event->ssid,
              MAC2STR(event->bssid), event->reason);
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_DISCONNECTED;
        memcpy(&wifiespEvent.eventInfo.wifiStaDisconnected, eventData,
               sizeof(wifi_event_sta_disconnected_t));
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *)eventData;
        log_v("STA Got %sIP:" IPSTR, event->ip_changed ? "New " : "Same ",
              IP2STR(&event->ip_info.ip));
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_GOT_IP;
        memcpy(&wifiespEvent.eventInfo.gotIp, eventData, sizeof(ip_event_got_ip_t));
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_LOST_IP) {
        log_v("STA IP Lost");
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_LOST_IP;

        /*
         * SCAN
         * */
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_SCAN_DONE) {
        auto *event = (wifi_event_sta_scan_done_t *)eventData;
        log_v("SCAN Done: ID: %u, Status: %u, Results: %u", event->scan_id, event->status,
              event->number);
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_SCAN_DONE;
        memcpy(&wifiespEvent.eventInfo.wifiScanDone, eventData, sizeof(wifi_event_sta_scan_done_t));

        /*
         * AP
         * */
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_START) {
        log_v("AP Started");
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_START;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STOP) {
        log_v("AP Stopped");
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_STOP;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_PROBEREQRECVED) {
        auto *event = (wifi_event_ap_probe_req_rx_t *)eventData;
        log_v("AP Probe Request: RSSI: %d, MAC: " MACSTR, event->rssi, MAC2STR(event->mac));
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_PROBEREQRECVED;
        memcpy(&wifiespEvent.eventInfo.wifiApProbereqrecved, eventData,
               sizeof(wifi_event_ap_probe_req_rx_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STACONNECTED) {
        auto *event = (wifi_event_ap_staconnected_t *)eventData;
        log_v("AP Station Connected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_STACONNECTED;
        memcpy(&wifiespEvent.eventInfo.wifiApStaconnected, eventData,
               sizeof(wifi_event_ap_staconnected_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_AP_STADISCONNECTED) {
        auto *event = (wifi_event_ap_stadisconnected_t *)eventData;
        log_v("AP Station Disconnected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_STADISCONNECTED;
        memcpy(&wifiespEvent.eventInfo.wifiApStadisconnected, eventData,
               sizeof(wifi_event_ap_stadisconnected_t));
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_AP_STAIPASSIGNED) {
        auto *event = (ip_event_ap_staipassigned_t *)eventData;
        log_v("AP Station IP Assigned:" IPSTR, IP2STR(&event->ip));
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_STAIPASSIGNED;
        memcpy(&wifiespEvent.eventInfo.wifiApStaipassigned, eventData,
               sizeof(ip_event_ap_staipassigned_t));

        /*
         * ETH
         * */
    } else if (eventBase == ETH_EVENT && eventId == ETHERNET_EVENT_CONNECTED) {
        log_v("Ethernet Link Up");
        esp_eth_handle_t ethHandle = *(esp_eth_handle_t *)eventData;
        (void)ethHandle;
        wifiespEvent.eventId = WIFIESP_EVENT_ETH_CONNECTED;
        memcpy(&wifiespEvent.eventInfo.ethConnected, eventData, sizeof(esp_eth_handle_t));
    } else if (eventBase == ETH_EVENT && eventId == ETHERNET_EVENT_DISCONNECTED) {
        log_v("Ethernet Link Down");
        wifiespEvent.eventId = WIFIESP_EVENT_ETH_DISCONNECTED;
    } else if (eventBase == ETH_EVENT && eventId == ETHERNET_EVENT_START) {
        log_v("Ethernet Started");
        wifiespEvent.eventId = WIFIESP_EVENT_ETH_START;
    } else if (eventBase == ETH_EVENT && eventId == ETHERNET_EVENT_STOP) {
        log_v("Ethernet Stopped");
        wifiespEvent.eventId = WIFIESP_EVENT_ETH_STOP;
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_ETH_GOT_IP) {
        auto *event = (ip_event_got_ip_t *)eventData;
        log_v("Ethernet got %sip:" IPSTR, event->ip_changed ? "new" : "",
              IP2STR(&event->ip_info.ip));
        wifiespEvent.eventId = WIFIESP_EVENT_ETH_GOT_IP;
        memcpy(&wifiespEvent.eventInfo.gotIp, eventData, sizeof(ip_event_got_ip_t));

        /*
         * IPv6
         * */
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_GOT_IP6) {
        auto *event = (ip_event_got_ip6_t *)eventData;
        esp_interface_t iface = CWiFiESP::getEspNetifInterface(event->esp_netif);
        log_v("IF[%d] Got IPv6: IP Index: %d, Zone: %d, " IPV6STR, iface, event->ip_index,
              event->ip6_info.ip.zone, IPV62STR(event->ip6_info.ip));
        memcpy(&wifiespEvent.eventInfo.gotIp6, eventData, sizeof(ip_event_got_ip6_t));
        if (iface == ESP_IF_WIFI_STA) {
            wifiespEvent.eventId = WIFIESP_EVENT_WIFI_STA_GOT_IP6;
        } else if (iface == ESP_IF_WIFI_AP) {
            wifiespEvent.eventId = WIFIESP_EVENT_WIFI_AP_GOT_IP6;
        } else if (iface == ESP_IF_ETH) {
            wifiespEvent.eventId = WIFIESP_EVENT_ETH_GOT_IP6;
        }

        /*
         * WPS
         * */
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
        wifiespEvent.eventId = WIFIESP_EVENT_WPS_ER_SUCCESS;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_WPS_ER_FAILED) {
        auto *event = (wifi_event_sta_wps_fail_reason_t *)eventData;
        (void)event;
        wifiespEvent.eventId = WIFIESP_EVENT_WPS_ER_FAILED;
        memcpy(&wifiespEvent.eventInfo.wpsFailReason, eventData,
               sizeof(wifi_event_sta_wps_fail_reason_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
        wifiespEvent.eventId = WIFIESP_EVENT_WPS_ER_TIMEOUT;
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_WPS_ER_PIN) {
        auto *event = (wifi_event_sta_wps_er_pin_t *)eventData;
        (void)event;
        wifiespEvent.eventId = WIFIESP_EVENT_WPS_ER_PIN;
        memcpy(&wifiespEvent.eventInfo.wpsErPin, eventData, sizeof(wifi_event_sta_wps_er_pin_t));
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
        wifiespEvent.eventId = WIFIESP_EVENT_WPS_ER_PBC_OVERLAP;

        /*
         * FTM
         * */
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_FTM_REPORT) {
        auto *event = (wifi_event_ftm_report_t *)eventData;
        (void)event;
        wifiespEvent.eventId = WIFIESP_EVENT_WIFI_FTM_REPORT;
        memcpy(&wifiespEvent.eventInfo.wifiFtmReport, eventData, sizeof(wifi_event_ftm_report_t));

        /*
         * SMART CONFIG
         * */
    } else if (eventBase == SC_EVENT && eventId == SC_EVENT_SCAN_DONE) {
        log_v("SC Scan Done");
        wifiespEvent.eventId = WIFIESP_EVENT_SC_SCAN_DONE;
    } else if (eventBase == SC_EVENT && eventId == SC_EVENT_FOUND_CHANNEL) {
        log_v("SC Found Channel");
        wifiespEvent.eventId = WIFIESP_EVENT_SC_FOUND_CHANNEL;
    } else if (eventBase == SC_EVENT && eventId == SC_EVENT_GOT_SSID_PSWD) {
        auto *event = (smartconfig_event_got_ssid_pswd_t *)eventData;
        log_v("SC: SSID: %s, Password: %s", (const char *)event->ssid,
              (const char *)event->password);
        wifiespEvent.eventId = WIFIESP_EVENT_SC_GOT_SSID_PSWD;
        memcpy(&wifiespEvent.eventInfo.scGotSsidPswd, eventData,
               sizeof(smartconfig_event_got_ssid_pswd_t));

    } else if (eventBase == SC_EVENT && eventId == SC_EVENT_SEND_ACK_DONE) {
        log_v("SC Send Ack Done");
        wifiespEvent.eventId = WIFIESP_EVENT_SC_SEND_ACK_DONE;

        /*
         * Provisioning
         * */
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_INIT) {
        log_v("Provisioning Initialized!");
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_INIT;
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_DEINIT) {
        log_v("Provisioning Uninitialized!");
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_DEINIT;
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_START) {
        log_v("Provisioning Start!");
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_START;
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_END) {
        log_v("Provisioning End!");
        wifi_prov_mgr_deinit();
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_END;
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_CRED_RECV) {
        auto *event = (wifi_sta_config_t *)eventData;
        log_v("Provisioned Credentials: SSID: %s, Password: %s", (const char *)event->ssid,
              (const char *)event->password);
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_CRED_RECV;
        memcpy(&wifiespEvent.eventInfo.provCredRecv, eventData, sizeof(wifi_sta_config_t));
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_CRED_FAIL) {
        auto *reason = (wifi_prov_sta_fail_reason_t *)eventData;
        log_e("Provisioning Failed: Reason : %s",
              (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Authentication Failed" : "AP Not Found");
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_CRED_FAIL;
        memcpy(&wifiespEvent.eventInfo.provFailReason, eventData,
               sizeof(wifi_prov_sta_fail_reason_t));
    } else if (eventBase == WIFI_PROV_EVENT && eventId == WIFI_PROV_CRED_SUCCESS) {
        log_v("Provisioning Success!");
        wifiespEvent.eventId = WIFIESP_EVENT_PROV_CRED_SUCCESS;
    }

    if (wifiespEvent.eventId < WIFIESP_EVENT_MAX) {
        WiFiEspTaskPostEvent(&wifiespEvent);
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_StartNetworkEventTask

    \brief  Start the Network even task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskStartNetworkEventTask() -> bool {
    if (!_wifiesp_event_group) {
        _wifiesp_event_group = xEventGroupCreate();
        if (!_wifiesp_event_group) {
            log_e("Network Event Group Create Failed!");
            return false;
        }
        xEventGroupSetBits(_wifiesp_event_group, WIFIESP_EVENTSTATUSBIT_WIFI_DNS_IDLE);
    }
    if (!_wifiesp_event_queue) {
        _wifiesp_event_queue = xQueueCreate(32, sizeof(wifiesp_event_t *));
        if (!_wifiesp_event_queue) {
            log_e("Network Event Queue Create Failed!");
            return false;
        }
    }

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        log_e("esp_event_loop_create_default failed!");
        return err;
    }

    if (!_wifiesp_event_task_handle) {
        xTaskCreateUniversal(wiFiEspTaskEventTask, "wifiesp_events", WIFIESP_TASK_STACKSIZE,
                             nullptr, ESP_TASKD_EVENT_PRIO - 1, &_wifiesp_event_task_handle,
                             WIFIESP_TASK_CORENUMBER);
        if (!_wifiesp_event_task_handle) {
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }

    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wiFiEspTaskEventCb,
                                            nullptr, nullptr)) {
        log_e("event_handler_instance_register for WIFI_EVENT Failed!");
        return false;
    }

    if (esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wiFiEspTaskEventCb,
                                            nullptr, nullptr)) {
        log_e("event_handler_instance_register for IP_EVENT Failed!");
        return false;
    }

    if (esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &wiFiEspTaskEventCb,
                                            nullptr, nullptr)) {
        log_e("event_handler_instance_register for SC_EVENT Failed!");
        return false;
    }

    if (esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &wiFiEspTaskEventCb,
                                            nullptr, nullptr)) {
        log_e("event_handler_instance_register for ETH_EVENT Failed!");
        return false;
    }

    if (esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wiFiEspTaskEventCb,
                                            nullptr, nullptr)) {
        log_e("event_handler_instance_register for WIFI_PROV_EVENT Failed!");
        return false;
    }

    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_SetEventStatusBits

    \brief  Set Event Status Bit
    \param  bits - bits to set
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskSetEventStatusBits(int bits) -> int {
    if (!_wifiesp_event_group) {
        return 0;
    }
    return xEventGroupSetBits(_wifiesp_event_group, bits);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_ClearEventStatusBits

    \brief  Clear Event Status Bit
    \param  bits - bits to clear
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskClearEventStatusBits(int bits) -> int {
    if (!_wifiesp_event_group) {
        return 0;
    }
    return xEventGroupClearBits(_wifiesp_event_group, bits);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_GetEventStatusBits

    \brief  Get Event Status Bit
    \param  N/A
    \return Current Bits Set
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskGetEventStatusBits() -> int {
    if (!_wifiesp_event_group) {
        return 0;
    }
    return xEventGroupGetBits(_wifiesp_event_group);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_WaitEventStatusBits

    \brief  Wait for an Event Status Bit
    \param  bits - bits to wait for
    \param  timeout_ms - timeout in milliseconds
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto WiFiEspTaskWaitEventStatusBits(int bits, uint32_t timeoutMs) -> int {
    if (!_wifiesp_event_group) {
        return 0;
    }
    return xEventGroupWaitBits(_wifiesp_event_group, // The event group being tested.
                               bits,                 // The bits within the event group to wait for.
                               pdFALSE, // BIT_0 and BIT_4 should be cleared before returning.
                               pdTRUE,  // Don't wait for both bits, either bit will do.
                               timeoutMs / portTICK_PERIOD_MS) &
           bits; // Wait a maximum of 100ms for either bit to be set.
}
