#if !SIMULATOR
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_log.h>
#include <soc/rtc.h>
#endif

#include <esp_sleep.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/BootError.hpp"
#include "Misc/BootLoopDetection.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Models/Readable.hpp"
#include "Storage/CrashReportWriter.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContentSyncScreen.hpp"
#include "UI/RootUI.hpp"
#include "config.h"
#include "sindarin-debug.h"

#define ENABLE_CONTENT_SYNC_ON_CLOSE 1

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
BLESettings bleSettings(&prefs);
#endif

#if USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
BLEImprov bleWifi;
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
bool runningContentSync;
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
SolClock solClock(prefs);
PowerManager powerManager(prefs);

bool templesClosed = false;
bool bootCompleted = false;

// Task for all periodic things that need to run ~1Hz. Polling tasks should be grouped here so that
// they all complete in a tight loop, thus maximizing sleep time.
void PollingLoopTask(void *parameter) {
    for (;;) {
        // Run the power loop first so that we don't show a 0% battery reading
        power.loop();

#if USE_TURN_TOUCH_REMOTE
        // Skip scanning while WiFi is connected to avoid kicking off the WiFi connection
        if (!WiFi::getInstance().connected() && !templesClosed) {
            ttRemoteInput.connect();
        } else if (!ttRemoteInput.connected()) {
            // log_d("Skipping Turn Touch scan while WiFi is connected");
        }
#endif

#if USE_BLE_SERVER_REMOTE
        bleWifi.loop();
#endif

#if USE_MMC5603
        templeCloseSensor.checkTemple();
#endif

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // This happens after the delay, so that the first check can run with a faster fade.
        lightingController.checkAmbientLight();
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

void UIInnerLoop(RemoteCommand command) {
    powerManager.feedInactivityTimer();
    rootUi.loop(myDisplay, command);
}

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

    if (!prefs.begin()) {
        bootErrors.push_back({.type = BootError::Type::Preferences});
    }

#if USE_MMC5603 & !SIMULATOR
    int morningNews = prefs.morningNewspaperTime.get().value_or(0);
    int eveningNews = prefs.eveningNewspaperTime.get().value_or(0);

    log_i("Morning News: %d, Evening News: %d", morningNews, eveningNews);

    // We are waking up after the inactivty timeout, so we need to check if the temple is closed
    // and if so, reset it.
    if (prefs.sleepReason.get().value_or(SLEEP_REASON_NONE) == SLEEP_REASON_INACTIVITY) {
        log_i("Inactivity timer caused this reboot, checking if temple is closed");
        templeCloseSensor.checkTemple();
        if (!templeCloseSensor.getOpened()) {
            log_i("Changing sleep reason because the temple is now closed");
            prefs.sleepReason.set(SLEEP_REASON_TEMPLES_CLOSED);
        }
    }

    if ((prefs.sleepReason.get().value_or(SLEEP_REASON_NONE) == SLEEP_REASON_INACTIVITY) ||
        (morningNews != 0) || (eveningNews != 0)) {
        templeCloseSensor.checkTemple();
        if (!templeCloseSensor.getOpened() ||
            (prefs.sleepReason.get().value_or(SLEEP_REASON_NONE) == SLEEP_REASON_INACTIVITY)) {
            uint8_t hours, minutes;
            solClock.getClockTime(hours, minutes);
            log_i("Temple is closed or glasses inactive, current time: %d:%d", hours, minutes);
            if (((hours && (hours == morningNews)) || (hours && (hours == eveningNews))) &&
                (minutes == 0)) {
                digitalWrite(BLUE_LED, LED_ON);
                runningContentSync = true;
                localStore.syncFromRemote([](const ContentSyncState &state) {
                    if (state.state == ContentSyncState::Failure ||
                        state.state == ContentSyncState::Synced ||
                        state.state == ContentSyncState::AlreadySynced) {
                        if (state.state == ContentSyncState::Failure) {
                            log_i("Content sync failed with error: %s", state.message.c_str());
                        } else if (state.state == ContentSyncState::Synced) {
                            log_i("Content sync successful");
                        } else if (state.state == ContentSyncState::AlreadySynced) {
                            log_i("Content sync already synced");
                        }
                        digitalWrite(BLUE_LED, LED_OFF);
                        runningContentSync = false;
                        WiFi::getInstance().disconnectWiFi();
                        log_i("Going to sleep after content sync");
                        PowerManager::getInstance().powerOff();
                    }
                });
            } else {
                log_i("Temple closed, going to sleep");
                PowerManager::getInstance().powerOff();
            }
        }
    }
#endif

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
                templesClosed = false;
                if (runningContentSync) {
                    // Effectively cancel the content sync by disconnecting WiFi
                    WiFi::getInstance().disconnectWiFi();
                }
            } else {
                lightingController.disableLighting();
                templesClosed = true;
                // Will be turned off after content sync
                prefs.lastBatteryReport.set((int)power.getBatteryPercentage());
            }
        });
        templeCloseSensor.setLongCloseCallback([]() {
#if ENABLE_CONTENT_SYNC_ON_CLOSE
            DispatchGetUiQueue()->dispatch([]() {
                log_d("Temples closed for a while, running content sync");
                // Debug LED so we know what's happening
                if (templesClosed) {
                    digitalWrite(BLUE_LED, LED_ON);
                    runningContentSync = true;
                    localStore.syncFromRemote([](const ContentSyncState &state) {
                        if (state.state == ContentSyncState::Failure ||
                            state.state == ContentSyncState::Synced ||
                            state.state == ContentSyncState::AlreadySynced) {
                            if (state.state == ContentSyncState::Failure) {
                                log_i("Content sync failed with error: %s", state.message.c_str());
                            } else if (state.state == ContentSyncState::Synced) {
                                log_i("Content sync successful");
                            } else if (state.state == ContentSyncState::AlreadySynced) {
                                log_i("Content sync already synced");
                            }
                            digitalWrite(BLUE_LED, LED_OFF);
                            // Only if the temples are still closed, allow power off
                            if (templesClosed) {
                                prefs.sleepReason.set(SLEEP_REASON_TEMPLES_CLOSED);
                                PowerManager::getInstance().powerOff();
                            }
                            runningContentSync = false;
                            WiFi::getInstance().disconnectWiFi();
                        }
                    });
                }
            });
#else
            // Just precautinoary in case we turn off ENABLE_CONTENT_SYNC_ON_CLOSE in the future
            // We should still power down if the temples are closed for a while!
            if (templesClosed) {
                prefs.sleepReason.set(SLEEP_REASON_TEMPLES_CLOSED);
                PowerManager::getInstance().powerOff();
            }
#endif
        });
    }
#endif

    SHOW_HEAP();

    // This setting must happen *before* the below vcom set from
    // preferences. Otherwise, when in dark mode, the vcom
    // will have the wrong value due to the calculation that
    // happens in this method. Clunky, for sure. But when
    // we better understand vcom and don't need to have such fine-tuned
    // settings for dark mode and vcom, we can probably clean it up then.
    myDisplay.invertDisplay(prefs.invertSetting.get().value_or(false));

#if EINKET013TT1
#ifdef DISPLAY_POWER
    // Turn on power to the screen
    digitalWrite(DISPLAY_POWER, HIGH);
#endif
    // If the saved prefs indicated a vcom, set it here.
    auto vcomSetting = prefs.vcom.get();
    if (vcomSetting.has_value()) {
        myDisplay.getDriver()->setVcom(vcomSetting.value());
    }
#endif

    switch (prefs.font.get().value_or(PREFS_TAHOMA_REGULAR)) {
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
    initialRect.x = prefs.rectX.get().value_or(myDisplay.defaultBoundingBox.x);
    initialRect.y = prefs.rectY.get().value_or(myDisplay.defaultBoundingBox.y);
    initialRect.width = prefs.rectW.get().value_or(myDisplay.defaultBoundingBox.width);
    initialRect.height = prefs.rectH.get().value_or(myDisplay.defaultBoundingBox.height);

    log_d("Initial bounding box is %d %d %d %d", initialRect.x, initialRect.y, initialRect.width,
          initialRect.height);
    myDisplay.setBoundingBox(initialRect);
    myDisplay.setLineSpacing(prefs.lineSpacing.get().value_or(0));

    SIZE_OF(myDisplay);

    if (prefs.cesMode.get().value_or(false)) {
        // In CES mode, we want to turn on the lights to full brightness
        lightingController.setOverrideValuePercent(100);
    }

    // For all the EMC modes, we'll keep the lights at 100%
    if (prefs.emcMode.get().value_or(false)) {
        lightingController.setOverrideValuePercent(100);
    }

#if USE_BLE
    BLEDevice::init("Sindarin");
    auto bleStatus = esp_bt_controller_get_status();
    if (bleStatus != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        bootErrors.push_back({.type = BootError::Type::BLE});
    }
#endif

#if USE_TURN_TOUCH_REMOTE || USE_BLE_SERVER_REMOTE
    auto remoteCallback = [](RemoteCommand command) {
        DispatchGetUiQueue()->dispatch([command]() {
            if (bootCompleted) {
                UIInnerLoop(command);
            } else {
                log_w("Ignoring user input until boot completes");
            }
        });
    };
#endif

#if USE_TURN_TOUCH_REMOTE
    ttRemoteInput.setCallback(remoteCallback);
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    auto bleManager = BLEManager::getInstance();
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#if USE_BLE_SERVER_REMOTE
    bleRemoteInput.configureCharacteristics(sindarinService);
    bleRemoteInput.setCallback(remoteCallback);
#endif

#if USE_BLE_SERVER_SETTINGS
    bleSettings.configureCharacteristics(sindarinService);
#if USE_TURN_TOUCH_REMOTE
    bleSettings.attachTTRemoteInput(&ttRemoteInput);
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
    bleWifi.configureService(bleManager->server());
    if (!bleWifi.start()) {
        bootErrors.push_back({.type = BootError::Type::BLEWifi});
    }
#endif

    WiFi::getInstance().setWiFiPower(
        (wifi_power_t)prefs.wifiPower.get().value_or(WIFI_POWER_17dBm));

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    if (!bleManager->startAdvertisingIfNecessary()) {
        bootErrors.push_back({.type = BootError::Type::BLEManagerAdvertising});
    }
#endif

#if !DISABLE_POWER_MANAGEMENT
    if (powerManager.begin() != ESP_OK) {
        bootErrors.push_back({.type = BootError::Type::PowerManagement});
    }

    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
        bootErrors.push_back({.type = BootError::Type::LowPowerOscillator});
    }
#endif

    // set current boot id
    auto bootCount = prefs.bootCount.get().value_or(0);
    bootCount++;
    prefs.bootCount.set(bootCount);

    if (!fileSystem.begin()) {
        bootErrors.push_back({.type = BootError::Type::FileSystem});
    } else {
        // This is a hack for now for debugging crashes while connected to the serial
        // monitor -- don't save (and thus delete crashes) when connected to USB. This means
        // the boot count will be wrong once this is saved, but that's probably ok
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

    xTaskCreatePinnedToCore(PollingLoopTask, "Polling", 10000, nullptr, 1, nullptr, 0);
    // xTaskCreatePinnedToCore(StatsTask, "Stats", 5000, nullptr, 0, nullptr, 1);

    // Begin fade-in on the lighting before rendering the UI
    DispatchGetUiQueue()->dispatch([]() { lightingController.checkAmbientLight(300, 100); });

    // Render the first thing
    DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::NONE); });

    GoodBootTimer();

    bootCompleted = true;
}
