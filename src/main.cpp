// Obvious BUGS
// (1) Prev doesn't work at all
// (2) When you turn it off and back on, if you hadn't gone forward before,
// it goes to the screen you were on, but if you had gone forward, it goes
// to the next screen
// (3) When you go back to the main menu from the text, and then go back
// to the text, it returns you to the NEXT page instead of the page you were
// on (similar to the above).
//

#include <Arduino.h>
#include <mbedtls/md.h>

#include "config.h"

#include "Communications/OTA.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "Displays/AllDisplays.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/RootUI.hpp"
#include "sindarin-debug.h"

// Required for olikraus/U8g2 font lib
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifdef EINKET013TT1
DisplayEinkET013TT1 *myDisplay;
#endif

PreferencesStore prefs;

#ifdef USE_IR_REMOTE
#include "Inputs/IRRemoteInput.hpp"
IRRemoteInput *ir_remote_input;
#endif

#if defined(USE_BLE_SERVER_REMOTE) || defined(USE_BLE_SERVER_SETTINGS)
#include "Communications/BLESindarinService.hpp"
BLESindarinService bleSindarinService;
#endif

#ifdef USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
BLERemoteInput *bleRemoteInput;
#endif

#ifdef USE_BLE_SERVER_SETTINGS
#include "Communications/BLESettings.hpp"
BLESettings *bleSettings;
#endif

#ifdef USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
BLEImprov *bleWifi;
#endif

#ifdef USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput ttRemoteInput(prefs);
#endif

#ifdef ENABLE_OTA
OTA ota;
#endif

Power power;

RootUI rootUi(prefs, power);

void setup() {
    InitializePlatform();

    Serial.begin(115200);

    SHOW_HEAP();

#ifdef EINKET013TT1
    // Turn on power to the screen
    digitalWrite(DISPLAY_POWER, HIGH);
    myDisplay = new DisplayEinkET013TT1(DISPLAY_CS, DISPLAY_OEI, DISPLAY_RST, DISPLAY_BUSY);
    myDisplay->setFocusSize(15, 15);
    myDisplay->localXOffset = prefs.localXOffset.get().value();
    myDisplay->localYOffset = prefs.localYOffset.get().value();
#endif

    SIZE_OF(*myDisplay);

#ifdef USE_IR_REMOTE
    ir_remote_input = new IRRemoteInput();
    SIZE_OF(*ir_remote_input);
    ir_remote_input->start_remote(IR_REMOTE_INPUT_PIN);
#endif

    BLEManager *bleManager = BLEManager::getInstance();

#if defined(USE_BLE_SERVER_REMOTE) || defined(USE_BLE_SERVER_SETTINGS)
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#ifdef USE_BLE_SERVER_REMOTE
    bleRemoteInput = new BLERemoteInput();
    bleRemoteInput->configureCharacteristics(sindarinService);
#endif

#ifdef USE_BLE_SERVER_SETTINGS
    bleSettings = new BLESettings(&prefs);
    bleSettings->configureCharacteristics(sindarinService);
#endif

    bleSindarinService.start();
#endif

#ifdef USE_BLE_SERVER_IMPROV
    // NOTE(john): It might make sense at some point to consolidate everything
    // as one service. However keeping these services separate for now allows us
    // to use the Improv WebBluetooth implementation for Wifi provisioning and
    // also use the Sindarin mobile app as-is after provisioning is done. If we
    // want to merge these in the future, it will be as simple as using
    // ble_wifi->configureCharacteristics(sindarinService)
    bleWifi = new BLEImprov();
    bleWifi->configureService(bleManager->server());
    bleWifi->start();
#endif

    bleManager->startAdvertisingIfNecessary();

#ifdef USE_TURN_TOUCH_REMOTE
    ttRemoteInput.startRemote();
#endif

    prefs.begin();

#ifndef DISABLE_WIFI
    ConnectWiFiIfCredentialsExist();

#ifdef ENABLE_OTA
    ota.setup();
#endif
#endif

    FileSystem::getInstance()->begin();
    auto readables = new std::vector<Readable>();
    FileSystem::getInstance()->getReadables(readables);

    SHOW_HEAP();

    power.setup();

    rootUi.setup(myDisplay, *readables);

    SHOW_HEAP();

    log_i("Setup done");
}

void loop() {
#if defined(USE_BLE_SERVER_REMOTE) && !defined(DISABLE_WIFI)
    bleWifi->loop();
#endif

// Always have to do a check press and check release. In this case, we don't care about press.
#ifdef USE_IR_REMOTE
    ir_remote_input->check_press();
#endif

#ifdef USE_BLE_SERVER_REMOTE
    bleRemoteInput->checkPress();
#endif

#ifdef USE_TURN_TOUCH_REMOTE
    ttRemoteInput.checkPress();
#endif

    // We could do this better by iterating, but fine for now with only a few input devices.
    // Note that if an IR input comes in and then a BLE input comes at the same time,
    // it may only get one of them. This seems unlikely.
    RemoteCommand codeFromRelease = RemoteCommand::NONE;

#ifdef USE_IR_REMOTE
    if (ir_remote_input->check_release()) {
        code_from_release = ir_remote_input->get_code();
    }
#endif

#ifdef USE_BLE_SERVER_REMOTE
    if (bleRemoteInput->checkRelease()) {
        codeFromRelease = bleRemoteInput->getCode();
    }
#endif

#ifdef USE_TURN_TOUCH_REMOTE
    if (ttRemoteInput.checkRelease()) {
        codeFromRelease = ttRemoteInput.getCode();
    }
#endif

    if (codeFromRelease != RemoteCommand::NONE) {
        digitalWrite(RED_LED, HIGH);
    }

    rootUi.loop(*myDisplay, codeFromRelease);

    digitalWrite(RED_LED, LOW);

#ifdef ENABLE_OTA
    ota.loop();
#endif

    power.loop();
}
