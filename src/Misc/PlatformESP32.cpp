#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/rtc_io.h>
#include <driver/uart.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_err.h>
#include <esp_mac.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <hal/gpio_types.h>
#include <nvs_flash.h>
#include <optional>
#include <soc/rtc.h>
#include <tinyusb.h>
#include <tusb.h>
#include <tusb_cdc_acm.h>
#include <tusb_console.h>
#include <ulp_riscv.h>

#include "Misc/StringUtil.hpp"
#include "Platform.hpp"
#include "Storage/include/EFusesCustomTable.h"
#include "config.h"
#include "sindarin-debug.h"

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
extern PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
extern LittleFSFileSystem fileSystem;
#endif

#define BACKLIGHT_PWM_CHANNEL 0
#define HARDWARE_REVISION_LATEST 3

adc_cali_handle_t adcCalHandle;
adc_oneshot_unit_handle_t adcOneshotHandle;
bool ledcFailed = false;
uint8_t sysOnPin;
bool platformInitialized;

static uint8_t minLightingDutyCycle;
static uint8_t maxLightingDutyCycle;
static QueueHandle_t uartCommandQueue;

void PinMode(int pin, int mode) {
    gpio_set_direction(static_cast<gpio_num_t>(pin), static_cast<gpio_mode_t>(mode));
}

void DigitalWrite(int pin, int level) {
    gpio_set_level(static_cast<gpio_num_t>(pin), static_cast<uint32_t>(level));
}

auto DigitalRead(uint8_t pin) -> int { return gpio_get_level(static_cast<gpio_num_t>(pin)); }

void Delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

auto Millis() -> int { return GetRunTimeMs(); }

void HardwareInit() {
    esp_err_t err;
#if USE_I2C
    // release rtc i2c lines
    ulp_timer_stop();
    ulp_riscv_halt();
    err = rtc_gpio_deinit(static_cast<gpio_num_t>(I2C_MASTER_SDA_IO));
    if (err != ESP_OK) {
        log_e("rtc_gpio_deinit failed for SDA: %d", err);
    }
    err = rtc_gpio_deinit(static_cast<gpio_num_t>(I2C_MASTER_SCL_IO));
    if (err != ESP_OK) {
        log_e("rtc_gpio_deinit failed for SCL: %d", err);
    }

    // Initialize I2C port 0
    int i2cPort = 0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        // TODO: We have 2.7K hardware pullups, we should experiment with removing them
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    err = i2c_param_config(static_cast<i2c_port_t>(i2cPort), &conf);
    if (err != ESP_OK) {
        log_e("i2c_param_config failed: %d", err);
    }
    err = i2c_driver_install(static_cast<i2c_port_t>(i2cPort), conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        log_e("i2c_driver_install failed: %d", err);
    }
#endif

#ifdef USB_CONNECTED_PIN
    PinMode(USB_CONNECTED_PIN, INPUT);
#endif

    DigitalWrite(RED_LED, LED_OFF);
    DigitalWrite(GREEN_LED, LED_OFF);
    PinMode(RED_LED, OUTPUT);
    PinMode(GREEN_LED, OUTPUT);
#ifdef BLUE_LED
    DigitalWrite(BLUE_LED, LED_OFF);
    PinMode(BLUE_LED, OUTPUT);
#endif

    // Attenuation of 2.5db gets us up to a max of 1250mV
    // Using our 442k/160k divider, that's 4.7V max
    adc_cali_curve_fitting_config_t adcConfig;
    adcConfig.unit_id = ADC_UNIT_1;
    adcConfig.atten = ADC_ATTEN_DB_2_5;
    adcConfig.bitwidth = ADC_BITWIDTH_12;

    if (adc_cali_create_scheme_curve_fitting(&adcConfig, &adcCalHandle) != ESP_OK) {
        log_e("Failed to create adc curve fitting scheme");
    }

    adc_oneshot_unit_init_cfg_t adcOneshotCfg;
    adcOneshotCfg.unit_id = ADC_UNIT_1;
    adcOneshotCfg.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
    adcOneshotCfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adcOneshotCfg, &adcOneshotHandle));

    adc_oneshot_chan_cfg_t adcChanCfg;
    adcChanCfg.bitwidth = adcConfig.bitwidth;
    adcChanCfg.atten = adcConfig.atten;

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcOneshotHandle, BATT_CHANNEL_LEFT, &adcChanCfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcOneshotHandle, BATT_CHANNEL_RIGHT, &adcChanCfg));

    esp_log_level_set("*", static_cast<esp_log_level_t>(CONFIG_LOG_DEFAULT_LEVEL));
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        const esp_partition_t *partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, nullptr);
        if (partition != nullptr) {
            err = esp_partition_erase_range(partition, 0, partition->size);
            if (!err) {
                err = nvs_flash_init();
            } else {
                log_e("Failed to format the broken NVS partition!");
            }
        } else {
            log_e("Could not find NVS partition");
        }
    }
    if (err) {
        log_e("Failed to initialize NVS! Error: %u", err);
    }
}

auto HardwareDeinit() -> esp_err_t {
    esp_err_t err;

    err = nvs_flash_deinit();
    if (err != ESP_OK) {
        log_e("Failed to deinit NVS: %d", err);
    }

    // We need to shut down the ADC or else we consume ~1.6mA during deep sleep
    err = adc_oneshot_del_unit(adcOneshotHandle);
    if (err != ESP_OK) {
        log_e("Failed to delete oneshot unit: %d", err);
    }

    err = adc_cali_delete_scheme_curve_fitting(adcCalHandle);
    if (err != ESP_OK) {
        log_e("Failed to delete adc calibration scheme: %d", err);
    }

    err = i2c_driver_delete(I2C_NUM_0);
    if (err != ESP_OK) {
        log_e("Failed to delete I2C driver with err %s", esp_err_to_name(err));
    }

    return err;
}

auto GetHeapSize() -> size_t {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    return info.total_free_bytes + info.total_allocated_bytes;
}

void EnableCore0Wdt() {
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
    if (idle0 == nullptr || esp_task_wdt_add(idle0) != ESP_OK) {
        log_e("Failed to add Core 0 IDLE task to WDT");
    }
}

void DisableCore0Wdt() {
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
    if (idle0 == nullptr || esp_task_wdt_delete(idle0) != ESP_OK) {
        log_e("Failed to remove Core 0 IDLE task from WDT");
    }
}

void InitializePlatform() {
#if USE_SYS_ON
    auto hardwareRevision = GetHardwareRevision();
    if (hardwareRevision == 1) {
        sysOnPin = 5;
    } else {
        sysOnPin = 46;
    }
    if (hardwareRevision < 3) {
        minLightingDutyCycle = 5;
        maxLightingDutyCycle = 60;
    } else {
        // Revisions 3 and higher have the 82ohm resistors in series
        minLightingDutyCycle = 2;
        maxLightingDutyCycle = 210;
    }
    // The MCU will keep itself on until the digital magnetometer detects the temples are closed, or
    // there are 5 mins of inactivity
    ForcePowerOn();
#endif

#if EINKET013TT1
#ifdef DISPLAY_POWER
    PinMode(DISPLAY_POWER, OUTPUT);
#endif
#if LEDC_BACKLIGHT
    // This allows LEDC to work in light sleep on ESP32-S3
    esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);

    if (err != ESP_OK) {
        log_e("Error configuring esp_sleep_pd_config: %s", esp_err_to_name(err));
    }

    // Backlight configs
    ledc_timer_config_t ledcTimer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                     .duty_resolution = LEDC_TIMER_8_BIT,
                                     .timer_num = LEDC_TIMER_0,
                                     .freq_hz = 1000,
                                     .clk_cfg = LEDC_USE_RC_FAST_CLK};
    err = ledc_timer_config(&ledcTimer);
    if (err != ESP_OK) {
        log_e("Error configuring LEDC timer: %s", esp_err_to_name(err));
        ledcFailed = true;
    }

    ledc_channel_config_t ledcLChannel = {
        .gpio_num = BACKLIGHT_LEFT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0 //
    };

    ledc_channel_config_t ledcRChannel = {
        .gpio_num = BACKLIGHT_RIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0 //
    };

    err = ledc_channel_config(&ledcLChannel);
    if (ledc_channel_config(&ledcLChannel) != ESP_OK) {
        log_e("Error configuring left LEDC: %s", esp_err_to_name(err));
        ledcFailed = true;
    }

    err = ledc_channel_config(&ledcRChannel);
    if (err != ESP_OK) {
        log_e("Error configuring right LEDC: %s", esp_err_to_name(err));
        ledcFailed = true;
    }

    err = ledc_fade_func_install(0);
    if (err != ESP_OK) {
        log_e("Error installing LEDC fade function: %s", esp_err_to_name(err));
        ledcFailed = true;
        return;
    }
#endif
#endif // EINKET013TT1

    // Configure the UART for reading (used for testing)
    const uart_config_t uartConfig = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_RTC,
    };
    // We won't use a buffer for sending data.
    constexpr auto kRxBufSize = 256;
    uart_driver_install(UART_NUM_0, kRxBufSize, 0, 3, &uartCommandQueue, 0);
    uart_param_config(UART_NUM_0, &uartConfig);

    platformInitialized = true;
}

// Define custom device descriptor
// Define custom device descriptor
tusb_desc_device_t my_descriptor = {.bLength = sizeof(my_descriptor),
                                    .bDescriptorType = TUSB_DESC_DEVICE,
                                    .bcdUSB = 0x0200, // USB version. 0x0200 means version 2.0
                                    .bDeviceClass = TUSB_CLASS_UNSPECIFIED,
                                    .bMaxPacketSize0 = 64,

                                    .idVendor = 0x303A,
                                    .idProduct = 0x3000,
                                    .bcdDevice = 0x0101, // Device FW version

                                    .iManufacturer = 0x01, // see string_descriptor[1] below
                                    .iProduct = 0x02,      // see string_descriptor[2] below
                                    .iSerialNumber = 0x03, // see string_descriptor[3] below

                                    .bNumConfigurations = 0x01};

// Define custom configuration descriptor
/*
const uint8_t my_configuration_descriptor[] = {
    // Configuration Descriptor
    0x09, TUSB_DESC_CONFIGURATION, 0x43, 0x00, 0x02, 0x01, 0x00, 0xC0, 0x32,

    // Interface Association Descriptor
    0x08, TUSB_DESC_INTERFACE_ASSOCIATION, 0x00, 0x02, TUSB_CLASS_CDC,
    CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, 0x00,

    // CDC Control Interface
    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x01, TUSB_CLASS_CDC,
    CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, 0x00, 0x05, 0x24,
    CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0110), 0x05, 0x24, CDC_FUNC_DESC_CALL_MANAGEMENT, 0x00,
    0x01, 0x04, 0x24, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0x02, 0x05, 0x24,
    CDC_FUNC_DESC_UNION, 0x00, 0x01, 0x07, TUSB_DESC_ENDPOINT, 0x83, TUSB_XFER_INTERRUPT, 0x08,
    0x00, 0x10,

    // CDC Data Interface
    0x09, TUSB_DESC_INTERFACE, 0x01, 0x00, 0x02, TUSB_CLASS_CDC_DATA, 0x00, 0x00, 0x00, 0x07,
    TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_BULK, 0x40, 0x00, 0x00, 0x07, TUSB_DESC_ENDPOINT, 0x82,
    TUSB_XFER_BULK, 0x40, 0x00, 0x00};
*/
/* Did this in menuconfig
tusb_desc_strarray_device_t my_string_descriptor = {
    (char[]){0x09, 0x04},
    "Sindarin, Inc.",
    "Sol Reader",
    "4096",
};
*/

// CDC line state change callback
void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event) {
    int dtr = event->line_state_changed_data.dtr;
    int rst = event->line_state_changed_data.rts;
    log_i("Line state changed! dtr:%d, rst:%d", dtr, rst);
}

#define CONFIG_USB_CDC_RX_BUFSIZE 64
static uint8_t buf[CONFIG_USB_CDC_RX_BUFSIZE + 1];

// CDC RX callback
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event) {
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret =
        tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, buf, CONFIG_USB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        buf[rx_size] = '\0';
        log_i("Got data (%d bytes): %s", rx_size, buf);
    } else {
        log_i("Read error");
    }

    /* write back */
    tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)itf, buf, rx_size);
    tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)itf, 0);
}

#define FILENAME_BUFSIZE 65
int fileReceiveState = 0;
char secretCode[9] = "uuddlrlr";
char file_name[FILENAME_BUFSIZE];
u_int file_size = 0;
u_int bytes_received = 0;
u_int checksum = 0;
SFile file;

uint8_t calculate_checksum(const uint8_t *data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

// CDC RX callback
void tinyusb_cdc_rx_callback_for_files(int itf, cdcacm_event_t *event) {
    /* initialization */
    size_t rx_size = 0;

    // Turn off logging to not mess with stuff
    esp_tusb_deinit_console(TINYUSB_CDC_ACM_0); // log to usb

    /* read */
    esp_err_t ret =
        tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, buf, CONFIG_USB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        buf[rx_size] = '\0';
        log_i("Got data in fileReceiveState %d (%d bytes)", fileReceiveState, rx_size);
    } else {
        log_i("Read error");
    }

    if (fileReceiveState == 0) {
        log_i("Doing a string n compare of %s and %s", buf, secretCode);
        if (strncmp((char *)buf, secretCode, 8) == 0) {
            log_i("File transfer start detected");
            fileReceiveState = 1;
        }
    } else if (fileReceiveState == 1) {
        strncpy(file_name, (char *)buf, rx_size);
        file_name[rx_size] = '\0'; // Ensure null-terminated string
        log_i("File Name: %s", file_name);
        fileReceiveState = 2;
    } else if (fileReceiveState == 2) {
        file_size = atoi((char *)buf);
        log_i("File Size: %d bytes", file_size);
        fileSystem.createDir("/reader", true);
        /* Open the file for writing here */
        file =
            fileSystem.open(StringFormat("%s%s", "/reader/", file_name).c_str(), FILE_WRITE, true);
        if (file.getFilePtr() == nullptr) {
            log_e("Failed to open file for writing");
            fileReceiveState = 3;
        } else {
            checksum = 0;
            fileReceiveState = 3;
        }
    } else if (fileReceiveState == 3) {
        /* Write the file */
        file.write(buf, rx_size);
        bytes_received += rx_size;
        log_i("Wrote a chunk to file: %s", buf);
        if (bytes_received >= file_size) {
            /* Close the file and finish the write*/
            file.close();
            fileReceiveState = 4;
        }
    } else if (fileReceiveState == 4) {
        if (1) /* error check later (rx_size == 1 && buf[0] == checksum) */ {
            log_i("File received and saved successfully: %s", file_name);
        } else {
            log_i("Checksum verification failed");
            // unlink(file_name); // Remove the corrupted file
        }
        fileReceiveState = 0;
    }

    esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb
}
/* write back */
// tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)itf, buf, rx_size);
// tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)itf, 0);

void USBDeviceInit() {
    // init usb device driver
    log_i("USB initialization");

    const tinyusb_config_t tusbCfg = {.device_descriptor = &my_descriptor,
                                      .string_descriptor = nullptr,
                                      .external_phy = false,
                                      .configuration_descriptor = nullptr};

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusbCfg));

    // Define custom configuration for CDC ACM in C++
    tinyusb_config_cdcacm_t acmCfg;
    acmCfg.usb_dev = TINYUSB_USBDEV_0;             // USB device to set up
    acmCfg.cdc_port = TINYUSB_CDC_ACM_0;           // Use first CDC ACM interface
    acmCfg.rx_unread_buf_sz = 64;                  // Size of the receive buffer
    acmCfg.callback_rx = &tinyusb_cdc_rx_callback; // RX callback
    acmCfg.callback_rx_wanted_char = NULL;         // RX wanted char callback
    acmCfg.callback_line_state_changed = NULL;     // Line state change callback
    acmCfg.callback_line_coding_changed = NULL;    // Line coding change callback

    // tinyusb_config_cdcacm_t acmCfg = {(tinyusb_usbdev_t)0}; // the configuration uses default
    // values
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg));

    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
        TINYUSB_CDC_ACM_0, CDC_EVENT_LINE_STATE_CHANGED, &tinyusb_cdc_line_state_changed_callback));

    esp_tusb_init_console(TINYUSB_CDC_ACM_0); // log to usb

    Delay(2000); // wait for USB to enumerate

    log_i("Completed port CDC_ACM_0 initialization");
    log_i("Attempting port CDC_ACM_1 initialization");
    // Define custom configuration for CDC ACM in C++
    tinyusb_config_cdcacm_t acmCfg1;
    acmCfg1.usb_dev = TINYUSB_USBDEV_0;                       // USB device to set up
    acmCfg1.cdc_port = TINYUSB_CDC_ACM_1;                     // Use second CDC ACM interface
    acmCfg1.rx_unread_buf_sz = 64;                            // Size of the receive buffer
    acmCfg1.callback_rx = &tinyusb_cdc_rx_callback_for_files; // RX callback
    acmCfg1.callback_rx_wanted_char = NULL;                   // RX wanted char callback
    acmCfg1.callback_line_state_changed = NULL;               // Line state change callback
    acmCfg1.callback_line_coding_changed = NULL;              // Line coding change callback
    log_i("Before init call");
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg1));
    log_i("After init call");
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
        TINYUSB_CDC_ACM_1, CDC_EVENT_LINE_STATE_CHANGED, &tinyusb_cdc_line_state_changed_callback));
    log_i("Finished port CDC_ACm_1 initialization attempt");
}

auto DeinitializePlatform() -> esp_err_t {
    if (!platformInitialized) {
        return ESP_OK;
    }

    esp_err_t err;

    err = esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);
    if (err != ESP_OK) {
        log_e("Failed to disable fast RC oscillator: %d", err);
    }

#if LEDC_BACKLIGHT
    ledc_fade_func_uninstall();

    err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (err != ESP_OK) {
        log_e("Failed to stop left LEDC: %d", err);
    }

    err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    if (err != ESP_OK) {
        log_e("Failed to stop right LEDC: %d", err);
    }
#endif
    return err;
}

auto GetUartCommandQueue() -> QueueHandle_t { return uartCommandQueue; }

// Forked from TinyPico's GetBatteryVoltage() method:
// https://github.com/tinypico/tinypico-arduino/blob/master/TinyPICO-Helper/src/TinyPICO.cpp#L207
auto GetBatteryVoltageLeft() -> float {
    int raw;
    if (adc_oneshot_read(adcOneshotHandle, BATT_CHANNEL_LEFT, &raw) != ESP_OK) {
        log_e("error reading raw adc value for left battery");
        return 0.0;
    }

    int millivolts;
    if (adc_cali_raw_to_voltage(adcCalHandle, raw, &millivolts) != ESP_OK) {
        log_e("error reading left battery voltage");
        return 0.0;
    }
    auto mv = static_cast<float>(UPPER_DIVIDER + LOWER_DIVIDER) / LOWER_DIVIDER * millivolts;
    return static_cast<float>(mv) / 1000.0;
}

auto GetBatteryVoltageRight() -> float {
    int raw;
    if (adc_oneshot_read(adcOneshotHandle, BATT_CHANNEL_RIGHT, &raw) != ESP_OK) {
        log_e("error reading raw adc value for right battery");
        return 0.0;
    }

    int millivolts;
    if (adc_cali_raw_to_voltage(adcCalHandle, raw, &millivolts) != ESP_OK) {
        log_e("error reading left battery voltage");
        return 0.0;
    }
    auto mv = static_cast<float>(UPPER_DIVIDER + LOWER_DIVIDER) / LOWER_DIVIDER * millivolts;
    return static_cast<float>(mv) / 1000.0;
}

void SetLeftScreenLight(int dutyCycle) {
    if (ledcFailed) {
        return;
    }
    esp_err_t err;
#if LEDC_BACKLIGHT
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle);
    if (err != ESP_OK) {
        log_e("Error setting duty ch0: %s", esp_err_to_name(err));
        return;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        log_e("Error updating duty: %s", esp_err_to_name(err));
        return;
    }
#endif
}

void SetLeftScreenLightFade(int dutyCycle, int fadeMs) {
    if (ledcFailed) {
        return;
    }
    esp_err_t err;
#if LEDC_BACKLIGHT
    // Set the LED to the target brightness
    err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle, fadeMs);
    if (err != ESP_OK) {
        log_e("Error setting fade with time: %s", esp_err_to_name(err));
        return;
    }
    // Start the LED fading process
    err = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
    if (err != ESP_OK) {
        log_e("Error starting fade: %s", esp_err_to_name(err));
        return;
    }
#endif
}

void SetRightScreenLight(int dutyCycle) {
    if (ledcFailed) {
        return;
    }
    esp_err_t err;
#if LEDC_BACKLIGHT
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dutyCycle);
    if (err != ESP_OK) {
        log_e("Error setting duty ch1: %s", esp_err_to_name(err));
        return;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    if (err != ESP_OK) {
        log_e("Error updating duty: %s", esp_err_to_name(err));
        return;
    }
#endif
}

void SetRightScreenLightFade(int dutyCycle, int fadeMs) {
    if (ledcFailed) {
        return;
    }
    esp_err_t err;
#if LEDC_BACKLIGHT
    // Set the LED to the target brightness
    err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dutyCycle, fadeMs);
    if (err != ESP_OK) {
        log_e("Error setting fade with time: %s", esp_err_to_name(err));
        return;
    }
    // Start the LED fading process
    err = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
    if (err != ESP_OK) {
        log_e("Error starting fade: %s", esp_err_to_name(err));
        return;
    }
#endif
}

auto GetMinLightingDutyCycle() -> uint8_t { return minLightingDutyCycle; }

auto GetMinLightingDutyCycleOverride() -> uint8_t { return 1; }

auto GetMaxLightingDutyCycle() -> uint8_t { return maxLightingDutyCycle; }

auto GetRunTimeMs() -> uint32_t {
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

auto USBConnected() -> bool {
#ifdef USB_CONNECTED_PIN
    return DigitalRead(USB_CONNECTED_PIN);
#else
    // No way to know if USB is connected on old boards. But it's probably not.
    return false;
#endif
}

auto GetSerialNumber() -> std::string {
    uint8_t serialNumber[14] = {0};
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, &serialNumber,
                                              sizeof(serialNumber) * 8));

    // sanitize unset bytes in serial number field
    for (unsigned char &i : serialNumber) {
        if (i == 0) {
            i = '0';
        }
    }

    return {reinterpret_cast<const char *>(serialNumber), sizeof(serialNumber)};
}

auto GetMacAddress() -> std::string {
    uint8_t wifimac[6];
    char str[18];
    str[17] = '\0';
    esp_err_t ret = esp_read_mac(wifimac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        int ret = snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X", wifimac[0],
                           wifimac[1], wifimac[2], wifimac[3], wifimac[4], wifimac[5]);
        if (ret < 0 || ret >= sizeof(str)) {
            log_e("snprintf failure %d!", ret);
        }
    } else {
        log_e("esp_read_mac failure %d!", ret);
    }
    return {str};
}

auto GetHardwareRevision() -> uint8_t {
    // Cache the value
    static std::optional<uint8_t> rev;
    if (rev.has_value()) {
        return rev.value();
    }

    uint8_t serialNumber[14] = {0};
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, &serialNumber,
                                              sizeof(serialNumber) * 8));

    // If these bytes are null, assume it's the latest hardware version
    if (serialNumber[3] == 0 || serialNumber[4] == 0) {
        log_d("Unset SN (null), assuming latest hardware revision");
        return HARDWARE_REVISION_LATEST;
    }

    // Check that the first two bytes are in ascii numeric range
    if (serialNumber[3] < '0' || serialNumber[3] > '9' || serialNumber[4] < '0' ||
        serialNumber[4] > '9') {
        log_e("Invalid SN (out of ascii ranges), assuming latest hardware revision");
        return HARDWARE_REVISION_LATEST;
    }

    // Serial number starts with SOL01 where the `01` is the hardware revision. Turn those two ascii
    // bytes into an int.
    rev = (serialNumber[3] - '0') * 10 + (serialNumber[4] - '0');
    log_d("Hardware revision: %d", rev.value());
    return rev.value();
}

void ForcePowerOn() {
    gpio_reset_pin(static_cast<gpio_num_t>(sysOnPin));
    gpio_set_pull_mode(static_cast<gpio_num_t>(sysOnPin), GPIO_FLOATING);
    gpio_set_direction(static_cast<gpio_num_t>(sysOnPin), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(sysOnPin), 1);
    gpio_hold_en(static_cast<gpio_num_t>(sysOnPin));
    gpio_deep_sleep_hold_en();
}

void AllowPowerOff() {
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(static_cast<gpio_num_t>(sysOnPin));

    // We haven't been particuarly scientific, but experimentally using INPUT_PULLDOWN seems to work
    // the best. I don't understand why. You'd think either an open drain or driving to 0 would be
    // more effective at turning off the mostfet.
    PinMode(sysOnPin, INPUT_PULLDOWN);

    // pinMode(sysOnPin, OUTPUT_OPEN_DRAIN);
    // DigitalWrite(sysOnPin, LOW);

    // pinMode(sysOnPin, OUTPUT);
    // DigitalWrite(sysOnPin, LOW);
}
