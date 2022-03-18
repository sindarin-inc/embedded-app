// Obvious BUGS
// (1) Prev doesn't work at all
// (2) When you turn it off and back on, if you hadn't gone forward before,
// it goes to the screen you were on, but if you had gone forward, it goes
// to the next screen
// (3) When you go back to the main menu from the text, and then go back
// to the text, it returns you to the NEXT page instead of the page you were
// on (similar to the above).
//

#include "Communications/OTA.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "Displays/AllDisplays.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/RootUI.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if EINKET013TT1
DisplayEinkET013TT1 myDisplay(DISPLAY_CS, DISPLAY_OEI, DISPLAY_RST, DISPLAY_BUSY);
#endif

#if SIMULATOR
DisplaySDLSimulator myDisplay;
#endif

#if WROVER_KIT
#include "Displays/DisplayWroverLcd.hpp"
DisplayWroverLcd myDisplay;
#endif

PreferencesStore prefs;

#if USE_IR_REMOTE
#include "Inputs/IRRemoteInput.hpp"
IRRemoteInput *ir_remote_input;
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
#include "Communications/BLESindarinService.hpp"
BLESindarinService bleSindarinService;
#endif

#if USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
BLERemoteInput *bleRemoteInput;
#endif

#if USE_BLE_SERVER_SETTINGS
#include "Communications/BLESettings.hpp"
BLESettings *bleSettings;
#endif

#if USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
BLEImprov *bleWifi;
#endif

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput ttRemoteInput(prefs);
#endif

#if SIMULATOR
#include "Inputs/SimulatorRemoteInput.hpp"
SimulatorRemoteInput simulatorRemoteInput;
#endif

#if ENABLE_OTA
OTA ota;
#endif

Power power;

RootUI rootUi(prefs, power);

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
PosixFileSystem fileSystem;
#else
#include "Storage/SpiffsFileSystem.hpp"
SpiffsFileSystem fileSystem;
#endif

#if !SIMULATOR
QueueHandle_t remoteQueue = nullptr;

void RemoteLoop(void *parameter) {
    for (;;) {
        // Always have to do a check press and check release. In this case, we don't care about
        // press.
#if USE_IR_REMOTE
        ir_remote_input->check_press();
#endif

#if USE_BLE_SERVER_REMOTE
        bleRemoteInput->checkPress();
#endif

#if USE_TURN_TOUCH_REMOTE
        ttRemoteInput.checkPress();
#endif

        // We could do this better by iterating, but fine for now with only a few input devices.
        // Note that if an IR input comes in and then a BLE input comes at the same time,
        // it may only get one of them. This seems unlikely.
        RemoteCommand codeFromRelease = RemoteCommand::NONE;

#if USE_IR_REMOTE
        if (ir_remote_input->check_release()) {
            code_from_release = ir_remote_input->get_code();
        }
#endif

#if USE_BLE_SERVER_REMOTE
        if (bleRemoteInput->checkRelease()) {
            codeFromRelease = bleRemoteInput->getCode();
        }
#endif

#if USE_TURN_TOUCH_REMOTE
        if (ttRemoteInput.checkRelease()) {
            codeFromRelease = ttRemoteInput.getCode();
        }
#endif

        if (codeFromRelease != RemoteCommand::NONE) {
            xQueueSend(remoteQueue, (void *)&codeFromRelease, (TickType_t)0);
        }

        // TODO: Is there a way to idle until the exact moment we get a BT message for maximum
        // responsiveness?
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(nullptr);
}
#endif

void setup() {
    bool inputDeviceConnected = false;

    InitializePlatform();

#if !SIMULATOR
    Serial.begin(115200);
#endif

    prefs.begin();

    SHOW_HEAP();

#if EINKET013TT1
    // Turn on power to the screen
#if REVISION < 7
    digitalWrite(DISPLAY_POWER, HIGH);
#endif
    // If the saved prefs indicated a vcom, set it here.
    if (prefs.vcom.get().value() != 0) {
        myDisplay.getDriver()->setVcom(prefs.vcom.get().value());
    }

    // If the saved prefs have a backlight of zero, turn on the lights anyways
    if (prefs.backlight.get().value() == 0) {
        prefs.backlight.set(20);
    }
    SetBacklight(prefs.backlight.get().value());

#elif WROVER_KIT
    myDisplay.begin();
#endif

    switch (prefs.font.get().value()) {
    case PREFS_DEFAULT_FONT:
        myDisplay.style.setFontFamily(myDisplay.style.lucidaRegular);
        break;
    case PREFS_LUCIDA_SKINNY:
        myDisplay.style.setFontFamily(myDisplay.style.lucidaSkinny);
        break;
    case PREFS_LUCIDA_FAT:
        myDisplay.style.setFontFamily(myDisplay.style.lucidaFat);
        break;
    case PREFS_TAHOMA_REGULAR:
        myDisplay.style.setFontFamily(myDisplay.style.tahomaRegular);
        break;
    case PREFS_TAHOMA_LARGE:
        myDisplay.style.setFontFamily(myDisplay.style.tahomaLarge);
        break;
    }

    myDisplay.setup();

    DisplayRect initialRect;
    initialRect.x = prefs.rectX.get().value();
    initialRect.y = prefs.rectY.get().value();
    initialRect.width = prefs.rectW.get().value();
    initialRect.height = prefs.rectH.get().value();

    // If prefs have never been set before set them.
    if ((initialRect.width == 0) && (initialRect.height == 0) && (initialRect.x == 0) &&
        (initialRect.y == 0)) {
        initialRect.x = 33;
        initialRect.y = 33;
        initialRect.width = 190;
        initialRect.height = 190;
        prefs.rectX.set(initialRect.x);
        prefs.rectY.set(initialRect.y);
        prefs.rectW.set(initialRect.width);
        prefs.rectH.set(initialRect.height);
    }

    myDisplay.setBoundingBox(initialRect);
    myDisplay.invertDisplay(prefs.invertSetting.get().value());
    myDisplay.setLineSpacing(prefs.lineSpacing.get().value());

    SIZE_OF(myDisplay);

#if USE_IR_REMOTE
    ir_remote_input = new IRRemoteInput();
    SIZE_OF(*ir_remote_input);
    ir_remote_input->start_remote(IR_REMOTE_INPUT_PIN);
#endif

#if USE_BLE
    BLEDevice::init("Sindarin");
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    BLEManager *bleManager = BLEManager::getInstance();
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#if USE_BLE_SERVER_REMOTE
    bleRemoteInput = new BLERemoteInput();
    bleRemoteInput->configureCharacteristics(sindarinService);
#endif

#if USE_BLE_SERVER_SETTINGS
    bleSettings = new BLESettings(&prefs);
    bleSettings->configureCharacteristics(sindarinService);
#endif

    bleSindarinService.start();
#endif

#if USE_BLE_SERVER_IMPROV
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

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    bleManager->startAdvertisingIfNecessary();
#endif

#if USE_TURN_TOUCH_REMOTE
    ttRemoteInput.startRemote();
    inputDeviceConnected = ttRemoteInput.attemptConnect();
#endif

#if !DISABLE_WIFI
    ConnectWiFiIfCredentialsExist();

#if ENABLE_OTA
    ota.setup();
#endif
#endif

    auto readables = new std::vector<Readable>();
    fileSystem.begin();
    fileSystem.getReadables(readables);
    fileSystem.getDownloadedPocketReadables(readables);

    SHOW_HEAP();

    power.setup();

    rootUi.setup(&myDisplay, *readables, inputDeviceConnected, fileSystem);

    SHOW_HEAP();

    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

#if !SIMULATOR
    remoteQueue = xQueueCreate(10, sizeof(RemoteCommand));
    // Run our Bluetooth task on core 0 since it's pretty simple anyways and the rest of the
    // bluetooth stuff already runs there
    xTaskCreatePinnedToCore(RemoteLoop, "Remote", 5000, nullptr, 1, nullptr, 0);
#endif

    log_i("Setup done");
}

void loop() {
#if USE_BLE_SERVER_REMOTE && !DISABLE_WIFI
    bleWifi->loop();
#endif
    RemoteCommand command = RemoteCommand::NONE;

#if !SIMULATOR
    // TODO: Does this allow sleep/idle to happen gracefully while waiting?
    xQueueReceive(remoteQueue, &command, (TickType_t)(1000 / portTICK_PERIOD_MS));
#else
    if (simulatorRemoteInput.checkRelease()) {
        command = simulatorRemoteInput.getCode();
    }
#endif

    if (command != RemoteCommand::NONE) {
        digitalWrite(RED_LED, HIGH);
    }

    rootUi.loop(myDisplay, command);

    digitalWrite(RED_LED, LOW);

#if ENABLE_OTA
    ota.loop();
#endif

    power.loop();
}
