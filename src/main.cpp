#if !SIMULATOR
#include <driver/i2c.h>
#include <driver/uart.h>
#include <soc/rtc.h>
#endif

#include <esp_err.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/BackgroundSync.hpp"
#include "Communications/WiFi.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/BootError.hpp"
#include "Misc/BootLoopDetection.hpp"
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
#include "config.h"
#include "sindarin-debug.h"

#define DISABLE_POWER_MANAGEMENT 1
#define DISABLE_DEEP_SLEEP 1

#define ENABLE_CONTENT_SYNC_ON_CLOSE 1

#if USE_LILYGO_KEYBOARD

#define I2C_MASTER_FREQ_HZ 100000UL

#define LILYGO_KB_SLAVE_ADDRESS 0x55

#define BOARD_POWERON (gpio_num_t)10
#define BOARD_I2C_SDA (gpio_num_t)18
#define BOARD_I2C_SCL (gpio_num_t)8
#define I2C_MASTER_NUM I2C_NUM_0     // I2C port number
#define WRITE_BIT I2C_MASTER_WRITE   // I2C master write
#define READ_BIT I2C_MASTER_READ     // I2C master read
#define ACK_CHECK_EN 0x1             // I2C master will check ack from slave
#define ACK_CHECK_DIS 0x0            // I2C master will not check ack from slave
#define ACK_VAL (i2c_ack_type_t)0x0  // I2C ack value
#define NACK_VAL (i2c_ack_type_t)0x1 // I2C nack value

static const char *TAG = "T-Deck Keyboard Master";

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = BOARD_I2C_SDA;
    conf.scl_io_num = BOARD_I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags =
        I2C_SCLK_SRC_FLAG_FOR_NOMAL; // Optional for some configurations, uncomment if needed

    i2c_param_config((i2c_port_t)i2c_master_port, &conf);
    esp_err_t ret = i2c_driver_install((i2c_port_t)i2c_master_port, conf.mode, 0, 0, 0);
    i2c_set_timeout((i2c_port_t)i2c_master_port, 0x0000001FU);
    return ret;
}

#define READ_SIZE 1 // Number of bytes to read

esp_err_t custom_i2c_master_read_from_device(i2c_port_t i2c_num, uint8_t device_address,
                                             uint8_t *data, size_t size, TickType_t ticks_to_wait) {
    if (size == 0) {
        return ESP_OK; // Nothing to read
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    esp_err_t err = i2c_master_start(cmd);

    if (err != ESP_OK) {
        log_e("Error starting I2C command: %d", err);
    }

    err = i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);

    if (err != ESP_OK) {
        log_e("Error writing byte to I2C command: %d", err);
    }

    if (size > 1) {
        err = i2c_master_read(cmd, data, size - 1, I2C_MASTER_ACK);
        if (err != ESP_OK) {
            log_e("Error reading I2C command: %d", err);
        }
    }

    err = i2c_master_read_byte(cmd, data + size - 1, I2C_MASTER_NACK); // NACK on last byte
    if (err != ESP_OK) {
        log_e("Error reading byte from I2C command: %d", err);
    }
    err = i2c_master_stop(cmd);
    if (err != ESP_OK) {
        log_e("Error stopping I2C command: %d", err);
    }

    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, ticks_to_wait);
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * @brief Test function to check existence of the keyboard device
 */
static esp_err_t keyboard_check(void) {
    uint8_t data;
    esp_err_t ret = i2c_master_read_from_device(I2C_MASTER_NUM, LILYGO_KB_SLAVE_ADDRESS, &data, 1,
                                                1000 / portTICK_RATE_MS);

    return ret;
}
#endif

#if EINKET013TT1
#if SIMULATOR
#include "Displays/DisplayEinkET013TT1.hpp"
DisplayEinkET013TT1 myDisplay(DISPLAY_CS_LEFT, DISPLAY_CS_RIGHT, DISPLAY_RST);
#else
#include "Displays/DisplayLilygo.hpp"
DisplayLilygo myDisplay(TDECK_TFT_DC, TDECK_TFT_CS, TDECK_SPI_SCK, TDECK_SPI_MOSI, TDECK_SPI_MISO);
#endif
#endif

PreferencesStore prefs;

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
#include "Communications/BLESindarinService.hpp"
BLESindarinService bleSindarinService;
#endif

#if USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
#endif

#if USE_BLE_SERVER_SETTINGS
#include "Communications/BLESettings.hpp"
BLESettings bleSettings(&prefs);
#endif

#if USE_BLE_SERVER_IMPROV
#include "Communications/BLEImprov.hpp"
#endif

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput ttRemoteInput(prefs);
#endif

#if USE_BLE_SERVER_PROXY
#include "Communications/BLESyncService.hpp"
BLESyncService bleSyncService;
#endif

#if USE_BLE_SERVER_PROXY
#include "Communications/BLEProxy.hpp"
#endif

#include "Sensors/LightingController.hpp"
LightingController lightingController;

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
LittleFSFileSystem fileSystem;
#endif

Telemetry telemetry(fileSystem, prefs);
PowerManager powerManager(fileSystem, prefs, lightingController, myDisplay, telemetry);
LocalStore localStore(fileSystem, prefs, powerManager.battery, telemetry);
RootUI rootUi(prefs, powerManager, telemetry);
SolClock solClock(prefs);
WiFi wiFi(telemetry);

bool bootCompleted = false;

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

#if USE_TURN_TOUCH_REMOTE
        // Skip scanning while WiFi is connected to avoid kicking off the WiFi connection
        if (!WiFi::getInstance().connected()) {
            ttRemoteInput.scanIfNecessary();
        } else if (!ttRemoteInput.connected()) {
            // log_d("Skipping PageTurner scan while WiFi is connected");
        }
#endif

#if USE_BLE_SERVER_REMOTE
        BLEImprov::getInstance()->loop();
#endif

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // This happens after the delay, so that the first check can run with a faster fade.
        // lightingController.checkAmbientLight();
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

void UIInnerLoop(RemoteCommand command) {
    powerManager.feedInactivityTimer();
    rootUi.loop(myDisplay, command);
}

char keyboardBuffer[256];
int keyboardBufferIndex = 0;
int nextKeyboardBufferToRead = 0;

void clearKeyboardBuffer() {
    keyboardBufferIndex = 0;
    nextKeyboardBufferToRead = 0;
}

void setKeyboardReadIndex(int index) { nextKeyboardBufferToRead = index; }
int getKeyboardReadIndex() { return nextKeyboardBufferToRead; }

// -1 means there was nothing
// probably need some race condition stuff between this and KeyboardInputTask
signed char readKeyboardBuffer() {
    char ret = -1;
    if (nextKeyboardBufferToRead != keyboardBufferIndex) {
        ret = keyboardBuffer[nextKeyboardBufferToRead];
        nextKeyboardBufferToRead++;
        if (nextKeyboardBufferToRead >= 256) {
            nextKeyboardBufferToRead = 0;
        }
    }
    return ret;
}

#if USE_LILYGO_KEYBOARD
// Thread to monitor keyboard input and store it until the UI Loop can process it
void KeyboardInputTask(void *parameter) {
    uint8_t data;
    while (1) {
        esp_err_t ret = i2c_master_read_from_device(I2C_MASTER_NUM, LILYGO_KB_SLAVE_ADDRESS, &data,
                                                    1, 1000 / portTICK_RATE_MS);
        if (ret == ESP_OK && data != 0x00) {
            log_d("keyValue : %c", (char)data);
            if (data != 8) {
                keyboardBuffer[keyboardBufferIndex++] = (char)data;
            } else {
                // Backspace if there's anything to remove!
                if (keyboardBufferIndex > 0) {
                    keyboardBufferIndex--;
                }
            }
            if (keyboardBufferIndex >= 256) {
                keyboardBufferIndex = 0;
            }
            if (bootCompleted) {
                UIInnerLoop(RemoteCommand::KEYPRESS);
            } else {
                log_w("From Keyboard input task: Ignoring user input until boot completes");
            }
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    log_d("KeyboardInputTask exited");
    vTaskDelete(nullptr);
}
#endif

#if 0
extern "C" void app_main(void) {
    HardwareInit();

    ESP_LOGI(TAG, "T-Deck Keyboard Master");

    // Peripheral power control
    gpio_set_direction(BOARD_POWERON, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_POWERON, 1);

    // Wait some time to let the keyboard power up
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());

    // Check if keyboard is online
    if (keyboard_check() != ESP_OK) {
        while (1) {
            ESP_LOGI(TAG, "LILYGO Keyboard not online.");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    uint8_t data;
    while (1) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (LILYGO_KB_SLAVE_ADDRESS << 1) | READ_BIT, ACK_CHECK_EN);
        i2c_master_read_byte(cmd, &data, NACK_VAL);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK && data != 0x00) {
            ESP_LOGI(TAG, "keyValue : %c", (char)data);
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
#endif

#if CONFIG_AUTOSTART_ARDUINO
void loop() { vTaskDelay(portMAX_DELAY); };
void setup() {
#else
extern "C" void app_main(void) { // NOLINT(readability-identifier-naming)
#if USE_LILYGO_KEYBOARD
    // Peripheral power control
    PinMode(BOARD_POWERON, OUTPUT);
    DigitalWrite(BOARD_POWERON, HIGH);

    // Wait some time to let the keyboard power up
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Initialize I2C
    err_t init_err = i2c_master_init();
    if (init_err != ESP_OK) {
        log_e("ERROR initializing I2C via i2c_master_init: %d", init_err);
    }

    init_err = keyboard_check();
    // Check if keyboard is online
    if (init_err != ESP_OK) {
        log_e("Couldn't get an ACK from the keyboard, continuing anyway, error code %d", init_err);
    } else {
        log_d("SUCCESSFUL Connection to keyboard.");
    }
#endif

    HardwareInit();
#endif

    // Configure log level for all Sindarin logs
    esp_log_level_set("Sol", ESP_LOG_DEBUG);
    powerManager.wakeupCheck(solClock);

    BootLoopCheck(fileSystem, powerManager);
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

    solClock.begin();
#if 0
    log_i("SN: %s", GetSerialNumber().c_str());

    if (esp_reset_reason() == ESP_RST_POWERON) {
        // Flash all the LEDs
        DigitalWrite(RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(RED_LED, LED_OFF);
        DigitalWrite(GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(GREEN_LED, LED_OFF);
#ifdef BLUE_LED
        DigitalWrite(BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(BLUE_LED, LED_OFF);
#endif
    }
#endif
    if (powerManager.begin() != ESP_OK) {
        bootErrors.push_back({.type = BootError::Type::PowerManagement});
    }

    if (!fileSystem.begin()) {
        bootErrors.push_back({.type = BootError::Type::FileSystem});
    } else {
        log_d("Filesystem ready!");
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
#if USE_LTR329
    if (!lightingController.checkSensorAvailable()) {
        bootErrors.push_back({.type = BootError::Type::AmbientLight});
    } else {
        lightingController.begin();
    }
#endif

#if USE_MMC5603
    if (!powerManager.checkMagnetometerProductId()) {
        bootErrors.push_back({.type = BootError::Type::Magnetometer});
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
    DigitalWrite(DISPLAY_POWER, HIGH);
#endif
    // If the saved prefs indicated a vcom, set it here.
    auto vcomSetting = prefs.vcom.get();
    if (vcomSetting.has_value()) {
        // myDisplay.getDriver()->setVcom(vcomSetting.value());
    }
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

    DisplayRect initialRect{};
    initialRect.x = prefs.rectX.get().value_or(myDisplay.defaultBoundingBox.x);
    initialRect.y = prefs.rectY.get().value_or(myDisplay.defaultBoundingBox.y);
    initialRect.width = prefs.rectW.get().value_or(myDisplay.defaultBoundingBox.width);
    initialRect.height = prefs.rectH.get().value_or(myDisplay.defaultBoundingBox.height);

    log_d("Initial bounding box is %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 "", initialRect.x,
          initialRect.y, initialRect.width, initialRect.height);
    myDisplay.setBoundingBox(initialRect);
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

#if USE_LILYGO_KEYBOARD
    auto keyboardCallback = [](char key) {
        DispatchGetUiQueue()->dispatch([key]() {
            if (bootCompleted) {
                UIInnerLoop(RemoteCommand::KEYPRESS);
            } else {
                log_w("Ignoring user input until boot completes");
            }
        });
    };
#endif

#if USE_TURN_TOUCH_REMOTE
    ttRemoteInput.setCallback(remoteCallback);
    powerManager.setBeginShutdownCallback([]() { ttRemoteInput.disconnectTemporarily(60); });
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
    auto bleManager = BLEManager::getInstance();
    if (prefs.bleAdvertisingKey.get().has_value()) {
        bleManager->setManufacturerData(prefs.bleAdvertisingKey.get().value_or(""));
    }
#endif

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS
    auto sindarinService = bleSindarinService.configureService(bleManager->server());

#if USE_BLE_SERVER_REMOTE
    BLERemoteInput::getInstance()->configureCharacteristics(sindarinService);
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

#if USE_BLE_SERVER_PROXY
    auto syncService = bleSyncService.configureService(bleManager->server());
    BLEProxy::getInstance()->setPrefs(&prefs);
    BLEProxy::getInstance()->configureCharacteristics(syncService);

    if (!bleSyncService.start()) {
        bootErrors.push_back({.type = BootError::Type::BLESync});
    }
#endif

#if USE_BLE_SERVER_IMPROV
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

#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS || USE_BLE_SERVER_IMPROV
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

    xTaskCreatePinnedToCore(PollingLoopTask, "Polling", 10000, nullptr, 1, nullptr, 0);
    // xTaskCreatePinnedToCore(StatsTask, "Stats", 5000, nullptr, 0, nullptr, 1);

    // Begin fade-in on the lighting before rendering the UI
    DispatchGetUiQueue()->dispatch([]() { /*lightingController.checkAmbientLight(300, 100); */ });

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

#if USE_LILYGO_KEYBOARD
    xTaskCreatePinnedToCore(KeyboardInputTask, "KeyboardInput", 4096, nullptr, 1, nullptr, 1);
#endif
    GoodBootTimer();

    bootCompleted = true;
}
