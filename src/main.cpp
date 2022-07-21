#include "Communications/OTA.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "Displays/AllDisplays.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/BootError.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/RootUI.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if !DISABLE_POWER_MANAGEMENT
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_pm.h>
#endif

#if EINKET013TT1
DisplayEinkET013TT1 myDisplay(DISPLAY_CS_LEFT, DISPLAY_CS_RIGHT, DISPLAY_OEI, DISPLAY_RST,
                              DISPLAY_BUSY);
#endif

#if SIMULATOR
DisplaySDLSimulator myDisplay;
#endif

#if WROVER_KIT
#include "Displays/DisplayWroverLcd.hpp"
DisplayWroverLcd myDisplay;
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

// Resonsible for handling connecting / reconnecting to remotes
void RadioTask(void *parameter) {
    for (;;) {
#if USE_TURN_TOUCH_REMOTE
        ttRemoteInput.connect();
#endif

#if USE_BLE_SERVER_REMOTE && !DISABLE_WIFI
        bleWifi->loop();
#endif

#if ENABLE_OTA
        ota.loop();
#endif

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(nullptr);
}
#endif

void StatsTask(void *parameter) {
#if !SIMULATOR
    for (;;) {
        // Run the power loop first so that we don't show a 0% battery reading
        power.loop();
        // Spit out data on locks every 30s
        vTaskDelay(30000 / portTICK_PERIOD_MS);
#if !DISABLE_POWER_MANAGEMENT
        esp_pm_dump_locks(stdout);
#endif
    }
    vTaskDelete(nullptr);
#endif
}

void UIInnerLoop(RemoteCommand command) { rootUi.loop(myDisplay, command); }

#if !SIMULATOR
TaskHandle_t uiTaskHandle = nullptr;

void UITask(void *parameter) {
    RemoteCommand command = RemoteCommand::NONE;
    for (;;) {
        UIInnerLoop(command);

        // Peek if there's anything remaining in the queue
        if (xQueuePeek(remoteQueue, &command, 0) != pdTRUE) {
            // If there's nothing in the queue, sleep the thread until we're told to wake up
            // (probably because there's something in the queue)
            xTaskNotifyWait(0, 0, nullptr, portMAX_DELAY);
        }
        if (xQueueReceive(remoteQueue, &command, 0) != pdTRUE) {
            log_w("Checking the remoteQueue but nothing was there");
            command = RemoteCommand::NONE;
        }
    }
    vTaskDelete(nullptr);
}
#endif

#if CONFIG_AUTOSTART_ARDUINO
void loop() { vTaskDelay(portMAX_DELAY); };
void setup() {
#else
extern "C" void app_main(void) { // NOLINT(readability-identifier-naming)
    initArduino();
#endif

    std::vector<BootError> bootErrors;

#if BOARD_HAS_PSRAM
    if (ESP.getPsramSize() <= 0) {
        bootErrors.push_back({.type = BootError::Type::ExternalRam});
    }
#endif

    InitializePlatform();

#if !SIMULATOR
    esp_log_level_set("Sindarin", ESP_LOG_DEBUG);
    // Arduino breaks the UART with sleep. So we manually initialize the UART using IDF functions.
    // There's unfortunately no way currently to customize the source clock in Arduino:
    // https://github.com/espressif/arduino-esp32/blob/548412952ba58e299ad83d932cea417a2490fd78/cores/esp32/esp32-hal-uart.c#L163
    uart_config_t uartConfig;
    uartConfig.baud_rate = 115200;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.rx_flow_ctrl_thresh = 0;
    // This is the part Arduino gets wrong. The ref tick clock is stable even amidst light sleep.
    uartConfig.source_clk = UART_SCLK_REF_TICK;
    ESP_ERROR_CHECK(uart_driver_install(0, 256, 0, 20, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(0, &uartConfig));
    ESP_ERROR_CHECK(uart_set_pin(0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
#endif

    if (!prefs.begin()) {
        bootErrors.push_back({.type = BootError::Type::Preferences});
    }

    SHOW_HEAP();

#if EINKET013TT1
#ifdef DISPLAY_POWER
    // Turn on power to the screen
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

#if USE_BLE
    BLEDevice::init("Sindarin");
    auto bleStatus = esp_bt_controller_get_status();
    if (bleStatus != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        bootErrors.push_back({.type = BootError::Type::BLE});
    }
#endif

#if USE_TURN_TOUCH_REMOTE || USE_BLE_SERVER_REMOTE
    auto remoteCallback = [](RemoteCommand command) {
        xQueueSend(remoteQueue, (void *)&command, (TickType_t)0);
        // Notify the UI thread that we have a new command
        xTaskNotify(uiTaskHandle, 0, eNoAction);
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

#if !DISABLE_WIFI
    if (!ConnectWiFiIfCredentialsExist()) {
        bootErrors.push_back({.type = BootError::Type::WiFi});
    }

#if ENABLE_OTA
    // Disabling for now since WiFi coexistance is not working
    // ota.setup();
#endif
#endif

#if !DISABLE_POWER_MANAGEMENT
    esp_pm_config_esp32_t pmConfig = {
        .max_freq_mhz = CPU_FREQ_MHZ_HIGH,
        .min_freq_mhz = CPU_FREQ_MHZ_LOW,
        .light_sleep_enable = true,
    };
    if (esp_pm_configure(&pmConfig) != ESP_OK) {
        bootErrors.push_back({.type = BootError::Type::PowerManagement});
    }
    if (rtc_clk_slow_freq_get() != RTC_SLOW_FREQ_32K_XTAL) {
        bootErrors.push_back({.type = BootError::Type::LowPowerOscillator});
    }

#endif

    auto readables = new std::vector<Readable>();
    if (!fileSystem.begin()) {
        bootErrors.push_back({.type = BootError::Type::FileSystem});
    }
    fileSystem.getReadables(readables);
    fileSystem.getDownloadedPocketReadables(readables);

    SHOW_HEAP();

    power.setup();

    rootUi.setup(&myDisplay, *readables, fileSystem);

    SHOW_HEAP();

    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);

    BlinkBootErrors(bootErrors);

#if !SIMULATOR
    remoteQueue = xQueueCreate(10, sizeof(RemoteCommand));
    xTaskCreatePinnedToCore(RadioTask, "Radio", 5000, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(UITask, "UI", 10000, nullptr, 10, &uiTaskHandle, 1);
    xTaskCreatePinnedToCore(StatsTask, "Stats", 5000, nullptr, 0, nullptr, 1);
#endif
}
