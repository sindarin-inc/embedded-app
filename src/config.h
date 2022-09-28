// Configuration defines
// WARNING: Based on how this file is pre-included everywhere, we don't trigger
// an entirely new build when this file changes. To make sure changes here get
// picked up you need to clean and build everything. Or make sure any files that
// need stuff from here explicitly #include "config.h"

#ifndef REVISION
#define REVISION 7
#endif

#ifndef EINKET013TT1
#define EINKET013TT1 1
#endif

#define Hastings20x40_3

// Currently using the App (BLE Server in firmware) is incompatible with
// Turn Touch (BLE client in firmware). Enable one of these options or the
// other.
#ifndef USE_TURN_TOUCH_REMOTE
#define USE_TURN_TOUCH_REMOTE 1
#endif
#ifndef USE_BLE_SERVER_REMOTE
#define USE_BLE_SERVER_REMOTE 1
#endif
#ifndef USE_BLE_SERVER_SETTINGS
#define USE_BLE_SERVER_SETTINGS 1
#endif
#ifndef USE_BLE_SERVER_IMPROV
#define USE_BLE_SERVER_IMPROV 1
#endif
#define USE_BLE                                                                                    \
    (USE_TURN_TOUCH_REMOTE || USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS ||                  \
     USE_BLE_SERVER_IMPROV)

#ifndef DISABLE_WIFI
#define DISABLE_WIFI 0
#endif
// #define DISABLE_SPIRAM

#ifndef ENABLE_OTA
#define ENABLE_OTA 1
#define HOSTNAME "sindarin"
#endif

// Disable Improve BLE WiFi provisioning service
// #define DISABLE_IMPROV

#ifndef ENABLE_POCKET
#define ENABLE_POCKET 1
#endif

#ifdef WROVER_KIT
#define RED_LED 0
#define GREEN_LED 2
#elif GILPICO_SINDARIN
#define RED_LED 22
#define GREEN_LED 25
#else
#define RED_LED 7
#define GREEN_LED 6
#define BLUE_LED 18
#endif

#ifdef LED_INVERT
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#if GILPICO_SINDARIN
#define BAT_VOLTAGE_RIGHT 37
#define BAT_VOLTAGE_LEFT 38
#define BATT_CHANNEL_RIGHT ADC1_CHANNEL_1
#define BATT_CHANNEL_LEFT ADC1_CHANNEL_2
#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
// Default referance voltage in mv
#define DEFAULT_VREF 1100
#else
#define BAT_VOLTAGE_RIGHT 10
#define BAT_VOLTAGE_LEFT 4
#define BATT_CHANNEL_RIGHT ADC1_CHANNEL_9
#define BATT_CHANNEL_LEFT ADC1_CHANNEL_3
#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
// Unused on ESP32-S3
#define DEFAULT_VREF 0
#endif

#if defined(REVSION) && REVISION < 7
// On revisions prior to 7, the screen's power was tied to pin 26 along with the backlight, so
// define the power pin to turn on display and keep it on.
#define DISPLAY_POWER 26
#else // Revision 7 and above defaults
#ifndef LEDC_BACKLIGHT
#define LEDC_BACKLIGHT 1
#endif
#ifndef ULP_BACKLIGHT
#define ULP_BACKLIGHT 0
#endif
#if GILPICO_SINDARIN
#define BACKLIGHT_RIGHT 26
#define BACKLIGHT_LEFT 9
#else
#define BACKLIGHT_RIGHT 2
#define BACKLIGHT_LEFT 17
#endif
#endif

// Boards prior to Sol3 boards don't have separate CS pins, but it's safe to use the same pin twice
// here
#if GILPICO_SINDARIN
#define DISPLAY_CS_LEFT 5
#define DISPLAY_CS_RIGHT 5
#define DISPLAY_OEI 21
#define DISPLAY_BUSY 33
#else
// Sol3
#define DISPLAY_CS_LEFT 21
#define DISPLAY_CS_RIGHT 11
#define SYS_ON 5
#endif
#ifndef DISPLAY_RST
#define DISPLAY_RST 9
#endif

#if GILPICO_SINDARIN
#define DISPLAY_SPI VSPI_HOST
#define DISPLAY_MOSI VSPI_IOMUX_PIN_NUM_MOSI
#define DISPLAY_MISO VSPI_IOMUX_PIN_NUM_MISO
#define DISPLAY_CLK VSPI_IOMUX_PIN_NUM_CLK
#else
#define DISPLAY_SPI SPI2_HOST
#define DISPLAY_MOSI 12
#define DISPLAY_MISO 14
#define DISPLAY_CLK 13
#endif

#define CPU_FREQ_MHZ_HIGH 240
#if !DISABLE_POWER_MANAGEMENT
#define CPU_FREQ_MHZ_LOW 40
#else
#define CPU_FREQ_MHZ_LOW 80
#endif

// HACK for Adafruit_GFX. Adafruit_GFX needs this value, but if we set it globally, then Arduino
// complains it's already defined.
#ifndef ARDUINO
#define ARDUINO 10185
#endif

#if (USE_LTR329 || USE_MMC5603)
#define USE_I2C 1
#else
#define USE_I2C 0
#endif

#if USE_I2C
#define I2C_MASTER_SCL_IO 0
#define I2C_MASTER_SDA_IO 1
#define I2C_MASTER_FREQ_HZ 400000
#endif

#if !GILPICO_SINDARIN
#define USB_CONNECTED_PIN 8
#endif
