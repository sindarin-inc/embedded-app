#if ENABLE_OTA

#include "OTA.hpp"

#include <ArduinoOTA.h>

#include "sindarin-debug.h"

void OTA::setup() {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA
        .onStart([]() {
            auto command = ArduinoOTA.getCommand();
            if (command == U_FLASH) {
                log_i("OTA Start updating firmware");
            } else if (command == U_SPIFFS) {
                log_i("OTA Start updating SPIFFS");
            }
        })
        .onEnd([]() { log_i("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
            log_i("OTA Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) { log_e("OTA Error: %u", error); });
    ArduinoOTA.begin();
}

void OTA::loop() { ArduinoOTA.handle(); }

#endif
