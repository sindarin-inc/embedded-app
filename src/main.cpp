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
#include "Optics/AllOptics.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/RootUI.hpp"
#include "sindarin-debug.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifdef EINK154in
DisplayEink154in *my_display;
#endif

#ifdef EINKET013TT1
DisplayEinkET013TT1 *my_display;
#endif

#ifdef OLED96in
DisplayOLED96in *my_display;
#endif

#ifdef SHARPMEM
DisplaySharpMem *my_display;
#endif

#ifdef PlanoConvex20x40
OpticsPlanoConvex20mmX40mm *my_optics;
#endif

#ifdef PlanoConvex16x36
OpticsPlanoConvex16mmX36mm *my_optics;
#endif

#ifdef Hastings20x40_3
OpticsHastings20mmX40_3mm *my_optics;
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
BLERemoteInput *ble_remote_input;
#endif

#ifdef USE_BLE_SERVER_SETTINGS
#include "Communications/BLESettings.hpp"
BLESettings *ble_settings;
#endif

#ifdef USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
BLEImprov *ble_wifi;
#endif

#ifdef USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput tt_remote_input(prefs);
#endif

#ifdef ENABLE_OTA
OTA ota;
#endif

Power power;

RootUI root_ui(prefs, power);

void setupDisplayAndOptics() {
#ifdef EINK154in
    my_display = new DisplayEink154in();
    SIZE_OF(*my_display);
    // my_display->set_font(u8g2_font_6x10_tf);
#endif

#ifdef EINKET013TT1
#ifdef GILPICO_SINDARIN
    // Turn on power to the screen
    digitalWrite(DISPLAY_POWER, HIGH);
    my_display = new DisplayEinkET013TT1(DISPLAY_CS, DISPLAY_OEI, DISPLAY_RST, DISPLAY_BUSY);
#endif
    SIZE_OF(*my_display);
#endif

#ifdef OLED96in
    my_display = new DisplayOLED96in();
#endif

#ifdef SHARPMEM
    my_display = new DisplaySharpMem();
#endif

#ifdef PlanoConvex20x40
    my_optics = new OpticsPlanoConvex20mmX40mm();
#endif

#ifdef PlanoConvex16x36
    my_optics = new OpticsPlanoConvex16mmX36mm();
#endif

#ifdef Hastings20x40_3
    my_optics = new OpticsHastings20mmX40_3mm();
    SIZE_OF(*my_optics);
#endif
}

void setup() {
    InitializePlatform();

    Serial.begin(115200);

    SHOW_HEAP();

    setupDisplayAndOptics();

#ifdef USE_IR_REMOTE
    ir_remote_input = new IRRemoteInput();
    SIZE_OF(*ir_remote_input);
    ir_remote_input->start_remote(IR_REMOTE_INPUT_PIN);
#endif

    BLEManager *bleManager = BLEManager::getInstance();

#if defined(USE_BLE_SERVER_REMOTE) || defined(USE_BLE_SERVER_SETTINGS)
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#ifdef USE_BLE_SERVER_REMOTE
    ble_remote_input = new BLERemoteInput();
    ble_remote_input->configureCharacteristics(sindarinService);
#endif

#ifdef USE_BLE_SERVER_SETTINGS
    ble_settings = new BLESettings(&prefs);
    ble_settings->configureCharacteristics(sindarinService);
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
    ble_wifi = new BLEImprov();
    ble_wifi->configureService(bleManager->server());
    ble_wifi->start();
#endif

    bleManager->startAdvertisingIfNecessary();

#ifdef USE_TURN_TOUCH_REMOTE
    tt_remote_input.start_remote();
#endif

    prefs.begin();

#ifndef DISABLE_WIFI
    connectWiFiIfCredentialsExist();

#ifdef ENABLE_OTA
    ota.setup();
#endif
#endif

    FileSystem::getInstance()->begin();
    auto readables = new std::vector<Readable>();
    FileSystem::getInstance()->getReadables(readables);

    SHOW_HEAP();

    power.setup();

    root_ui.setup(my_display, my_optics, *readables);

    SHOW_HEAP();

    log_i("Setup done");
}

void loop() {
#if defined(USE_BLE_SERVER_REMOTE) && !defined(DISABLE_WIFI)
    ble_wifi->loop();
#endif

// Always have to do a check press and check release. In this case, we don't care about press.
#ifdef USE_IR_REMOTE
    ir_remote_input->check_press();
#endif

#ifdef USE_BLE_SERVER_REMOTE
    ble_remote_input->check_press();
#endif

#ifdef USE_TURN_TOUCH_REMOTE
    tt_remote_input.check_press();
#endif

    // We could do this better by iterating, but fine for now with only a few input devices.
    // Note that if an IR input comes in and then a BLE input comes at the same time,
    // it may only get one of them. This seems unlikely.
    RemoteCommand code_from_release = RemoteCommand::NONE;

#ifdef USE_IR_REMOTE
    if (ir_remote_input->check_release()) {
        code_from_release = ir_remote_input->get_code();
    }
#endif

#ifdef USE_BLE_SERVER_REMOTE
    if (ble_remote_input->check_release()) {
        code_from_release = ble_remote_input->get_code();
    }
#endif

#ifdef USE_TURN_TOUCH_REMOTE
    if (tt_remote_input.check_release()) {
        code_from_release = tt_remote_input.get_code();
    }
#endif

    if (code_from_release != RemoteCommand::NONE) {
        digitalWrite(RED_LED, HIGH);
    }

    root_ui.loop(code_from_release);

    digitalWrite(RED_LED, LOW);

#ifdef ENABLE_OTA
    ota.loop();
#endif

    power.loop();
}
