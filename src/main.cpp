#include <esp_err.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#if !SIMULATOR
#include <soc/rtc.h>
#endif
#if CONFIG_USE_TINYUSB_SERIAL
#include <tusb.h>
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/BackgroundSync.hpp"
#include "Communications/FileTransfer.hpp"
#include "Communications/WiFi.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/BootError.hpp"
#include "Misc/BootLoopDetection.hpp"
#include "Misc/DirectCommand.hpp"
#include "Misc/FactoryReset.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/SolMorse.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Models/Readable.hpp"
#include "Storage/CrashReportWriter.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ChargingScreen.hpp"
#include "UI/OverTempScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/RootUI.hpp"
#include "UI/TestAlignmentScreen.hpp"
#include "Version.hpp"
#include "config.h"
#include "sindarin-debug.h"

#define ENABLE_CONTENT_SYNC_ON_CLOSE 1

#if CONFIG_DISPLAY_EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
DisplayEinkET013TT1 myDisplay(DISPLAY_CS_LEFT, DISPLAY_CS_RIGHT, DISPLAY_RST);
#elif DISPLAY_SIM_8BIT
// Declared in native.cpp
#include "Displays/DisplaySim8Bit.hpp"
extern DisplaySim8Bit myDisplay;
#elif CONFIG_DISPLAY_M5STACK_ATOM_DISPLAY
#include "Displays/DisplayM5StackAtomDisplay.hpp"
DisplayM5StackAtomDisplay myDisplay;
#elif CONFIG_DISPLAY_M5STACK_MODULE_DISPLAY
#include "Displays/DisplayM5StackModuleDisplay.hpp"
DisplayM5StackModuleDisplay myDisplay;
#endif

PreferencesStore prefs;

#if CONFIG_USE_BLE_SERVER_REMOTE || CONFIG_USE_BLE_SERVER_SETTINGS
#include "Communications/BLESindarinService.hpp"
#endif

#if CONFIG_USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
#endif

#if CONFIG_USE_BLE_SERVER_SETTINGS
#include "Communications/BLESettings.hpp"
BLESettings bleSettings(&prefs);
#endif

#if CONFIG_USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
#endif

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLESyncService.hpp"
BLESyncService bleSyncService;
#endif

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEProxy.hpp"
#endif

#include "Sensors/LightingController.hpp"
LightingController lightingController(prefs);

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
LittleFSFileSystem fileSystem;
#endif

Telemetry telemetry(fileSystem, prefs);

#if CONFIG_USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput ttRemoteInput(prefs, telemetry);
#endif

PowerManager powerManager(fileSystem, prefs, lightingController, myDisplay, telemetry);
LocalStore localStore(fileSystem, prefs, powerManager.battery, telemetry);
RootUI rootUi(prefs, powerManager, telemetry);
SolClock solClock(prefs);
WiFi wiFi(telemetry);

USBSerial usbSerial;

DirectCommand directCommand(usbSerial, fileSystem, powerManager);
FileTransfer fileTransferListener(fileSystem, localStore, usbSerial, directCommand);

bool bootCompleted = false;

void UIInnerLoop(RemoteCommand command) {
    powerManager.feedInactivityTimer();
    rootUi.loop(myDisplay, command);
}

// Task for all periodic things that need to run ~1Hz. Polling tasks should be grouped here so that
// they all complete in a tight loop, thus maximizing sleep time.
void PollingLoopTask(void *parameter) {
    int factoryResetStartTimeMs = 0;
    while (powerManager.state() != PowerState::ShuttingDown) {
        // check if factory reset button is held down
        if (DigitalRead(FACTORY_RESET_BUTTON) == 0) {
            // prevent inactivity sleep while button held down
            powerManager.feedInactivityTimer();
            if (factoryResetStartTimeMs == 0) {
                factoryResetStartTimeMs = Millis();
            } else if ((Millis() - factoryResetStartTimeMs) > FACTORY_RESET_MS) {
                log_i("Button held down for %dms performing factory reset",
                      Millis() - factoryResetStartTimeMs);
                SoftFactoryReset(fileSystem);
                powerManager.releaseLED();
                BlinkSol();
                powerManager.reboot();
            }
        } else {
            factoryResetStartTimeMs = 0;
        }

        // Run the power loop first so that we don't show a 0% battery reading
        powerManager.loop();

        directCommand.pollHwUart();

#if CONFIG_USE_TURN_TOUCH_REMOTE
        // Skip scanning while WiFi is connected to avoid kicking off the WiFi connection
        if (!WiFi::getInstance().connected()) {
            ttRemoteInput.scanIfNecessary();
        } else if (!ttRemoteInput.connected()) {
            // log_d("Skipping PageTurner scan while WiFi is connected");
        }
#endif

#if CONFIG_USE_BLE_SERVER_REMOTE
        BLEImprov::getInstance()->loop();
#endif

        vTaskDelay(pdMS_TO_TICKS(1000));
#if CONFIG_USE_LTR329
        // This happens after the delay, so that the first check can run with a faster fade.
        lightingController.checkAmbientLight();
#endif
    }
    log_d("PollingLoopTask exited");
    vTaskDelete(nullptr);
}

void StatsTask(void *parameter) {
    while (powerManager.state() != PowerState::ShuttingDown) {
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

auto UIGetCurrentScreen() { return rootUi.getCurrentScreen(); }

extern "C" void app_main(void) { // NOLINT(readability-identifier-naming)
    usbSerial.begin();
    directCommand.begin();

    HardwareInit();

    powerManager.wakeupCheck(solClock);

    BootLoopCheck(fileSystem, powerManager);
    std::vector<BootError> bootErrors;

#if CONFIG_BOARD_HAS_PSRAM
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

    solClock.begin();

    log_i("SN: %s", GetSerialNumber().c_str());

    if (esp_reset_reason() == ESP_RST_POWERON) {
        // Flash all the LEDs
        DigitalWrite(CONFIG_RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_RED_LED, LED_OFF);
        DigitalWrite(CONFIG_GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_GREEN_LED, LED_OFF);
#ifdef CONFIG_BLUE_LED
        DigitalWrite(CONFIG_BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_BLUE_LED, LED_OFF);
#endif
    }

    if (powerManager.begin() != ESP_OK) {
        bootErrors.push_back({.type = BootError::Type::PowerManagement});
    }

    // The following commands can run after PowerManager::begin has completed
    directCommand.addCommandHandler([](const std::string &command, const std::string &payload,
                                       const DirectCommand::TxLine &txLine) -> bool {
        if (command == "BATT") {
            auto response =
                StringFormat("BATT L:%.2f R:%.2f", powerManager.battery.getLeftBatteryVoltage(),
                             powerManager.battery.getRightBatteryVoltage());
            // Also log for convenience during testing
            log_i("%s", response.c_str());
            txLine(response);
            return true;
        } else if (command == "BOOTREASON") {
            auto response =
                StringFormat("BOOTREASON %s", powerManager.getBootInfo().humanStr().c_str());
            // Also log for convenience during testing
            log_i("%s", response.c_str());
            txLine(response);
            return true;
        } else if (command == "LOCKS") {
            txLine(command);
#if !DISABLE_POWER_MANAGEMENT
            // TODO: Would be nice to also dump to the command port
            auto err = esp_pm_dump_locks(stdout);
            if (err != ESP_OK) {
                log_e("esp_pm_dump_locks error: %s", esp_err_to_name(err));
            }
#endif
            return true;
        }
        return false;
    });

    if (!fileSystem.begin()) {
        bootErrors.push_back({.type = BootError::Type::FileSystem});
    } else {
        log_i("Filesystem ready!");
    }

    // Set current boot id
    auto bootCount = prefs.bootCount.get().value_or(0);
    bootCount++;
    prefs.bootCount.set(bootCount);

    // This is a hack for now for debugging crashes while connected to the serial
    // monitor -- don't save (and thus delete crashes) when connected to USB. This means
    // the boot count will be wrong once this is saved, but that's probably ok
    if (!USBConnected()) {
        // file system started
        // attempt to write any core dumps to disk for later upload
        CrashReportWriter crashReportWriter(fileSystem, bootCount - 1);
        crashReportWriter.writeCrashReportIfExists();
    }

    if (powerManager.state() != PowerState::ActiveUSBCharging && !localStore.begin()) {
        bootErrors.push_back({.type = BootError::Type::LocalStore});
    }

    WiFi::getInstance().setWiFiPower(
        (wifi_power_t)prefs.wifiPower.get().value_or(WIFI_POWER_16dBm));

    if (powerManager.state() == PowerState::Background) {
        log_i("Booting into PowerState::Background, running a content sync");
        // Spin up a task just to check the temple and battery
        powerManager.setStateChangeCallback([](PowerState state) {
            if (state == PowerState::ShuttingDown) {
                log_d("Signaling background sync to cancel");
                BackgroundSyncCancel();
            }
        });

        xTaskCreatePinnedToCore(
            [](void *) {
            while (powerManager.state() != PowerState::ShuttingDown) {
                powerManager.loop();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            log_d("Background loop task exited");
            vTaskDelete(nullptr);
            },
            "BackgroundPowerLoop", 4096, nullptr, 1, nullptr, 1);
        BackgroundSyncAndSleep(powerManager, prefs, localStore, telemetry);

        // Should never get here, the system should be asleep
        return;
    }

    // Below code should only execute on an Active boot

#if CONFIG_USE_LTR329
    if (!lightingController.checkSensorAvailable()) {
        bootErrors.push_back({.type = BootError::Type::AmbientLight});
    } else {
        lightingController.begin();
    }
#else
    lightingController.setOverrideValuePercent(100);
#endif

#if CONFIG_USE_MMC5603
    if (!powerManager.checkMagnetometerProductId()) {
        bootErrors.push_back({.type = BootError::Type::Magnetometer});
    }
#endif

    SHOW_HEAP();

#if CONFIG_DISPLAY_EINKET013TT1
#ifdef DISPLAY_POWER
    // Turn on power to the screen
    DigitalWrite(DISPLAY_POWER, HIGH);
#endif
#endif

    switch (prefs.font.get().value_or(PREFS_SOL_SMALL)) {
    case PREFS_SOL_MEDIUM:
        myDisplay.style.setFontFamily(myDisplay.style.solSansMedium);
        break;
    case PREFS_SOL_LARGE:
        myDisplay.style.setFontFamily(myDisplay.style.solSansLarge);
        break;
    case PREFS_SOL_SMALL:
        myDisplay.style.setFontFamily(myDisplay.style.solSansSmall);
        break;
    }

    if (!myDisplay.setup()) {
        bootErrors.push_back({.type = BootError::Type::Display});
    }

    // This setting must happen *before* the below vcom set from
    // preferences. Otherwise, when in dark mode, the vcom
    // will have the wrong value due to the calculation that
    // happens in this method. Clunky, for sure. But when
    // we better understand vcom and don't need to have such fine-tuned
    // settings for dark mode and vcom, we can probably clean it up then.
    myDisplay.invertDisplay(prefs.invertSetting.get().value_or(false));

#if CONFIG_DISPLAY_EINKET013TT1
    // If the saved prefs indicated a vcom, set it here.
    auto vcomSetting = prefs.vcom.get();
    if (vcomSetting.has_value()) {
        myDisplay.getDriver()->setVcom(vcomSetting.value());
    }
#endif

    myDisplay.setLineSpacing(prefs.lineSpacing.get().value_or(0));

    SIZE_OF(myDisplay);

    // For all the EMC modes, we'll keep the lights at 100%
    if (prefs.emcMode.get().value_or(false)) {
        lightingController.setOverrideValuePercent(100);

        // Also kick off a task to try to content sync every minute or so
        xTaskCreatePinnedToCore(
            [](void *) {
            while (powerManager.state() != PowerState::ShuttingDown) {
                PreferencesStore::syncReason = SyncReason::Periodic;
                BackgroundSync(powerManager, prefs, localStore, telemetry);
                vTaskDelay(60000 / portTICK_PERIOD_MS);
            }
            log_d("Sync loop task exited");
            vTaskDelete(nullptr);
            },
            "BackgroundSync", 4096, nullptr, 1, nullptr, 1);
    }

#if USE_BLE
    BLEDevice::init("Sol Reader");

    if (prefs.blePower.get().has_value()) {
        auto power = prefs.blePower.get().value_or(ESP_PWR_LVL_P3);
        log_d("Setting default BLE power to %" PRIu32, power);
        esp_err_t err =
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, static_cast<esp_power_level_t>(power));
        if (err != ESP_OK) {
            log_e("esp_ble_tx_power_set err: %s", esp_err_to_name(err));
        }
    }

    auto bleStatus = esp_bt_controller_get_status();
    if (bleStatus != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        bootErrors.push_back({.type = BootError::Type::BLE});
    }
#endif

#if CONFIG_USE_TURN_TOUCH_REMOTE || CONFIG_USE_BLE_SERVER_REMOTE
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

#if CONFIG_USE_TURN_TOUCH_REMOTE
    ttRemoteInput.setCallback(remoteCallback);
    powerManager.setBeginShutdownCallback([]() { ttRemoteInput.disconnectTemporarily(60); });
#endif

#if CONFIG_USE_BLE_SERVER_REMOTE || CONFIG_USE_BLE_SERVER_SETTINGS || CONFIG_USE_BLE_SERVER_IMPROV
    auto bleManager = BLEManager::getInstance();
    if (prefs.bleAdvertisingKey.get().has_value()) {
        bleManager->setManufacturerData(prefs.bleAdvertisingKey.get().value_or(""));
    }
#endif

#if CONFIG_USE_BLE_SERVER_REMOTE || CONFIG_USE_BLE_SERVER_SETTINGS
    auto sindarinService =
        BLESindarinService::getInstance()->configureService(bleManager->server());

#if CONFIG_USE_BLE_SERVER_REMOTE
    BLERemoteInput::getInstance()->configureCharacteristics(sindarinService);
    BLERemoteInput::getInstance()->setCallback(remoteCallback);
#endif

#if CONFIG_USE_BLE_SERVER_SETTINGS
    bleSettings.configureCharacteristics(sindarinService);
#if CONFIG_USE_TURN_TOUCH_REMOTE
    bleSettings.attachTTRemoteInput(&ttRemoteInput);
#endif
#endif

    if (!BLESindarinService::getInstance()->start()) {
        bootErrors.push_back({.type = BootError::Type::BLESindarin});
    }
#endif

#if CONFIG_USE_BLE_SERVER_PROXY
    auto syncService = bleSyncService.configureService(bleManager->server());
    BLEProxy::getInstance()->setPrefs(&prefs);
    BLEProxy::getInstance()->configureCharacteristics(syncService);

    if (!bleSyncService.start()) {
        bootErrors.push_back({.type = BootError::Type::BLESync});
    }
#endif

#if CONFIG_USE_BLE_SERVER_IMPROV
    // NOTE(john): It might make sense at some point to consolidate everything
    // as one service. However keeping these services separate for now allows us
    // to use the Improv WebBluetooth implementation for Wifi provisioning and
    // also use the Sindarin mobile app as-is after provisioning is done. If we
    // want to merge these in the future, it will be as simple as using
    // ble_wifi->configureCharacteristics(sindarinService)
    BLEImprov::getInstance()->configureService(bleManager->server());
    if (!BLEImprov::getInstance()->start()) {
        bootErrors.push_back({.type = BootError::Type::BLEWifi});
    }
#endif

#if CONFIG_USE_BLE_SERVER_REMOTE || CONFIG_USE_BLE_SERVER_SETTINGS || CONFIG_USE_BLE_SERVER_IMPROV
    if (!bleManager->startAdvertisingIfNecessary()) {
        bootErrors.push_back({.type = BootError::Type::BLEManagerAdvertising});
    }
#endif

#if !DISABLE_POWER_MANAGEMENT
    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
        bootErrors.push_back({.type = BootError::Type::LowPowerOscillator});
    }
#endif

    SHOW_HEAP();

#if DISABLE_POWER_MANAGEMENT
    log_w("Power Management Disabled");
#endif

    auto showChargingScreen = []() {
        lightingController.setOverrideValuePercent(100);
        auto chargingScreen = new ChargingScreen(prefs);
        rootUi.setCurrentScreen(*chargingScreen);
        UIInnerLoop(RemoteCommand::NONE);
    };

    powerManager.setStateChangeCallback([&showChargingScreen](PowerState state) {
        DispatchGetUiQueue()->dispatch([&showChargingScreen, state]() {
            if (state == PowerState::ActiveUSBCharging) {
                showChargingScreen();
            } else if (state == PowerState::ActiveOverTemp) {
                auto overTempScreen = new OverTempScreen();
                rootUi.setCurrentScreen(*overTempScreen);
                UIInnerLoop(RemoteCommand::NONE);
            } else if (state == PowerState::ActiveBatteryDepleted) {
                auto powerDepletedScreen = new PowerDepletedScreen();
                rootUi.setCurrentScreen(*powerDepletedScreen);
                UIInnerLoop(RemoteCommand::NONE);
            }
        });
    });

    BlinkBootErrors(bootErrors);

    // Clear screen in case not cleared at shutdown
    if (!PreferencesStore::screenClearedAtShutdown) {
        DispatchGetUiQueue()->dispatch([]() { myDisplay.clearScreen(); });
    }
    PreferencesStore::screenClearedAtShutdown = false;

    xTaskCreatePinnedToCore(PollingLoopTask, "Polling", 5000, nullptr, 1, nullptr, 0);

    // xTaskCreatePinnedToCore(StatsTask, "Stats", 5000, nullptr, 0, nullptr, 1);

#if CONFIG_USE_LTR329
    // Begin fade-in on the lighting before rendering the UI
    DispatchGetUiQueue()->dispatch([]() { lightingController.checkAmbientLight(300, 100); });
#endif

    // Render the first thing
    DispatchGetUiQueue()->dispatch([showChargingScreen]() {
        SHOW_HEAP();
        if (powerManager.state() == PowerState::ActiveUSBCharging) {
            showChargingScreen();
        } else {
            rootUi.setup(&myDisplay, localStore, fileSystem, lightingController);
            UIInnerLoop(RemoteCommand::NONE);
            log_i("First screen rendered after boot in %" PRIu64 " ms",
                  esp_timer_get_time() / 1000);
        }
        SHOW_HEAP();
    });

    GoodBootTimer();

    bootCompleted = true;

    // The following commands can run after app_main has completed
    directCommand.addCommandHandler([](const std::string &command, const std::string &payload,
                                       const DirectCommand::TxLine &txLine) -> bool {
        if (command == "CONNECT") {
            // This is just the connect handshake to indicate we're here and listening
            txLine(command);
            return true;
        } else if (command == "MIRROR") {
            if (payload == "0") {
                myDisplay.mirrorToSerial = false;
                txLine("MIRROR 0");
            } else if (payload == "1") {
                myDisplay.mirrorToSerial = true;
                txLine("MIRROR 1");
                myDisplay.sendCurrentScreenToSerial();
            } else {
                txLine(StringFormat("MIRROR %d", myDisplay.mirrorToSerial));
            }
            return true;
        } else if (command == "SCREENSHOT") {
            myDisplay.sendCurrentScreenToSerial(true);
        } else if (command == "GRID") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() {
                // Override brightness to max
                lightingController.setOverrideValuePercent(100);
                auto testScreen = new TestAlignmentScreen();
                rootUi.setCurrentScreen(*testScreen);
                UIInnerLoop(RemoteCommand::NONE);
            });
            return true;
        } else if (command == "UP") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::UP); });
            return true;
        } else if (command == "LEFT") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::LEFT); });
            return true;
        } else if (command == "RIGHT") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::RIGHT); });
            return true;
        } else if (command == "DOWN") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::DOWN); });
            return true;
        } else if (command == "ENTER") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::ENTER); });
            return true;
        } else if (command == "BACK") {
            txLine(command);
            DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::BACK); });
            return true;
        } else if (command == "UNPAIRPT") {
#if CONFIG_USE_TURN_TOUCH_REMOTE
            ttRemoteInput.unpairAndDisconnect();
#endif
            txLine(command);
            return true;
        }
        return false;
    });
}
