#if !SIMULATOR
#include <driver/uart.h>
#include <esp_err.h>
#endif

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/OTA.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/BootError.hpp"
#include "Misc/BootLoopDetection.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Models/Readable.hpp"
#include "Storage/CrashReportWriter.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/RootUI.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
DisplayEinkET013TT1 myDisplay(DISPLAY_CS_LEFT, DISPLAY_CS_RIGHT, DISPLAY_RST);
#endif

PreferencesStore prefs;

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
#include "Communications/BLESindarinService.hpp"
BLESindarinService bleSindarinService;
#endif

#if USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
BLERemoteInput bleRemoteInput;
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

#include "Sensors/LightingController.hpp"
LightingController lightingController;

#if USE_MMC5603
#include "Sensors/TempleCloseSensor.hpp"
TempleCloseSensor templeCloseSensor;
#if !DISABLE_POWER_MANAGEMENT
esp_pm_lock_handle_t maglock;
#endif
#endif

#if ENABLE_OTA
OTA ota;
#endif

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
LittleFSFileSystem fileSystem;
#endif

LocalStore localStore(fileSystem, prefs);

Power power(fileSystem, prefs);
RootUI rootUi(prefs, power);

QueueHandle_t remoteQueue = nullptr;

// Resonsible for handling connecting / reconnecting to remotes
void RadioTask(void *parameter) {
    for (;;) {
#if USE_TURN_TOUCH_REMOTE
        // Skip scanning while WiFi is connected to avoid kicking off the WiFi connection
        if (!NetworkConnected()) {
            ttRemoteInput.connect();
        } else if (!ttRemoteInput.connected()) {
            log_d("Skipping Turn Touch scan while WiFi is connected");
        }
#endif

#if USE_BLE_SERVER_REMOTE
        bleWifi->loop();
#endif

#if ENABLE_OTA
        ota.loop();
#endif

// Put this with other periodic tasks so they all execute on the same wakeup
#if USE_MMC5603
        templeCloseSensor.checkTemple();
#endif
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // This happens after the delay, so that the first check can run with a faster fade.
        // TODO(john): Since LEDC doesn't work with the serial monitor connected, this is really
        // noisy in the logs
        lightingController.checkAmbientLight();
    }
    vTaskDelete(nullptr);
}

TaskHandle_t powerTaskHandle = nullptr;

#if !DISABLE_POWER_MANAGEMENT
esp_pm_config_esp32s3_t pmConfig;
#endif

void PowerTask(void *parameter) {

    for (;;) {
        // Run the power loop first so that we don't show a 0% battery reading
        power.loop();

        xTaskNotifyWait(0, 0, nullptr, 30000 / portTICK_PERIOD_MS);

#if !DISABLE_POWER_MANAGEMENT
        if (USBConnected() && pmConfig.light_sleep_enable == true) {
            // USB is connected so disable light sleep
            pmConfig.light_sleep_enable = false;
            auto err = esp_pm_configure(&pmConfig);
            if (err != ESP_OK) {
                log_e("Failed to disable light sleep with err %s", esp_err_to_name(err));
            } else {
                log_d("Disabled light sleep");
            }
        } else if (!USBConnected() && pmConfig.light_sleep_enable == false) {
            // USB is disconnected so enable light sleep
            pmConfig.light_sleep_enable = true;
            auto err = esp_pm_configure(&pmConfig);
            if (err != ESP_OK) {
                log_e("Failed to enable light sleep with err %s", esp_err_to_name(err));
            } else {
                log_d("Enabled light sleep");
            }
        }
#endif
    }
    vTaskDelete(nullptr);
}

void StatsTask(void *parameter) {
    for (;;) {
        // Spit out data on locks every 30s
        vTaskDelay(30000 / portTICK_PERIOD_MS);
#if !SIMULATOR && !DISABLE_POWER_MANAGEMENT
        if (USBConnected()) {
            esp_pm_dump_locks(stdout);
        }
#endif
    }
    vTaskDelete(nullptr);
}

void UIInnerLoop(RemoteCommand command) { rootUi.loop(myDisplay, command); }

#if CONFIG_AUTOSTART_ARDUINO
void loop() { vTaskDelay(portMAX_DELAY); };
void setup() {
#else
extern "C" void app_main(void) { // NOLINT(readability-identifier-naming)
    initArduino();
#endif

#if !SIMULATOR
    // Configure log level for all Sindarin logs
    esp_log_level_set("Sindarin", ESP_LOG_DEBUG);
#endif
    BootLoopCheck();
    std::vector<BootError> bootErrors;

#if BOARD_HAS_PSRAM
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    if (info.total_free_bytes + info.total_allocated_bytes <= 0) {
        bootErrors.push_back({.type = BootError::Type::ExternalRam});
    }
#endif

    InitializePlatform();

    log_i("SN: %s", GetSerialNumber().c_str());

    if (esp_reset_reason() == ESP_RST_POWERON) {
        // Flash all the LEDs
        digitalWrite(RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(RED_LED, LED_OFF);
        digitalWrite(GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(GREEN_LED, LED_OFF);
#ifdef BLUE_LED
        digitalWrite(BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(BLUE_LED, LED_OFF);
#endif
    }

    if (!lightingController.checkSensorAvailable()) {
        bootErrors.push_back({.type = BootError::Type::AmbientLight});
    } else {
        lightingController.begin();
    }

#if USE_MMC5603
    if (!templeCloseSensor.checkProductId()) {
        bootErrors.push_back({.type = BootError::Type::Magnetometer});
    } else {
        templeCloseSensor.setCallback([](bool open) {
            if (open) {
                lightingController.enableLighting();
            } else {
                lightingController.disableLighting();
            }
        });
    }
#endif

    if (!prefs.begin()) {
        bootErrors.push_back({.type = BootError::Type::Preferences});
    }

    SHOW_HEAP();

    // This setting must happen *before* the below vcom set from
    // preferences. Otherwise, when in dark mode, the vcom
    // will have the wrong value due to the calculation that
    // happens in this method. Clunky, for sure. But when
    // we better understand vcom and don't need to have such fine-tuned
    // settings for dark mode and vcom, we can probably clean it up then.
    myDisplay.invertDisplay(prefs.invertSetting.get().value());

#if EINKET013TT1
#ifdef DISPLAY_POWER
    // Turn on power to the screen
    digitalWrite(DISPLAY_POWER, HIGH);
#endif
    // If the saved prefs indicated a vcom, set it here.
    if (prefs.vcom.get().value() != 0) {
        myDisplay.getDriver()->setVcom(prefs.vcom.get().value());
    }
#endif

    switch (prefs.font.get().value()) {
    case PREFS_LUCIDA_REGULAR:
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

    if (!myDisplay.setup()) {
        bootErrors.push_back({.type = BootError::Type::Display});
    }

    DisplayRect initialRect;
    initialRect.x = prefs.rectX.get().value();
    initialRect.y = prefs.rectY.get().value();
    initialRect.width = prefs.rectW.get().value();
    initialRect.height = prefs.rectH.get().value();

    // If prefs have never been set before set them.
    if ((initialRect.width == 0) && (initialRect.height == 0) && (initialRect.x == 0) &&
        (initialRect.y == 0)) {
        initialRect.x = myDisplay.defaultBoundingBox.x;
        initialRect.y = myDisplay.defaultBoundingBox.y;
        initialRect.width = myDisplay.defaultBoundingBox.width;
        initialRect.height = myDisplay.defaultBoundingBox.height;
        prefs.rectX.set(initialRect.x);
        prefs.rectY.set(initialRect.y);
        prefs.rectW.set(initialRect.width);
        prefs.rectH.set(initialRect.height);
    }

    log_d("Setting bounding box %d %d %d %d", initialRect.x, initialRect.y, initialRect.width,
          initialRect.height);
    myDisplay.setBoundingBox(initialRect);
    myDisplay.setLineSpacing(prefs.lineSpacing.get().value());

    SIZE_OF(myDisplay);

#if USE_BLE
    BLEDevice::init("Sindarin");
    auto bleStatus = esp_bt_controller_get_status();
    if (bleStatus != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        bootErrors.push_back({.type = BootError::Type::BLE});
    }
#endif

#if USE_TURN_TOUCH_REMOTE || USE_BLE_SERVER_REMOTE
    auto remoteCallback = [](RemoteCommand command) {
        DispatchGetUiQueue()->dispatch([command]() { UIInnerLoop(command); });
    };
#endif

#if USE_TURN_TOUCH_REMOTE
    ttRemoteInput.setCallback(remoteCallback);
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    BLEManager *bleManager = BLEManager::getInstance();
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#if USE_BLE_SERVER_REMOTE
    bleRemoteInput.configureCharacteristics(sindarinService);
    bleRemoteInput.setCallback(remoteCallback);
#endif

#if USE_BLE_SERVER_SETTINGS
    bleSettings = new BLESettings(&prefs);
    bleSettings->configureCharacteristics(sindarinService);
#if USE_TURN_TOUCH_REMOTE
    bleSettings->attachTTRemoteInput(&ttRemoteInput);
#endif
#endif

    if (!bleSindarinService.start()) {
        bootErrors.push_back({.type = BootError::Type::BLESindarin});
    }
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
    if (!bleWifi->start()) {
        bootErrors.push_back({.type = BootError::Type::BLEWifi});
    }
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    if (!bleManager->startAdvertisingIfNecessary()) {
        bootErrors.push_back({.type = BootError::Type::BLEManagerAdvertising});
    }
#endif

#if !DISABLE_POWER_MANAGEMENT
    pmConfig.max_freq_mhz = CPU_FREQ_MHZ_HIGH;
    pmConfig.min_freq_mhz = CPU_FREQ_MHZ_LOW;
    pmConfig.light_sleep_enable = !USBConnected();
    if (esp_pm_configure(&pmConfig) != ESP_OK) {
        bootErrors.push_back({.type = BootError::Type::PowerManagement});
    }
    // There may be a magic incantation that keeps USB running through light sleep,
    // but I haven't found it yet. I've tried every power domain in esp_sleep_pd_config but it
    // didn't help
    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
        bootErrors.push_back({.type = BootError::Type::LowPowerOscillator});
    }

#ifdef USB_CONNECTED_PIN
    attachInterrupt(
        digitalPinToInterrupt(USB_CONNECTED_PIN),
        []() { xTaskNotify(powerTaskHandle, 0, eNoAction); }, CHANGE);
#endif
#endif

    // set current boot id
    auto bootCount = prefs.bootCount.get().value();
    bootCount++;
    prefs.bootCount.set(bootCount);

    if (!fileSystem.begin()) {
        bootErrors.push_back({.type = BootError::Type::FileSystem});
    } else {
        // This is a hack for now for debugging crashes while connected to the serial monitor --
        // don't save (and thus delete crashes) when connected to USB. This means the boot count
        // will be wrong once this is saved, but that's probably ok
        if (!USBConnected()) {
            // file system started
            // attempt to write any core dumps to disk for later upload
            CrashReportWriter crashReportWriter(fileSystem, bootCount - 1);
            crashReportWriter.writeCrashReportIfExists();
        }
    }

    if (!localStore.begin()) {
        bootErrors.push_back({.type = BootError::Type::LocalStore});
    }

    SHOW_HEAP();

    power.setup();

    rootUi.setup(&myDisplay, localStore, fileSystem, lightingController);

    SHOW_HEAP();

    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

    BlinkBootErrors(bootErrors);

    remoteQueue = xQueueCreate(10, sizeof(RemoteCommand));
    xTaskCreatePinnedToCore(RadioTask, "Radio", 10000, nullptr, 1, nullptr, 0);
    // xTaskCreatePinnedToCore(StatsTask, "Stats", 5000, nullptr, 0, nullptr, 1);
    xTaskCreatePinnedToCore(PowerTask, "Power", 5000, nullptr, 0, &powerTaskHandle, 1);

    // Begin fade-in on the lighting before rendering the UI
    DispatchGetUiQueue()->dispatch([]() { lightingController.checkAmbientLight(300, 500); });

    // Render the first thing
    DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::NONE); });

    GoodBootTimer();
}
