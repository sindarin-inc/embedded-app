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
#include <SPIFFS.h>
#include <mbedtls/md.h>

#include "config.h"

#include "Contents/Json.hpp"
#include "Contents/Readable.hpp"
#include "Displays/AllDisplays.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Menus/Menu.hpp"
#include "Optics/AllOptics.hpp"
#include "Storage/PreferencesStore.hpp"
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

#ifdef USE_IR_REMOTE
#include "Inputs/IRRemoteInput.hpp"
IRRemoteInput *ir_remote_input;
#endif

#ifdef USE_BLE_SERVER_REMOTE
#include "Communications/BLEReadableServer.hpp"
#include "Communications/BLESettings.hpp"
#include "Communications/BLEWifi.hpp"
#include "Inputs/BLERemoteInput.hpp"
BLERemoteInput *ble_remote_input;
BLESettings *ble_settings;
BLEReadableServer *ble_readable_server;
// TODO(john): For now, grouping BLEWifi with BLERemoteInput since they're both
// BLE server services. These are separate things and it will be better to make
// them independent.
BLEWifi *ble_wifi;
#endif

#ifdef USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput *tt_remote_input;
#endif

PreferencesStore prefs;
Menu menu(prefs);

void setupDisplayAndOptics() {
#ifdef EINK154in
    my_display = new DisplayEink154in();
    SIZE_OF(*my_display);
    // my_display->set_font(u8g2_font_6x10_tf);
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
    if (Serial) Serial.begin(115200);
    SHOW_HEAP();

    // Launch SPIFFS file system
    if (!SPIFFS.begin(false, "/spiffs", 255, nullptr)) {
        DEBUG_LN("An Error has occurred while mounting SPIFFS");
    }

    SHOW_HEAP();

    SpiRamJsonDocument *manifest = new SpiRamJsonDocument(2048);
    File manifestFile = SPIFFS.open("/manifest.json");
    if (!manifestFile) {
        DEBUG_LN("Failed to open manifest file.");
    }
    deserializeJson(*manifest, manifestFile);
    manifestFile.close();

    JsonArray array = manifest->as<JsonArray>();
    for (JsonVariant v : array) {
        menu.readables.emplace_back(v["id"], v["name"], v["path"]);
    }

    SHOW_HEAP();

    setupDisplayAndOptics();

#ifdef USE_IR_REMOTE
    ir_remote_input = new IRRemoteInput();
    SIZE_OF(*ir_remote_input);
    ir_remote_input->start_remote(IR_REMOTE_INPUT_PIN);
#endif

#ifdef USE_BLE_SERVER_REMOTE
    ble_remote_input = new BLERemoteInput();
    SIZE_OF(*ble_remote_input);
    ble_remote_input->start_remote();

    ble_settings = new BLESettings(&prefs);
    ble_settings->listen_for_settings();

    ble_readable_server =
        new BLEReadableServer(&(menu.raw_renderer), &(menu.update_box), my_optics, my_display);
    ble_readable_server->setup();

    ble_wifi = new BLEWifi();
    SIZE_OF(*ble_wifi);
    ble_wifi->startService();

    BLEManager::getInstance()->startServiceAndAdvertising();
#endif

    prefs.begin();

    my_display->display_splash_screen();

    delay(500);

    menu.setup(my_display, my_optics);

    SHOW_HEAP();
}

void loop() {
#ifdef USE_BLE_SERVER_REMOTE
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
    if (tt_remote_input == nullptr) {
        tt_remote_input = new TTRemoteInput();
        tt_remote_input->start_remote();
    }
    tt_remote_input->check_press();
#endif

    // We could do this better by iterating, but fine for now with only 2 input devices.
    // Note that if an IR input comes in and then a BLE input comes at the same time,
    // it may only get one of them. This seems unlikely.
    bool got_a_release = false;
    int code_from_release = 0;

#ifdef USE_IR_REMOTE
    if (ir_remote_input->check_release()) {
        got_a_release = true;
        code_from_release = ir_remote_input->get_code();
    }
#endif

#ifdef USE_BLE_SERVER_REMOTE
    if (ble_remote_input->check_release()) {
        got_a_release = true;
        code_from_release = ble_remote_input->get_code();
    }
#endif

#ifdef USE_TURN_TOUCH_REMOTE
    if (tt_remote_input->check_release()) {
        got_a_release = true;
        code_from_release = tt_remote_input->get_code();
    }
#endif

    menu.loop(got_a_release, code_from_release);
}
